/// @file analysissession.cpp
/// @brief 検討・詰み探索のセッションの実装（開始・停止・やり直しを1つの状態遷移として扱う）

#include "analysissession.h"

#include "appactivitystate.h"
#include "enginelifecyclemanager.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "usi.h"

AnalysisSession::AnalysisSession(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    Q_ASSERT(m_deps.engines && m_deps.activity);
    m_restartTimer.setSingleShot(true);
    connect(&m_restartTimer, &QTimer::timeout, this, &AnalysisSession::restartConsiderationDeferred);
    if (m_deps.navState)
        connect(m_deps.navState, &KifuNavigationState::changed, this, &AnalysisSession::followDisplayedPosition);
}

Usi* AnalysisSession::engine() const
{
    return m_engine.data();
}

bool AnalysisSession::isShutdownInProgress() const
{
    return m_deps.engines->isShutdownInProgress();
}

// ============================================================
// 開始・停止
// ============================================================

bool AnalysisSession::start(const AnalysisOptions& opt)
{
    if (m_state == State::Starting || m_state == State::CancelStarting || isShutdownInProgress()) return false;

    // 前のセッションは知らせずに終え、エンジンを破棄してから新しいセッションを始める
    endSession();
    m_deps.engines->destroyEngines();
    m_state = State::Starting;
    m_startupError.clear();
    m_engineName = opt.engineName;
    m_mode = opt.mode == AnalysisOptions::Mode::Tsume ? Mode::Tsume : Mode::Consideration;
    m_deps.activity->setActivity(m_mode == Mode::Tsume ? AppActivityState::Activity::DuringTsumeSearch
                                                      : AppActivityState::Activity::DuringConsideration);

    // エンジンは E1 の枠に置く（対局の手番は持たない）
    Usi* engine = m_deps.engines->createEngine(1, std::nullopt, opt.engineName);
    m_engine = engine;
    m_restartTimer.stop();
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

    const bool accepted = engine->startEngine(opt.enginePath, opt.engineName);
    const bool cancelled = m_state == State::CancelStarting;
    m_state = State::Searching;
    if (cancelled) {
        stop();
        return false;
    }
    if (!accepted || !m_startupError.isEmpty()) {
        onEngineError(m_startupError.isEmpty() ? opt.enginePath : m_startupError);
        return false;
    }
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
    if (m_state == State::Starting || m_state == State::CancelStarting) {
        m_state = State::CancelStarting;
        if (m_engine) m_engine->cancelCurrentOperation();
        return;
    }
    if (isShutdownInProgress()) return;
    m_deps.engines->setShutdownInProgress(true);
    const Mode previous = endSession();
    m_deps.engines->destroyEngines(false);
    m_deps.engines->setShutdownInProgress(false);
    m_deps.activity->setActivity(AppActivityState::Activity::Idle);
    if (previous == Mode::Consideration) emit considerationModeEnded();
    if (previous == Mode::Tsume) emit tsumeSearchModeEnded();
}

AnalysisSession::Mode AnalysisSession::endSession()
{
    const Mode previous = m_mode;
    m_restartTimer.stop();
    m_mode = Mode::None;
    m_state = State::Idle;
    if (m_engine) {
        disconnect(m_engine, nullptr, this, nullptr);
        disconnect(m_engine->protocol(), nullptr, this, nullptr);
    }
    m_engine.clear();
    return previous;
}

void AnalysisSession::onEngineError(const QString& message)
{
    if (m_state == State::Starting || m_state == State::CancelStarting) {
        m_startupError = message;
        return;
    }
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
    if (m_mode != Mode::Consideration || m_position == position) return false;
    m_position = position;
    requestConsiderationRestart();
    return true;
}

void AnalysisSession::requestConsiderationRestart()
{
    if (m_mode == Mode::Consideration) emit considerationResultsCleared();
    if (!m_engine || m_mode != Mode::Consideration) return;
    switch (m_state) {
    case State::Searching:
        if (!m_engine->isInitializing()) {
            // stop の応答を受けるまで次の go を送らない。
            m_state = State::StoppingSearch;
            m_engine->stop();
            return;
        }
        m_engine->stop(); // 初期化待ちの古い探索要求を取り消す
        [[fallthrough]];
    case State::Waiting:
        m_state = State::RestartQueued;
        m_restartTimer.start(0);
        break;
    default: break; // 初期化中・再開待ちには最新の設定だけを保存する
    }
}

void AnalysisSession::onConsiderationBestMoveReceived(const UsiBestMove& /*result*/)
{
    if (m_mode != Mode::Consideration) return;
    if (m_state == State::StoppingSearch) {
        m_state = State::RestartQueued;
        m_restartTimer.start(0);
    } else if (m_state == State::Searching) {
        m_state = State::Waiting;
        emit considerationWaitingStarted();
    }
}

void AnalysisSession::restartConsiderationDeferred()
{
    if (m_mode != Mode::Consideration || m_state != State::RestartQueued || !m_engine) return;
    emit considerationResultsCleared();
    m_state = State::Searching;
    startSearch();
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
    switch (result.result) {
    case UsiCheckmate::Result::Mate:
        finalizeTsumeSearch(tr("詰みあり（手順 %1 手）").arg(result.pv.size()));
        return;
    case UsiCheckmate::Result::NoMate:
        finalizeTsumeSearch(tr("詰みなし"));
        return;
    case UsiCheckmate::Result::NotImplemented:
        finalizeTsumeSearch(tr("（エンジン側）未実装"));
        return;
    case UsiCheckmate::Result::Unknown:
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
