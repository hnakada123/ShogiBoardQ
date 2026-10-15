/// @file analysissessionhandler.cpp
/// @brief 検討・詰み探索の開始、停止、再探索を一つの状態遷移として扱う。
#include "analysissessionhandler.h"
#include "usi.h"
#include "shogienginethinkingmodel.h"
#include <QSignalBlocker>

AnalysisSessionHandler::AnalysisSessionHandler(QObject* parent) : QObject(parent)
{
    m_restartTimer.setSingleShot(true);
    connect(&m_restartTimer, &QTimer::timeout, this, &AnalysisSessionHandler::restartConsiderationDeferred);
}

void AnalysisSessionHandler::setHooks(const Hooks& hooks) { m_hooks = hooks; }

bool AnalysisSessionHandler::startFullAnalysis(const MatchCoordinator::AnalysisOptions& opt)
{
    if (m_state == State::Starting || m_state == State::CancelStarting
        || (m_hooks.isShutdownInProgress && m_hooks.isShutdownInProgress())) return false;

    // 旧セッションの終了通知を済ませてから新しいセッションを開始する。
    if (m_hooks.destroyEnginesAll) m_hooks.destroyEnginesAll();
    m_state = State::Starting;
    m_startupError.clear();
    if (m_hooks.setPlayMode) m_hooks.setPlayMode(opt.mode);
    Usi* engine = m_hooks.createAnalysisEngine ? m_hooks.createAnalysisEngine(opt) : nullptr;
    if (!engine) {
        handleStop();
        if (m_hooks.setPlayMode) m_hooks.setPlayMode(PlayMode::NotStarted);
        return false;
    }
    setupModeSpecificWiring(engine, opt);
    const bool accepted = m_hooks.initAndStartEngine
        && m_hooks.initAndStartEngine(1, opt.enginePath, opt.engineName);
    const bool cancelled = m_state == State::CancelStarting;
    m_state = State::Searching;
    if (cancelled) {
        stopFullAnalysis();
        return false;
    }
    if (!accepted || !m_startupError.isEmpty()) {
        handleEngineError(m_startupError.isEmpty() ? opt.enginePath : m_startupError);
        return false;
    }
    if (m_hooks.setEngineNames) m_hooks.setEngineNames(opt.engineName, QString());
    auto current = opt;
    if (m_mode == Mode::Consideration) {
        current.positionStr = m_positionStr;
        current.multiPV = m_multiPV;
        engine->setPreviousMove(BoardSquare{m_previousFileTo, m_previousRankTo});
        engine->setLastUsiMove(m_lastUsiMove);
    } else {
        engine->setLastUsiMove(opt.lastUsiMove);
    }
    startCommunication(engine, current);
    return true;
}

void AnalysisSessionHandler::stopFullAnalysis()
{
    if (m_state == State::Starting || m_state == State::CancelStarting) {
        m_state = State::CancelStarting;
        if (m_engine) m_engine->cancelCurrentOperation();
        return;
    }
    if (m_hooks.isShutdownInProgress && m_hooks.isShutdownInProgress()) return;
    if (m_hooks.setShutdownInProgress) m_hooks.setShutdownInProgress(true);
    const Mode previous = m_mode;
    {
        const QSignalBlocker blocker(this);
        handleStop();
    }
    if (m_hooks.destroyEnginesKeepModels) m_hooks.destroyEnginesKeepModels();
    if (m_hooks.setShutdownInProgress) m_hooks.setShutdownInProgress(false);
    if (m_hooks.setPlayMode) m_hooks.setPlayMode(PlayMode::NotStarted);
    if (previous == Mode::Consideration) emit considerationModeEnded();
    if (previous == Mode::Tsume) emit tsumeSearchModeEnded();
}

void AnalysisSessionHandler::setupModeSpecificWiring(Usi* engine,
                                                     const MatchCoordinator::AnalysisOptions& opt)
{
    m_restartTimer.stop();
    m_engine = engine;
    m_mode = opt.mode == PlayMode::TsumiSearchMode ? Mode::Tsume
        : opt.mode == PlayMode::ConsiderationMode ? Mode::Consideration : Mode::Analysis;
    if (m_state != State::Starting) m_state = State::Searching;
    if (!engine) return;
    if (m_mode == Mode::Tsume) {
        connect(engine, &Usi::checkmateSolved, this, &AnalysisSessionHandler::onCheckmateSolved, Qt::UniqueConnection);
        connect(engine, &Usi::checkmateNoMate, this, &AnalysisSessionHandler::onCheckmateNoMate, Qt::UniqueConnection);
        connect(engine, &Usi::checkmateNotImplemented, this, &AnalysisSessionHandler::onCheckmateNotImplemented, Qt::UniqueConnection);
        connect(engine, &Usi::checkmateUnknown, this, &AnalysisSessionHandler::onCheckmateUnknown, Qt::UniqueConnection);
        connect(engine, &Usi::bestMoveReceived, this, &AnalysisSessionHandler::onTsumeBestMoveReceived, Qt::UniqueConnection);
    }
    engine->setPreviousMove(BoardSquare{opt.previousFileTo, opt.previousRankTo});
    if (m_mode == Mode::Consideration) {
        if (opt.considerationModel) engine->setConsiderationModel(opt.considerationModel, opt.multiPV);
        m_positionStr = opt.positionStr;
        m_byoyomiMs = opt.byoyomiMs;
        m_multiPV = opt.multiPV;
        m_modelPtr = opt.considerationModel;
        m_previousFileTo = opt.previousFileTo;
        m_previousRankTo = opt.previousRankTo;
        m_lastUsiMove = opt.lastUsiMove;
        connect(engine, &Usi::bestMoveReceived, this, &AnalysisSessionHandler::onConsiderationBestMoveReceived, Qt::UniqueConnection);
    }
}

void AnalysisSessionHandler::startCommunication(Usi* engine, const MatchCoordinator::AnalysisOptions& opt)
{
    if (!engine) return;
    QString position = opt.positionStr;
    if (opt.mode == PlayMode::TsumiSearchMode) engine->executeTsumeCommunication(position, opt.byoyomiMs);
    else engine->sendAnalysisCommands(position, opt.byoyomiMs, opt.multiPV);
}

void AnalysisSessionHandler::handleStop()
{
    const Mode previous = m_mode;
    m_restartTimer.stop();
    m_mode = Mode::None;
    m_state = State::Idle;
    m_engine.clear();
    m_modelPtr = nullptr;
    if (previous == Mode::Consideration) emit considerationModeEnded();
    if (previous == Mode::Tsume) emit tsumeSearchModeEnded();
}

void AnalysisSessionHandler::updateMultiPV(Usi* engine, int multiPV)
{
    multiPV = qBound(1, multiPV, 10);
    if (m_mode != Mode::Consideration || m_multiPV == multiPV) return;
    m_multiPV = multiPV;
    requestConsiderationRestart(engine);
}

void AnalysisSessionHandler::requestConsiderationRestart(Usi* engine)
{
    if (m_modelPtr) m_modelPtr->clearAllItems();
    if (!engine || engine != m_engine || m_mode != Mode::Consideration) return;
    switch (m_state) {
    case State::Searching:
        if (!engine->isInitializing()) {
            // stop の応答を受けるまで次の go を送らない。
            m_state = State::StoppingSearch;
            engine->sendStopCommand();
            return;
        }
        engine->sendStopCommand(); // 初期化待ちの古い探索要求を取り消す
        [[fallthrough]];
    case State::Waiting:
        m_state = State::RestartQueued;
        m_restartTimer.start(0);
        break;
    default: break; // 初期化中・再開待ちには最新の設定だけを保存する
    }
}

bool AnalysisSessionHandler::updatePosition(Usi* engine, const QString& position,
                                            int file, int rank, const QString& lastUsiMove)
{
    if (m_mode != Mode::Consideration || m_positionStr == position) return false;
    m_positionStr = position;
    m_previousFileTo = file;
    m_previousRankTo = rank;
    m_lastUsiMove = lastUsiMove;
    requestConsiderationRestart(engine);
    return true;
}

bool AnalysisSessionHandler::handleEngineError(const QString& errorMsg)
{
    if (m_state == State::Starting || m_state == State::CancelStarting) {
        m_startupError = errorMsg;
        return true;
    }
    const Mode previous = m_mode;
    if (previous == Mode::None) return false;
    stopFullAnalysis();
    if (m_hooks.showGameOverDialog) {
        m_hooks.showGameOverDialog(previous == Mode::Tsume ? tr("詰み探索") : tr("検討"),
                                  tr("エンジンエラー: %1").arg(errorMsg));
    }
    return true;
}

void AnalysisSessionHandler::resetOnDestroyEngines() { handleStop(); }

void AnalysisSessionHandler::finalizeTsumeSearch(const QString& resultMessage)
{
    if (m_mode != Mode::Tsume) return;
    stopFullAnalysis();
    if (m_hooks.showGameOverDialog) m_hooks.showGameOverDialog(tr("詰み探索"), resultMessage);
}

void AnalysisSessionHandler::onCheckmateSolved(const QStringList& pv)
{ finalizeTsumeSearch(tr("詰みあり（手順 %1 手）").arg(pv.size())); }
void AnalysisSessionHandler::onCheckmateNoMate() { finalizeTsumeSearch(tr("詰みなし")); }
void AnalysisSessionHandler::onCheckmateNotImplemented() { finalizeTsumeSearch(tr("（エンジン側）未実装")); }
void AnalysisSessionHandler::onCheckmateUnknown() { finalizeTsumeSearch(tr("時間内に詰みを判定できませんでした")); }
void AnalysisSessionHandler::onTsumeBestMoveReceived() { finalizeTsumeSearch(tr("探索が完了しました")); }

void AnalysisSessionHandler::onConsiderationBestMoveReceived()
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

void AnalysisSessionHandler::restartConsiderationDeferred()
{
    if (m_mode != Mode::Consideration || m_state != State::RestartQueued || !m_engine) return;
    if (m_modelPtr) m_modelPtr->clearAllItems();
    m_engine->setPreviousMove(BoardSquare{m_previousFileTo, m_previousRankTo});
    m_engine->setLastUsiMove(m_lastUsiMove);
    m_state = State::Searching;
    m_engine->sendAnalysisCommands(m_positionStr, m_byoyomiMs, m_multiPV);
}
