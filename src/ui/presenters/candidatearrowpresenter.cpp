#include "candidatearrowpresenter.h"

#include "analysissession.h"
#include "analysissettings.h"
#include "appactivitystate.h"
#include "enginelifecyclemanager.h"
#include "kifupresentation.h"
#include "matchcoordinator.h"
#include "sfenreplay.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogienginethinkingmodel.h"
#include "usi.h"

namespace {
QString positionKey(const QString& sfen)
{
    // 手数は描画する盤面の同一性に含めない。
    return SfenUtils::normalizeSfenKey(sfen);
}

}

CandidateArrowPresenter::CandidateArrowPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_view(deps.view)
    , m_sources(deps)
{
    Q_ASSERT(deps.view);
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &CandidateArrowPresenter::refresh);
    connect(deps.view, &ShogiView::positionChanged, this, &CandidateArrowPresenter::scheduleRefresh);
    // 終局すると動作状態が待機に戻るので、終局もこの通知で分かる
    if (deps.activity) connect(deps.activity, &AppActivityState::changed, this, &CandidateArrowPresenter::scheduleRefresh);
    if (deps.analysis) {
        // 検討の開始・終了で、検討の読み筋と対局のエンジンの読みのどちらを描くかが変わる
        connect(deps.analysis, &AnalysisSession::considerationSearchStarted,
                this, &CandidateArrowPresenter::scheduleRefresh);
        connect(deps.analysis, &AnalysisSession::considerationModeEnded,
                this, &CandidateArrowPresenter::scheduleRefresh);
    }
    if (ShogiEngineThinkingModel* model = deps.consideration) {
        connect(model, &QAbstractItemModel::rowsInserted, this, &CandidateArrowPresenter::scheduleRefresh);
        connect(model, &QAbstractItemModel::rowsRemoved, this, &CandidateArrowPresenter::scheduleRefresh);
        connect(model, &QAbstractItemModel::dataChanged, this, &CandidateArrowPresenter::scheduleRefresh);
        connect(model, &QAbstractItemModel::modelReset, this, &CandidateArrowPresenter::scheduleRefresh);
    }
    if (deps.engines) {
        connect(deps.engines, &EngineLifecycleManager::enginesChanged, this, &CandidateArrowPresenter::bindEngines);
        bindEngines();
    }
}

void CandidateArrowPresenter::bindEngines()
{
    for (const auto& engine : m_engines)
        if (engine) disconnect(engine, nullptr, this, nullptr);
    m_engines = {m_sources.engines ? m_sources.engines->engineInSlot(1) : nullptr,
                 m_sources.engines ? m_sources.engines->engineInSlot(2) : nullptr};
    for (const auto& engine : m_engines) {
        if (!engine) continue;
        connect(engine, &Usi::searchCandidateChanged, this, &CandidateArrowPresenter::scheduleRefresh);
        connect(engine, &QObject::destroyed, this, &CandidateArrowPresenter::scheduleRefresh);
    }
    scheduleRefresh();
}

bool CandidateArrowPresenter::considerationActive() const
{
    // 中止した後は、残っている読み筋から矢印を作り直さない
    return m_sources.analysis && m_sources.analysis->isConsidering();
}

void CandidateArrowPresenter::scheduleRefresh()
{
    // 1回の受信や盤面更新に伴う複数の通知をまとめ、更新完了後の状態を描画する。
    if (!m_refreshTimer.isActive()) m_refreshTimer.start(0);
}

std::optional<ShogiView::Arrow> CandidateArrowPresenter::arrowForMove(
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
        arrow.dropPiece = parsed->dropPieceFor(turn == QLatin1String("b") ? Turn::Black : Turn::White);
    } else {
        arrow.fromFile = parsed->from.file;
        arrow.fromRank = parsed->from.rank;
    }
    arrow.color = priority <= 1 ? QColor(255, 0, 0, 200) : QColor(255, 100, 100, 150);
    return arrow;
}

void CandidateArrowPresenter::appendMatchArrows(QList<ShogiView::Arrow>& arrows, const QString& sfen)
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

void CandidateArrowPresenter::refresh()
{
    m_refreshTimer.stop();
    if (!m_view) return;
    QList<ShogiView::Arrow> arrows;
    m_ponderDescription.clear();
    auto* board = m_view->board();
    if (board) {
        const QString sfen = board->position().toSfen(board->currentPlayer(), 1);
        if (considerationActive()) {
            const bool show = !m_sources.showConsiderationArrows || m_sources.showConsiderationArrows();
            if (ShogiEngineThinkingModel* model = m_sources.consideration; show && model) {
                for (int row = 0; row < model->rowCount(); ++row) {
                    const auto* record = model->recordAt(row);
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
