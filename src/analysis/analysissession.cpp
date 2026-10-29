/// @file analysissession.cpp
/// @brief 検討・詰み探索のセッションの実装

#include "analysissession.h"

#include "mateoracle.h"

#include "appactivitystate.h"
#include "engineslots.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "usi.h"

AnalysisSession::AnalysisSession(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.engines && m_deps.activity);
    if (m_deps.navState)
        connect(m_deps.navState, &KifuNavigationState::changed, this, &AnalysisSession::followDisplayedPosition);
    connect(m_deps.engines, &EngineSlots::engineRevoked, this, &AnalysisSession::onEngineRevoked);
    connect(m_deps.activity, &AppActivityState::revoked, this, &AnalysisSession::onActivityRevoked);
}

Usi* AnalysisSession::engine() const
{
    return m_engine.data();
}

bool AnalysisSession::isShutdownInProgress() const
{
    return m_stopping;
}

// ============================================================
// 開始・停止
// ============================================================

bool AnalysisSession::start(const AnalysisOptions& opt)
{
    if (isShutdownInProgress() || opt.position.isEmpty()) return false;

    // 前のセッションは知らせずに終え、ほかの処理のエンジンも止めて（持ち主には知らせる）から始める
    endSession();
    m_deps.engines->destroyEngines();
    m_state = State::Searching;
    m_engineName = opt.engineName;
    m_mode = opt.mode == AnalysisOptions::Mode::Tsume ? Mode::Tsume : Mode::Consideration;
    m_activityLease = m_deps.activity->begin(m_mode == Mode::Tsume ? AppActivityState::Activity::DuringTsumeSearch
                                                                  : AppActivityState::Activity::DuringConsideration,
                                             this);

    // エンジンは E1 の枠に置く（対局の手番は持たない）
    m_lease = m_deps.engines->acquire(this, 1, opt.engineName);
    Usi* engine = m_lease.engine();
    m_engine = engine;
    connect(engine, &Usi::errorOccurred, this, &AnalysisSession::onEngineError);
    connect(engine, &Usi::searchTimedOut, this, &AnalysisSession::onSearchTimedOut);

    m_position = opt.position;
    m_byoyomiMs = opt.byoyomiMs;
    m_multiPV = opt.multiPV;
    const UsiProtocolHandler* protocol = engine->protocol();
    if (m_mode == Mode::Tsume) {
        connect(protocol, &UsiProtocolHandler::checkmateReceived, this, &AnalysisSession::onCheckmate);
        connect(protocol, &UsiProtocolHandler::bestMoveReceived, this, &AnalysisSession::onTsumeBestMoveReceived);
    } else {
        connect(protocol, &UsiProtocolHandler::bestMoveReceived,
                this, &AnalysisSession::onConsiderationBestMoveReceived);
    }

    // 起動できなければ、戻った後に onEngineError が届く（探索の要求は初期化の後で送られる）
    engine->startEngine(opt.enginePath, opt.engineName);
    emit engineNamesChanged(opt.engineName, QString());
    if (m_mode == Mode::Consideration) emit considerationResultsCleared();
    startSearch();
    return true;
}

void AnalysisSession::startSearch()
{
    if (!m_engine) return;
    if (m_mode == Mode::Tsume) {
        m_engine->startMateSearch(m_position, m_byoyomiMs);
        return;
    }
    m_engine->startConsideration(m_position, m_byoyomiMs, m_multiPV);
    emit considerationSearchStarted();
}

void AnalysisSession::stop()
{
    if (isShutdownInProgress()) return;
    // 自分のエンジンだけを止める（検討していなければ何もしない。ほかの処理のエンジンには触れない）
    m_stopping = true;
    const Mode previous = endSession();
    m_stopping = false;
    if (previous == Mode::None) return;
    // 自分の動作状態のときだけ待機中に戻す（ほかの部品が始めた動作は変えない）
    m_activityLease.reset();
    if (previous == Mode::Consideration) emit considerationModeEnded();
    if (previous == Mode::Tsume) emit tsumeSearchModeEnded();
}

void AnalysisSession::onEngineRevoked(QObject* owner)
{
    // ほかの処理がエンジンの枠を取った（棋譜解析・対局の開始など）。セッションを終えたことを知らせる
    if (owner == this) stop();
}

void AnalysisSession::onActivityRevoked(QObject* owner)
{
    if (owner == this) stop();
}

AnalysisSession::Mode AnalysisSession::endSession()
{
    const Mode previous = m_mode;
    m_mode = Mode::None;
    m_state = State::Idle;
    if (m_engine) {
        disconnect(m_engine, nullptr, this, nullptr);
        disconnect(m_engine->protocol(), nullptr, this, nullptr);
    }
    m_engine.clear();
    // 枠を空ける（quit を送る。思考情報・通信ログの表示は残す）
    m_lease.reset();
    return previous;
}

void AnalysisSession::onEngineError(const QString& message)
{
    const Mode previous = m_mode;
    if (previous == Mode::None) return;
    stop();
    emit messageRequested(previous == Mode::Tsume ? tr("詰み探索") : tr("検討"),
                          tr("エンジンエラー: %1").arg(message));
}

// ============================================================
// 検討のやり直し
// ============================================================

void AnalysisSession::updateMultiPV(int multiPV)
{
    multiPV = qBound(1, multiPV, 10);
    if (m_mode != Mode::Consideration || m_multiPV == multiPV) return;
    m_multiPV = multiPV;
    requestConsiderationRestart();
}

void AnalysisSession::followDisplayedPosition()
{
    if (m_mode != Mode::Consideration) return;
    const KifuBranchNode* node = m_deps.navState->currentNode();
    if (!node || node->sfen().isEmpty()) return;
    // 局面に至った手（読み筋の「同」の判定と、矢印の基準）
    const std::optional<UsiMove> lastMove =
        node->isActualMove() ? std::optional<UsiMove>(node->move().move) : std::nullopt;
    if (const auto position = UsiPosition::fromSfen(node->sfen(), lastMove)) updatePosition(*position);
}

bool AnalysisSession::updatePosition(const UsiPosition& position)
{
    if (m_mode != Mode::Consideration || position.isEmpty() || m_position == position) return false;
    m_position = position;
    requestConsiderationRestart();
    return true;
}

void AnalysisSession::requestConsiderationRestart()
{
    if (m_mode != Mode::Consideration) return;
    emit considerationResultsCleared();
    if (!m_engine) return;
    // 探索中なら、エンジンの側で止めて結果を捨ててから始める（初期化中なら最後の依頼だけを初期化の後で始める）
    m_state = State::Searching;
    startSearch();
}

void AnalysisSession::onConsiderationBestMoveReceived(const UsiBestMove& /*result*/)
{
    if (m_mode != Mode::Consideration || m_state != State::Searching) return;
    m_state = State::Waiting;
    emit considerationWaitingStarted();
}

// ============================================================
// 詰み探索の結果
// ============================================================

void AnalysisSession::finalizeTsumeSearch(const QString& resultMessage)
{
    if (m_mode != Mode::Tsume) return;
    stop();
    emit messageRequested(tr("詰み探索"), resultMessage);
}

void AnalysisSession::onCheckmate(const UsiCheckmate& result)
{
    const MateAnswer answer = MateAnswer::fromCheckmate(result);
    switch (answer.kind) {
    case MateAnswer::Kind::Mate:
        finalizeTsumeSearch(tr("詰みあり（手順 %1 手）").arg(answer.pv.size()));
        return;
    case MateAnswer::Kind::NoMate:
        finalizeTsumeSearch(tr("詰みなし"));
        return;
    case MateAnswer::Kind::Unsupported:
        finalizeTsumeSearch(tr("（エンジン側）未実装"));
        return;
    case MateAnswer::Kind::Unknown:
        finalizeTsumeSearch(tr("時間内に詰みを判定できませんでした"));
        return;
    }
}

void AnalysisSession::onSearchTimedOut()
{
    finalizeTsumeSearch(tr("エンジンが応答しなかったため、詰み探索を打ち切りました"));
}

void AnalysisSession::onTsumeBestMoveReceived(const UsiBestMove& /*result*/)
{
    finalizeTsumeSearch(tr("探索が完了しました"));
}
