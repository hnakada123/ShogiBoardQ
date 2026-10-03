#include "candidatearrowcontroller.h"

#include "analysissettings.h"
#include "enginelifecyclemanager.h"
#include "kifupresentation.h"
#include "matchcoordinator.h"
#include "sfenpositiontracer.h"
#include "shogiboard.h"
#include "shogienginethinkingmodel.h"
#include "usi.h"
#include <QRegularExpression>

namespace {
QString positionKey(const QString& sfen)
{
    // 手数は描画する盤面の同一性に含めない。
    return sfen.simplified().section(QLatin1Char(' '), 0, 2);
}

bool localEngineMatch(PlayMode mode)
{
    switch (mode) {
    case PlayMode::EvenHumanVsEngine:
    case PlayMode::EvenEngineVsHuman:
    case PlayMode::EvenEngineVsEngine:
    case PlayMode::HandicapHumanVsEngine:
    case PlayMode::HandicapEngineVsHuman:
    case PlayMode::HandicapEngineVsEngine:
        return true;
    default:
        return false;
    }
}
}

CandidateArrowController* CandidateArrowController::forView(ShogiView* view)
{
    if (!view) return nullptr;
    auto* controller = view->findChild<CandidateArrowController*>(QString(), Qt::FindDirectChildrenOnly);
    return controller ? controller : new CandidateArrowController(view);
}

CandidateArrowController::CandidateArrowController(ShogiView* view)
    : QObject(view), m_view(view)
{
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &CandidateArrowController::refresh);
    connect(view, &ShogiView::positionChanged, this, &CandidateArrowController::scheduleRefresh);
}

void CandidateArrowController::setMatchCoordinator(MatchCoordinator* match)
{
    if (m_match == match) return;
    if (m_match) {
        disconnect(m_match, nullptr, this, nullptr);
        disconnect(m_match->engineManager(), nullptr, this, nullptr);
    }
    m_match = match;
    if (match) {
        connect(match, &MatchCoordinator::playModeChanged, this, &CandidateArrowController::scheduleRefresh);
        connect(match, &MatchCoordinator::gameOverStateChanged, this, &CandidateArrowController::scheduleRefresh);
        connect(match, &MatchCoordinator::considerationModeEnded, this, &CandidateArrowController::endConsideration);
        connect(match, &QObject::destroyed, this, &CandidateArrowController::scheduleRefresh);
        connect(match->engineManager(), &EngineLifecycleManager::enginesChanged,
                this, &CandidateArrowController::bindEngines);
    }
    bindEngines();
}

void CandidateArrowController::bindEngines()
{
    for (const auto& engine : m_engines)
        if (engine) disconnect(engine, nullptr, this, nullptr);
    m_engines = {m_match ? m_match->primaryEngine() : nullptr,
                 m_match ? m_match->secondaryEngine() : nullptr};
    for (const auto& engine : m_engines) {
        if (!engine) continue;
        connect(engine, &Usi::searchCandidateChanged, this, &CandidateArrowController::scheduleRefresh,
                Qt::UniqueConnection);
        connect(engine, &QObject::destroyed, this, &CandidateArrowController::scheduleRefresh,
                Qt::UniqueConnection);
    }
    scheduleRefresh();
}

void CandidateArrowController::setConsiderationState(bool active, bool show, ShogiEngineThinkingModel* model)
{
    m_considerationActive = active;
    m_showConsideration = show;
    if (m_considerationModel != model) {
        if (m_considerationModel) disconnect(m_considerationModel, nullptr, this, nullptr);
        m_considerationModel = model;
        if (model) {
            connect(model, &QAbstractItemModel::rowsInserted, this, &CandidateArrowController::scheduleRefresh);
            connect(model, &QAbstractItemModel::rowsRemoved, this, &CandidateArrowController::scheduleRefresh);
            connect(model, &QAbstractItemModel::dataChanged, this, &CandidateArrowController::scheduleRefresh);
            connect(model, &QAbstractItemModel::modelReset, this, &CandidateArrowController::scheduleRefresh);
        }
    }
    scheduleRefresh();
}

void CandidateArrowController::endConsideration()
{
    m_considerationActive = false;
    scheduleRefresh();
}

bool CandidateArrowController::considerationActive() const
{
    return m_considerationActive && (!m_match || m_match->playMode() == PlayMode::ConsiderationMode);
}

void CandidateArrowController::scheduleRefresh()
{
    // 1回の受信や盤面更新に伴う複数の通知をまとめ、更新完了後の状態を描画する。
    if (!m_refreshTimer.isActive()) m_refreshTimer.start(0);
}

std::optional<ShogiView::Arrow> CandidateArrowController::arrowForMove(
    const QString& move, const QString& baseSfen, int priority)
{
    static const QRegularExpression valid(QStringLiteral("^(?:[1-9][a-i][1-9][a-i]\\+?|[PLNSGBR]\\*[1-9][a-i])$"));
    if (!valid.match(move).hasMatch()) return std::nullopt;
    ShogiView::Arrow arrow;
    arrow.toFile = move.at(2).digitValue();
    arrow.toRank = move.at(3).unicode() - QChar('a').unicode() + 1;
    arrow.priority = priority;
    if (move.at(1) == QLatin1Char('*')) {
        const auto turn = baseSfen.simplified().section(QLatin1Char(' '), 1, 1);
        if (turn != QLatin1String("b") && turn != QLatin1String("w")) return std::nullopt;
        arrow.dropPiece = turn == QLatin1String("b") ? move.at(0) : move.at(0).toLower();
    } else {
        arrow.fromFile = move.at(0).digitValue();
        arrow.fromRank = move.at(1).unicode() - QChar('a').unicode() + 1;
    }
    arrow.color = priority <= 1 ? QColor(255, 0, 0, 200) : QColor(255, 100, 100, 150);
    return arrow;
}

void CandidateArrowController::appendMatchArrows(QList<ShogiView::Arrow>& arrows, const QString& sfen)
{
    if (!m_match || !localEngineMatch(m_match->playMode()) || m_match->gameOverState().isOver
        || !AnalysisSettings::matchArrowsVisible()) return;

    const Usi* previous = nullptr;
    for (const auto& engine : m_engines) {
        // HvEでは両スロットが同じエンジンを指す場合がある。
        if (!engine || engine == previous) continue;
        previous = engine;
        const auto& candidate = engine->searchCandidate();
        if (!candidate.active || candidate.move.isEmpty()) continue;
        if (candidate.pondering) {
            if (!AnalysisSettings::ponderArrowsVisible() || candidate.predictedMove.isEmpty()) continue;
            SfenPositionTracer predicted;
            if (!predicted.setFromSfen(sfen) || !predicted.applyUsiMove(candidate.predictedMove)
                || positionKey(predicted.toSfenString()) != positionKey(candidate.baseSfen)) continue;
        } else if (positionKey(candidate.baseSfen) != positionKey(sfen)) {
            continue;
        }
        auto arrow = arrowForMove(candidate.move, candidate.baseSfen, 0);
        if (!arrow) continue;
        if (candidate.pondering) {
            arrow->color = QColor(30, 110, 220, 200);
            arrow->penStyle = Qt::DashLine;
            m_ponderDescription = tr("先読み：%1を想定").arg(
                KifuPresentation::move(sfen, candidate.predictedMove, KifuPresentation::options()));
        }
        arrows.append(*arrow);
    }
}

void CandidateArrowController::refresh()
{
    m_refreshTimer.stop();
    if (!m_view) return;
    QList<ShogiView::Arrow> arrows;
    m_ponderDescription.clear();
    auto* board = m_view->board();
    if (board) {
        const QString sfen = board->convertBoardToSfen() + QLatin1Char(' ')
            + turnToSfen(board->currentPlayer()) + QLatin1Char(' ') + board->convertStandToSfen()
            + QStringLiteral(" 1");
        if (considerationActive()) {
            if (m_showConsideration && m_considerationModel) {
                for (int row = 0; row < m_considerationModel->rowCount(); ++row) {
                    const auto* record = m_considerationModel->recordAt(row);
                    if (!record || positionKey(record->baseSfen()) != positionKey(sfen)) continue;
                    const auto move = record->usiPv().simplified().section(QLatin1Char(' '), 0, 0);
                    if (auto arrow = arrowForMove(move, record->baseSfen(), record->multipv()))
                        arrows.append(*arrow);
                }
            }
        } else {
            appendMatchArrows(arrows, sfen);
        }
    }
    m_view->setArrows(arrows);
    emit displayStateChanged();
}
