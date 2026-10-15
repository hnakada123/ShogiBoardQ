#include "candidatearrowcontroller.h"

#include "analysissession.h"
#include "analysissettings.h"
#include "appactivitystate.h"
#include "enginelifecyclemanager.h"
#include "kifupresentation.h"
#include "matchcoordinator.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include "shogienginethinkingmodel.h"
#include "usi.h"

namespace {
QString positionKey(const QString& sfen)
{
    // 手数は描画する盤面の同一性に含めない。
    return sfen.simplified().section(QLatin1Char(' '), 0, 2);
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

void CandidateArrowController::setSources(const Sources& sources)
{
    // 部品は起動時に一度だけ作るので、つなぎ直しは要らない
    Q_ASSERT(!m_sources.match && sources.match && sources.engines && sources.activity && sources.analysis);
    m_sources = sources;
    connect(sources.activity, &AppActivityState::changed, this, &CandidateArrowController::scheduleRefresh);
    connect(sources.match, &MatchCoordinator::gameOverStateChanged, this, &CandidateArrowController::scheduleRefresh);
    connect(sources.analysis, &AnalysisSession::considerationModeEnded,
            this, &CandidateArrowController::endConsideration);
    connect(sources.engines, &EngineLifecycleManager::enginesChanged, this, &CandidateArrowController::bindEngines);
    bindEngines();
}

void CandidateArrowController::bindEngines()
{
    for (const auto& engine : m_engines)
        if (engine) disconnect(engine, nullptr, this, nullptr);
    m_engines = {m_sources.engines ? m_sources.engines->engineInSlot(1) : nullptr,
                 m_sources.engines ? m_sources.engines->engineInSlot(2) : nullptr};
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
    return m_considerationActive && (!m_sources.analysis || m_sources.analysis->isConsidering());
}

void CandidateArrowController::scheduleRefresh()
{
    // 1回の受信や盤面更新に伴う複数の通知をまとめ、更新完了後の状態を描画する。
    if (!m_refreshTimer.isActive()) m_refreshTimer.start(0);
}

std::optional<ShogiView::Arrow> CandidateArrowController::arrowForMove(
    const QString& move, const QString& baseSfen, int priority)
{
    const auto parsed = UsiMove::parse(move);
    if (!parsed || move.trimmed() != parsed->toString()) return std::nullopt;
    ShogiView::Arrow arrow;
    arrow.toFile = parsed->to.file;
    arrow.toRank = parsed->to.rank;
    arrow.priority = priority;
    if (parsed->isDrop()) {
        const auto turn = baseSfen.simplified().section(QLatin1Char(' '), 1, 1);
        if (turn != QLatin1String("b") && turn != QLatin1String("w")) return std::nullopt;
        arrow.dropPiece = pieceToChar(parsed->dropPieceFor(turn == QLatin1String("b") ? Turn::Black : Turn::White));
    } else {
        arrow.fromFile = parsed->from.file;
        arrow.fromRank = parsed->from.rank;
    }
    arrow.color = priority <= 1 ? QColor(255, 0, 0, 200) : QColor(255, 100, 100, 150);
    return arrow;
}

void CandidateArrowController::appendMatchArrows(QList<ShogiView::Arrow>& arrows, const QString& sfen)
{
    // エンジンが指す対局の最中だけ描く（通信対局・検討・解析のエンジンは対象外）
    if (!m_sources.match || !m_sources.activity
        || m_sources.activity->activity() != AppActivityState::Activity::DuringGame
        || m_sources.match->isGameOver() || !AnalysisSettings::matchArrowsVisible()) return;

    for (const auto& engine : m_engines) {
        if (!engine) continue;
        const auto& candidate = engine->searchCandidate();
        if (!candidate.active || candidate.move.isEmpty()) continue;
        if (candidate.pondering) {
            if (!AnalysisSettings::ponderArrowsVisible() || candidate.predictedMove.isEmpty()) continue;
            const QString predicted = SfenReplay::positionAfter(sfen, candidate.predictedMove);
            if (predicted.isEmpty() || positionKey(predicted) != positionKey(candidate.baseSfen)) continue;
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
    emit displayStateChanged(considerationActive(), m_ponderDescription);
}
