#include "candidatearrowpresenter.h"

#include "analysissession.h"
#include "appactivitystate.h"
#include "boardarrowactions.h"
#include "engineslots.h"
#include "humanturnpolicy.h"
#include "josekibooksession.h"
#include "kifupresentation.h"
#include "matchcoordinator.h"
#include "sfenreplay.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogienginethinkingmodel.h"
#include "usi.h"

#include <QAction>

namespace {
QString positionKey(const QString& sfen)
{
    // 手数は描画する盤面の同一性に含めない。
    return SfenUtils::normalizeSfenKey(sfen);
}

/// 番号を付ける定跡手の矢印の数（表の上から。多いと番号で盤が隠れる）
constexpr int kNumberedJosekiArrows = 5;

/// 思考の表の最新の最善の読み筋（multipv 1 で、最初の手を指せた行。表は新しい行が先頭）
const ShogiInfoRecord* latestBestRecord(const ShogiEngineThinkingModel& thinking)
{
    for (int row = 0; row < thinking.rowCount(); ++row) {
        const ShogiInfoRecord* record = thinking.recordAt(row);
        if (record && record->multipv() == 1 && record->pv().firstMove()) return record;
    }
    return nullptr;
}

/// 表の行が変わったら描き直す
void connectRows(const QAbstractItemModel* model, CandidateArrowPresenter* presenter)
{
    QObject::connect(model, &QAbstractItemModel::rowsInserted, presenter, &CandidateArrowPresenter::scheduleRefresh);
    QObject::connect(model, &QAbstractItemModel::rowsRemoved, presenter, &CandidateArrowPresenter::scheduleRefresh);
    QObject::connect(model, &QAbstractItemModel::dataChanged, presenter, &CandidateArrowPresenter::scheduleRefresh);
    QObject::connect(model, &QAbstractItemModel::modelReset, presenter, &CandidateArrowPresenter::scheduleRefresh);
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
    // 検討の表と、対局のエンジンの思考の表
    const std::array<const ShogiEngineThinkingModel*, 3> models{deps.consideration, deps.thinking[0], deps.thinking[1]};
    for (const ShogiEngineThinkingModel* model : models)
        if (model) connectRows(model, this);
    if (deps.engines) {
        connect(deps.engines, &EngineSlots::enginesChanged, this, &CandidateArrowPresenter::bindEngines);
        bindEngines();
    }
    if (deps.josekiBook)
        connect(deps.josekiBook, &JosekiBookSession::contentChanged, this, &CandidateArrowPresenter::scheduleRefresh);
    if (const BoardArrowActions* toggles = deps.arrows) {
        for (const QAction* action : {toggles->match(), toggles->ponder(), toggles->consideration(), toggles->joseki()})
            connect(action, &QAction::toggled, this, &CandidateArrowPresenter::scheduleRefresh);
    }
    if (deps.josekiDisplay)
        connect(deps.josekiDisplay, &QAction::toggled, this, &CandidateArrowPresenter::scheduleRefresh);
}

bool CandidateArrowPresenter::arrowsOn(QAction* (BoardArrowActions::*toggle)() const, bool fallback) const
{
    return m_sources.arrows ? (m_sources.arrows->*toggle)()->isChecked() : fallback;
}

void CandidateArrowPresenter::appendJosekiArrows(QList<ShogiViewHighlighting::Arrow>& arrows, const QString& sfen) const
{
    if (!m_sources.josekiBook || !arrowsOn(&BoardArrowActions::joseki, false)) return;
    if (m_sources.josekiDisplay && !m_sources.josekiDisplay->isChecked()) return;
    // 対局中は人間の手番だけ（エンジンの手番に定跡手を見せても使えない）
    const HumanTurnPolicy* turn = m_sources.humanTurn;
    if (turn && turn->isGameActivelyInProgress() && !turn->isHumanTurnNow()) return;
    const QList<JosekiMove> moves = m_sources.josekiBook->movesAt(sfen);
    int maxFrequency = 0;
    for (const JosekiMove& move : moves) maxFrequency = qMax(maxFrequency, move.frequency);
    for (qsizetype i = 0; i < moves.size(); ++i) {
        // 番号は定跡ウィンドウの表の順（上の手だけに付ける）
        const int priority = i < kNumberedJosekiArrows ? static_cast<int>(i) + 1 : 0;
        auto arrow = arrowForMove(moves.at(i).move, sfen, priority);
        if (!arrow) continue;
        // 出現頻度の高い手ほど濃くする（出現頻度が無ければ同じ濃さ）
        const double share = maxFrequency > 0 ? qMax(moves.at(i).frequency, 0) / static_cast<double>(maxFrequency) : 1.0;
        arrow->color = QColor(0, 150, 70, 70 + qRound(160 * share));
        arrows.append(*arrow);
    }
}

void CandidateArrowPresenter::bindEngines()
{
    for (const auto& engine : m_engines) {
        if (!engine) continue;
        disconnect(engine, nullptr, this, nullptr);
        disconnect(engine->protocol(), nullptr, this, nullptr);
    }
    m_engines = {m_sources.engines ? m_sources.engines->engineInSlot(1) : nullptr,
                 m_sources.engines ? m_sources.engines->engineInSlot(2) : nullptr};
    for (const auto& engine : m_engines) {
        if (!engine) continue;
        // 探索を終えたら、思考の表に残った行からは描かない
        connect(engine->protocol(), &UsiProtocolHandler::searchEnded, this, &CandidateArrowPresenter::scheduleRefresh);
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

std::optional<ShogiViewHighlighting::Arrow> CandidateArrowPresenter::arrowForMove(
    const QString& move, const QString& baseSfen, int priority)
{
    const auto parsed = UsiMove::parse(move);
    if (!parsed || !UsiMove::isExact(move.trimmed())) return std::nullopt;
    ShogiViewHighlighting::Arrow arrow;
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

void CandidateArrowPresenter::appendConsiderationArrows(QList<ShogiViewHighlighting::Arrow>& arrows, const QString& sfen) const
{
    const ShogiEngineThinkingModel* model = m_sources.consideration;
    if (!model || !arrowsOn(&BoardArrowActions::consideration, true)) return;
    for (int row = 0; row < model->rowCount(); ++row) {
        const auto* record = model->recordAt(row);
        if (!record || positionKey(record->pv().baseSfen()) != positionKey(sfen)) continue;
        const std::optional<UsiMove> move = record->pv().firstMove();
        if (!move) continue;
        if (auto arrow = arrowForMove(move->toString(), record->pv().baseSfen(), record->multipv()))
            arrows.append(*arrow);
    }
}

void CandidateArrowPresenter::appendMatchArrows(QList<ShogiViewHighlighting::Arrow>& arrows, const QString& sfen)
{
    // エンジンが指す対局の最中だけ描く（通信対局・検討・解析のエンジンは対象外）
    if (!m_sources.activity || m_sources.activity->activity() != AppActivityState::Activity::DuringGame
        || (m_sources.match && m_sources.match->isGameOver()) || !arrowsOn(&BoardArrowActions::match, false))
        return;
    const bool ponderVisible = arrowsOn(&BoardArrowActions::ponder, false);

    for (size_t i = 0; i < m_engines.size(); ++i) {
        const Usi* engine = m_engines[i];
        const ShogiEngineThinkingModel* thinking = m_sources.thinking[i];
        // 探索を終えた後に残った行（待ったで戻した局面の前の探索など）からは描かない
        if (!engine || !thinking || !engine->protocol()->isSearching()) continue;
        const ShogiInfoRecord* record = latestBestRecord(*thinking);
        if (!record) continue;
        const QString& baseSfen = record->pv().baseSfen();
        const bool pondering = positionKey(baseSfen) != positionKey(sfen);
        QString predicted;
        if (pondering) {
            // 先読み: 表示中の局面に予想した手（探索の局面に至った手）を指した局面を探索している
            if (!ponderVisible || engine->protocol()->searchKind() != UsiProtocolHandler::SearchKind::Ponder) continue;
            predicted = record->lastUsiMove();
            const QString predictedSfen = predicted.isEmpty() ? QString() : SfenReplay::positionAfter(sfen, predicted);
            if (predictedSfen.isEmpty() || positionKey(predictedSfen) != positionKey(baseSfen)) continue;
        }
        auto arrow = arrowForMove(record->pv().firstMove()->toString(), baseSfen, 0);
        if (!arrow) continue;
        if (pondering) {
            arrow->color = QColor(30, 110, 220, 200);
            arrow->penStyle = Qt::DashLine;
            m_ponderDescription = tr("先読み：%1を想定").arg(
                KifuPresentation::move(sfen, predicted, KifuPresentation::options()));
        }
        arrows.append(*arrow);
    }
}

void CandidateArrowPresenter::refresh()
{
    m_refreshTimer.stop();
    if (!m_view) return;
    QList<ShogiViewHighlighting::Arrow> arrows;
    m_ponderDescription.clear();
    auto* board = m_view->board();
    if (board) {
        // 定跡手の矢印はエンジンの矢印の下に描く（先に足す）。定跡は手数違いの登録を区別するので手数付きで引く
        appendJosekiArrows(arrows, board->position().toSfen(board->currentPlayer(), board->position().moveNumber()));
        const QString sfen = board->position().toSfen(board->currentPlayer(), 1);
        if (considerationActive()) appendConsiderationArrows(arrows, sfen);
        else appendMatchArrows(arrows, sfen);
    }
    m_view->highlighting()->setArrows(arrows);
    emit displayStateChanged(considerationActive(), m_ponderDescription);
}
