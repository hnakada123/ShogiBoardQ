/// @file test_stubs_game_end_handler.cpp
/// @brief GameEndHandler テスト用スタブ
///
/// gameendhandler.cpp が参照する外部シンボルのスタブ実装を提供する。
/// テスト対象は gameendhandler.cpp 本体。

#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QVector>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>
#include <QLabel>
#include <QSize>

#include "matchcoordinator.h"
#include "gamestartorchestrator.h"
#include "gamemodestrategy.h"
#include "analysissessionhandler.h"
#include "shogiclock.h"
#include "shogiview.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "usi.h"
#include "enginegameovernotifier.h"
#include "engineprocessmanager.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "thinkinginfopresenter.h"
#include "shogiengineinfoparser.h"
#include "boardinteractioncontroller.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "recordpane.h"
#include "branchtreemanager.h"
#include "pvboarddialog.h"
#include "settingscommon.h"

// ============================================================
// MatchCoordinator スタブ
// ============================================================

#include "strategycontext.h"

MatchCoordinator::MatchCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_view(d.view)
    , m_hooks(d.hooks)
{
    m_sfenHistory = d.sfenRecord ? d.sfenRecord : &m_sharedSfenRecord;
    m_externalGameMoves = d.gameMoves;
}

MatchCoordinator::~MatchCoordinator() = default;

MatchCoordinator::StrategyContext& MatchCoordinator::strategyCtx()
{
    if (!m_strategyCtx) {
        m_strategyCtx = std::make_unique<StrategyContext>(*this);
    }
    return *m_strategyCtx;
}

void MatchCoordinator::configureAndStart(const StartOptions&) {}
void MatchCoordinator::startInitialEngineMoveIfNeeded() {}

namespace {
static MatchCoordinator::TimeControl s_stubTimeControl;
}

void MatchCoordinator::setTimeControlConfig(bool useByoyomi,
                                            int byoyomiMs1, int byoyomiMs2,
                                            int incMs1, int incMs2,
                                            bool loseOnTimeout)
{
    s_stubTimeControl.useByoyomi = useByoyomi;
    s_stubTimeControl.byoyomiMs1 = byoyomiMs1;
    s_stubTimeControl.byoyomiMs2 = byoyomiMs2;
    s_stubTimeControl.incMs1 = incMs1;
    s_stubTimeControl.incMs2 = incMs2;
    s_stubTimeControl.loseOnTimeout = loseOnTimeout;
}

const MatchCoordinator::TimeControl& MatchCoordinator::timeControl() const
{
    return s_stubTimeControl;
}

void MatchCoordinator::refreshGoTimes() {}
void MatchCoordinator::clearGameOverState() { m_gameOver = {}; }

namespace {
static ShogiClock* s_stubClock = nullptr;
}
ShogiClock* MatchCoordinator::clock() { return s_stubClock; }
const ShogiClock* MatchCoordinator::clock() const { return s_stubClock; }
void MatchCoordinator::setClock(ShogiClock* c) { s_stubClock = c; }
void MatchCoordinator::setPlayMode(PlayMode m) { m_playMode = m; }
void MatchCoordinator::pokeTimeUpdateNow() {}

MatchCoordinator::StartOptions MatchCoordinator::buildStartOptions(
    PlayMode mode, const QString& startSfenStr,
    const QStringList*, const StartGameDialog*) const
{
    StartOptions opt;
    opt.mode = mode;
    opt.sfenStart = startSfenStr;
    return opt;
}

void MatchCoordinator::ensureHumanAtBottomIfApplicable(const StartGameDialog*, bool) {}
void MatchCoordinator::handleResign() {}
void MatchCoordinator::handleEngineResign(int) {}
void MatchCoordinator::handleEngineWin(int) {}
void MatchCoordinator::flipBoard() {}
void MatchCoordinator::updateUsiPtrs(Usi*, Usi*) {}
void MatchCoordinator::handleBreakOff() {}
void MatchCoordinator::handleNyugyokuDeclaration(Player, bool, bool) {}
void MatchCoordinator::markTurnEpochNowFor(Player, qint64) {}
qint64 MatchCoordinator::turnEpochFor(Player) const { return -1; }
void MatchCoordinator::armTurnTimerIfNeeded() {}
void MatchCoordinator::finishTurnTimerAndSetConsiderationFor(Player) {}
void MatchCoordinator::disarmHumanTimerIfNeeded() {}
void MatchCoordinator::computeGoTimesForUSI(qint64&, qint64&) const {}
void MatchCoordinator::initializeAndStartEngineFor(Player, const QString&, const QString&) {}
void MatchCoordinator::destroyEngine(int, bool) {}
void MatchCoordinator::destroyEngines(bool) {}
void MatchCoordinator::setGameOver(const GameEndInfo&, bool, bool) {}
void MatchCoordinator::markGameOverMoveAppended() {}
void MatchCoordinator::startAnalysis(const AnalysisOptions&) {}
void MatchCoordinator::stopAnalysisEngine() {}
void MatchCoordinator::updateConsiderationMultiPV(int) {}
bool MatchCoordinator::updateConsiderationPosition(const QString&, int, int, const QString&) { return false; }
Usi* MatchCoordinator::primaryEngine() const { return nullptr; }
Usi* MatchCoordinator::secondaryEngine() const { return nullptr; }
void MatchCoordinator::prepareAndStartGame(PlayMode, const QString&, const QStringList*, const StartGameDialog*, bool) {}
void MatchCoordinator::handlePlayerTimeOut(int) {}
void MatchCoordinator::startMatchTimingAndMaybeInitialGo() {}
void MatchCoordinator::appendGameOverLineAndMark(Cause, Player) {}
void MatchCoordinator::onHumanMove(const QPoint&, const QPoint&, const QString&) {}
void MatchCoordinator::forceImmediateMove() {}
void MatchCoordinator::sendGoToEngine(Usi*, const GoTimes&) {}
void MatchCoordinator::sendStopToEngine(Usi*) {}
void MatchCoordinator::sendRawToEngine(Usi*, const QString&) {}
void MatchCoordinator::appendBreakOffLineAndMark() {}
void MatchCoordinator::handleMaxMovesJishogi() {}
bool MatchCoordinator::checkAndHandleSennichite() { return false; }
void MatchCoordinator::handleSennichite() {}
void MatchCoordinator::handleOuteSennichite(bool) {}
void MatchCoordinator::setUndoBindings(const MatchUndoHandler::UndoRefs&, const MatchUndoHandler::UndoHooks&) {}
bool MatchCoordinator::undoTwoPlies() { return false; }
void MatchCoordinator::initEnginesForEvE(const QString&, const QString&) {}
bool MatchCoordinator::engineThinkApplyMove(Usi*, QString&, QString&, QPoint*, QPoint*) { return false; }
bool MatchCoordinator::engineMoveOnce(Usi*, QString&, QString&, bool, int, QPoint*) { return false; }

// private methods
void MatchCoordinator::ensureUndoHandler() {}
void MatchCoordinator::ensureEngineManager() {}
void MatchCoordinator::ensureTimekeeper() {}
void MatchCoordinator::updateTurnDisplay(Player) {}
MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes() const { return {}; }
void MatchCoordinator::initPositionStringsFromSfen(const QString&) {}
void MatchCoordinator::recomputeClockSnapshot(QString&, QString&, QString&) const {}
void MatchCoordinator::createAndStartModeStrategy(const StartOptions&) {}
void MatchCoordinator::ensureGameStartOrchestrator() {}
void MatchCoordinator::ensureAnalysisSession() {}
void MatchCoordinator::ensureGameEndHandler() {}
EngineLifecycleManager* MatchCoordinator::engineManager() { return nullptr; }
bool MatchCoordinator::isEngineShutdownInProgress() const { return false; }

// private slots
void MatchCoordinator::onUsiError(const QString&) {}

// ============================================================
// AnalysisSessionHandler スタブ
// ============================================================

AnalysisSessionHandler::AnalysisSessionHandler(QObject* parent) : QObject(parent) {}
void AnalysisSessionHandler::setHooks(const Hooks&) {}
bool AnalysisSessionHandler::startFullAnalysis(const MatchCoordinator::AnalysisOptions&) { return false; }
void AnalysisSessionHandler::stopFullAnalysis() {}
void AnalysisSessionHandler::setupModeSpecificWiring(Usi*, const MatchCoordinator::AnalysisOptions&) {}
void AnalysisSessionHandler::startCommunication(Usi*, const MatchCoordinator::AnalysisOptions&) {}
void AnalysisSessionHandler::handleStop() {}
void AnalysisSessionHandler::updateMultiPV(Usi*, int) {}
bool AnalysisSessionHandler::updatePosition(Usi*, const QString&, int, int, const QString&) { return false; }
bool AnalysisSessionHandler::handleEngineError(const QString&) { return false; }
void AnalysisSessionHandler::resetOnDestroyEngines() {}
void AnalysisSessionHandler::onCheckmateSolved(const QStringList&) {}
void AnalysisSessionHandler::onCheckmateNoMate() {}
void AnalysisSessionHandler::onCheckmateNotImplemented() {}
void AnalysisSessionHandler::onCheckmateUnknown() {}
void AnalysisSessionHandler::onTsumeBestMoveReceived() {}
void AnalysisSessionHandler::onConsiderationBestMoveReceived() {}
void AnalysisSessionHandler::restartConsiderationDeferred() {}
void AnalysisSessionHandler::finalizeTsumeSearch(const QString&) {}

// ============================================================
// EngineGameOverNotifier スタブ（テスト対象の依存）
// ============================================================

namespace EngineGameOverNotifier {
void notifyResignation(PlayMode, bool, Usi*, Usi*, const RawSender&) {}
void notifyNyugyoku(PlayMode, bool, bool, Usi*, Usi*, const RawSender&) {}
}

// ============================================================
// ShogiClock スタブ
// ============================================================

ShogiClock::ShogiClock(QObject* parent) : QObject(parent) {}
void ShogiClock::setLoseOnTimeout(bool) {}
void ShogiClock::setPlayerTimes(int, int, int, int, int, int, bool) {}
void ShogiClock::setCurrentPlayer(int p) { m_currentPlayer = p; }
void ShogiClock::startClock() {}
void ShogiClock::stopClock() {}
void ShogiClock::updateClock() {}
void ShogiClock::applyByoyomiAndResetConsideration1() {}
void ShogiClock::applyByoyomiAndResetConsideration2() {}
void ShogiClock::undo() {}
QString ShogiClock::getPlayer1TimeString() const { return {}; }
QString ShogiClock::getPlayer2TimeString() const { return {}; }
QString ShogiClock::getPlayer1ConsiderationTime() const { return {}; }
QString ShogiClock::getPlayer2ConsiderationTime() const { return {}; }
QString ShogiClock::getPlayer1TotalConsiderationTime() const { return {}; }
QString ShogiClock::getPlayer2TotalConsiderationTime() const { return {}; }
QString ShogiClock::getPlayer1ConsiderationAndTotalTime() const { return {}; }
QString ShogiClock::getPlayer2ConsiderationAndTotalTime() const { return {}; }
void ShogiClock::setPlayer1ConsiderationTime(int) {}
void ShogiClock::setPlayer2ConsiderationTime(int) {}
int ShogiClock::getPlayer1ConsiderationTimeMs() const { return 0; }
int ShogiClock::getPlayer2ConsiderationTimeMs() const { return 0; }
void ShogiClock::saveState() {}
void ShogiClock::debugCheckInvariants() const {}
int ShogiClock::remainingDisplaySecP1() const { return 0; }
int ShogiClock::remainingDisplaySecP2() const { return 0; }

// ============================================================
// ShogiView スタブ
// ============================================================

#include "shogiviewlayout.h"
#include "shogiviewinteraction.h"
ShogiViewLayout::ShogiViewLayout() {}
ShogiViewInteraction::ShogiViewInteraction() {}

ShogiView::Highlight::~Highlight() = default;
ShogiView::FieldHighlight::~FieldHighlight() = default;
ShogiView::ShogiView(QWidget* parent) : QWidget(parent) {}
QSize ShogiView::fieldSize() const { return QSize(50, 50); }
QSize ShogiView::sizeHint() const { return QSize(400, 400); }
QSize ShogiView::minimumSizeHint() const { return QSize(200, 200); }
int ShogiView::squareSize() const { return 50; }
void ShogiView::setSquareSize(int) {}
void ShogiView::setFieldSize(QSize) {}
void ShogiView::enlargeBoard(bool) {}
void ShogiView::reduceBoard(bool) {}
void ShogiView::applyClockUrgency(qint64) {}
void ShogiView::paintEvent(QPaintEvent*) {}
void ShogiView::mouseMoveEvent(QMouseEvent*) {}
void ShogiView::mouseReleaseEvent(QMouseEvent*) {}
void ShogiView::resizeEvent(QResizeEvent*) {}
void ShogiView::wheelEvent(QWheelEvent*) {}
bool ShogiView::eventFilter(QObject*, QEvent*) { return false; }
void ShogiView::initializeToFlatStartingPosition() {}
void ShogiView::removeHighlightAllData() {}
void ShogiView::applyBoardAndRender(ShogiBoard*) {}
void ShogiView::setBlackPlayerName(const QString&) {}
void ShogiView::setWhitePlayerName(const QString&) {}
QLabel* ShogiView::blackClockLabel() const { return nullptr; }
QLabel* ShogiView::whiteClockLabel() const { return nullptr; }

// ============================================================
// ShogiBoard スタブ
// ============================================================

ShogiBoard::ShogiBoard(int ranks, int files, QObject* parent)
    : QObject(parent), m_ranks(ranks), m_files(files)
{
    m_boardData.fill(Piece::None, ranks * files);
}
const QVector<Piece>& ShogiBoard::boardData() const { return m_boardData; }
std::optional<SfenComponents> ShogiBoard::parseSfen(const QString&) { return std::nullopt; }
void ShogiBoard::setSfen(const QString&)
{
    m_boardData.fill(Piece::None, m_ranks * m_files);
}
Piece ShogiBoard::getPieceCharacter(int, int) { return Piece::None; }
const QMap<Piece, int>& ShogiBoard::getPieceStand() const { return m_pieceStand; }
Turn ShogiBoard::currentPlayer() const { return Turn::Black; }
QString ShogiBoard::convertBoardToSfen() { return {}; }
QString ShogiBoard::convertStandToSfen() const { return {}; }

// ============================================================
// ShogiGameController スタブ
// ============================================================

ShogiGameController::ShogiGameController(QObject* parent) : QObject(parent) {}
ShogiGameController::~ShogiGameController() = default;
ShogiBoard* ShogiGameController::board() const { return nullptr; }
void ShogiGameController::setCurrentPlayer(const Player p) { m_currentPlayer = p; }
void ShogiGameController::setPromote(bool) {}
void ShogiGameController::newGame(QString&) {}

// ============================================================
// Usi スタブ
// ============================================================

Usi::Usi(UsiCommLogModel*, ShogiEngineThinkingModel*,
         ShogiGameController*, PlayMode& playMode, QObject* parent)
    : QObject(parent), m_playMode(playMode) {}
Usi::~Usi() = default;
QString Usi::scoreStr() const { return {}; }
bool Usi::isResignMove() const { return false; }
bool Usi::isWinMove() const { return false; }
int Usi::lastScoreCp() const { return 0; }
QString Usi::pvKanjiStr() const { return {}; }
void Usi::setPvKanjiStr(const QString&) {}
void Usi::parseMoveCoordinates(int&, int&, int&, int&) {}
void Usi::initializeAndStartEngineCommunication(QString&, QString&) {}
void Usi::handleHumanVsEngineCommunication(QString&, QString&, QPoint&, QPoint&,
                                            int, const QString&, const QString&,
                                            QStringList&, int, int, bool) {}
void Usi::handleEngineVsHumanOrEngineMatchCommunication(QString&, QString&,
                                                         QPoint&, QPoint&,
                                                         int, const QString&, const QString&,
                                                         int, int, bool) {}
void Usi::sendGameOverCommand(GameOverResult) {}
void Usi::sendQuitCommand() {}
QChar Usi::rankToAlphabet(int) const { return QChar(); }
void Usi::sendGameOverWinAndQuitCommands() {}
void Usi::sendStopCommand() {}
void Usi::executeAnalysisCommunication(QString&, int, int) {}
void Usi::sendAnalysisCommands(const QString&, int, int) {}
void Usi::setPreviousFileTo(int) {}
void Usi::setPreviousRankTo(int) {}
void Usi::setLastUsiMove(const QString&) {}
qint64 Usi::lastBestmoveElapsedMs() const { return 0; }
void Usi::sendGameOverLoseAndQuitCommands() {}
void Usi::setLogIdentity(const QString&, const QString&, const QString&) {}
void Usi::setSquelchResignLogging(bool) {}
void Usi::setConsiderationModel(ShogiEngineThinkingModel*, int) {}
void Usi::updateConsiderationMultiPV(int) {}
void Usi::resetResignNotified() {}
void Usi::resetWinNotified() {}
void Usi::markHardTimeout() {}
void Usi::clearHardTimeout() {}
bool Usi::isIgnoring() const { return false; }
QString Usi::currentEnginePath() const { return {}; }
void Usi::sendGoCommand(int, const QString&, const QString&, int, int, bool) {}
void Usi::sendRaw(const QString&) const {}
bool Usi::isEngineRunning() const { return false; }
void Usi::setThinkingModel(ShogiEngineThinkingModel*) {}
void Usi::setLogModel(UsiCommLogModel*) {}
void Usi::prepareBoardDataForAnalysis() {}
void Usi::setClonedBoardData(const QVector<QChar>&) {}
void Usi::setBaseSfen(const QString&) {}
void Usi::flushThinkingInfoBuffer() {}
void Usi::requestClearThinkingInfo() {}
void Usi::cleanupEngineProcessAndThread(bool) {}
void Usi::startAndInitializeEngine(const QString&, const QString&) {}
void Usi::executeTsumeCommunication(QString&, int) {}
void Usi::sendPositionAndGoMateCommands(int, QString&) {}
void Usi::cancelCurrentOperation() {}
void Usi::setupConnections() {}
bool Usi::changeDirectoryToEnginePath(const QString&) { return false; }
void Usi::cloneCurrentBoardData() {}
void Usi::applyMovesToBoardFromBestMoveAndPonder() {}
void Usi::executeEngineCommunication(QString&, QString&, QPoint&, QPoint&,
                                      int, const QString&, const QString&, int, int, bool) {}
void Usi::processEngineResponse(QString&, QString&, int, const QString&, const QString&,
                                 int, int, bool) {}
void Usi::sendCommandsAndProcess(int, QString&, const QString&, const QString&,
                                  QString&, int, int, bool) {}
void Usi::startPonderingAfterBestMove(QString&, QString&) {}
void Usi::appendBestMoveAndStartPondering(QString&, QString&) {}
QString Usi::computeBaseSfenFromBoard() const { return {}; }
void Usi::updateBaseSfenForPonder() {}
QString Usi::convertHumanMoveToUsiFormat(const QPoint&, const QPoint&, bool) { return {}; }
void Usi::waitAndCheckForBestMoveRemainingTime(int, const QString&, const QString&, bool) {}
void Usi::onProcessError(QProcess::ProcessError, const QString&) {}
void Usi::onCommandSent(const QString&) {}
void Usi::onDataReceived(const QString&) {}
void Usi::onStderrReceived(const QString&) {}
void Usi::onSearchedMoveUpdated(const QString&) {}
void Usi::onSearchDepthUpdated(const QString&) {}
void Usi::onNodeCountUpdated(const QString&) {}
void Usi::onNpsUpdated(const QString&) {}
void Usi::onHashUsageUpdated(const QString&) {}
void Usi::onCommLogAppended(const QString&) {}
void Usi::onClearThinkingInfoRequested() {}
void Usi::onAnalysisStopTimeout() {}
void Usi::onConsiderationStopTimeout() {}
void Usi::onThinkingInfoUpdated(const QString&, const QString&, const QString&,
                                 const QString&, const QString&, const QString&,
                                 const QString&, int, int) {}

// ============================================================
// UsiCommLogModel / ShogiEngineThinkingModel スタブ
// ============================================================

UsiCommLogModel::UsiCommLogModel(QObject* parent) : QObject(parent) {}
QString UsiCommLogModel::engineName() const { return m_engineName; }
QString UsiCommLogModel::predictiveMove() const { return {}; }
QString UsiCommLogModel::searchedMove() const { return {}; }
QString UsiCommLogModel::searchDepth() const { return {}; }
QString UsiCommLogModel::nodeCount() const { return {}; }
QString UsiCommLogModel::nodesPerSecond() const { return {}; }
QString UsiCommLogModel::hashUsage() const { return {}; }
QString UsiCommLogModel::usiCommLog() const { return m_usiCommLog; }
void UsiCommLogModel::appendUsiCommLog(const QString&) {}
void UsiCommLogModel::clear() {}
void UsiCommLogModel::setEngineName(const QString& name) { m_engineName = name; }
void UsiCommLogModel::setPredictiveMove(const QString&) {}
void UsiCommLogModel::setSearchedMove(const QString&) {}
void UsiCommLogModel::setSearchDepth(const QString&) {}
void UsiCommLogModel::setNodeCount(const QString&) {}
void UsiCommLogModel::setNodesPerSecond(const QString&) {}
void UsiCommLogModel::setHashUsage(const QString&) {}

ShogiEngineThinkingModel::ShogiEngineThinkingModel(QObject* parent) : AbstractListModel(parent) {}
int ShogiEngineThinkingModel::columnCount(const QModelIndex&) const { return 6; }
QVariant ShogiEngineThinkingModel::data(const QModelIndex&, int) const { return {}; }
QVariant ShogiEngineThinkingModel::headerData(int, Qt::Orientation, int) const { return {}; }
QString ShogiEngineThinkingModel::usiPvAt(int) const { return {}; }
const ShogiInfoRecord* ShogiEngineThinkingModel::recordAt(int) const { return nullptr; }
int ShogiEngineThinkingModel::findRowByMultipv(int) const { return -1; }
void ShogiEngineThinkingModel::updateByMultipv(ShogiInfoRecord*, int) {}

ShogiInfoRecord::ShogiInfoRecord(QObject* parent) : QObject(parent) {}
ShogiInfoRecord::ShogiInfoRecord(const QString& time, const QString& depth, const QString& nodes,
                                  const QString& score, const QString& pv, QObject* parent)
    : QObject(parent), m_time(time), m_depth(depth), m_nodes(nodes), m_score(score), m_pv(pv) {}
ShogiInfoRecord::ShogiInfoRecord(const QString& time, const QString& depth, const QString& nodes,
                                  const QString& score, const QString& pv, const QString& usiPv,
                                  QObject* parent)
    : QObject(parent), m_time(time), m_depth(depth), m_nodes(nodes), m_score(score), m_pv(pv), m_usiPv(usiPv) {}
QString ShogiInfoRecord::time() const { return m_time; }
QString ShogiInfoRecord::depth() const { return m_depth; }
QString ShogiInfoRecord::nodes() const { return m_nodes; }
QString ShogiInfoRecord::score() const { return m_score; }
QString ShogiInfoRecord::pv() const { return m_pv; }
QString ShogiInfoRecord::usiPv() const { return m_usiPv; }
void ShogiInfoRecord::setUsiPv(const QString& usiPv) { m_usiPv = usiPv; }
QString ShogiInfoRecord::baseSfen() const { return m_baseSfen; }
void ShogiInfoRecord::setBaseSfen(const QString& sfen) { m_baseSfen = sfen; }
QString ShogiInfoRecord::lastUsiMove() const { return m_lastUsiMove; }
void ShogiInfoRecord::setLastUsiMove(const QString& move) { m_lastUsiMove = move; }
int ShogiInfoRecord::multipv() const { return m_multipv; }
void ShogiInfoRecord::setMultipv(int multipv) { m_multipv = multipv; }
int ShogiInfoRecord::scoreCp() const { return m_scoreCp; }
void ShogiInfoRecord::setScoreCp(int scoreCp) { m_scoreCp = scoreCp; }

// ============================================================
// EngineProcessManager スタブ
// ============================================================

EngineProcessManager::EngineProcessManager(QObject* parent) : QObject(parent) {}
EngineProcessManager::~EngineProcessManager() = default;
bool EngineProcessManager::startProcess(const QString&) { return false; }
void EngineProcessManager::stopProcess() {}
bool EngineProcessManager::isRunning() const { return false; }
QProcess::ProcessState EngineProcessManager::state() const { return QProcess::NotRunning; }
void EngineProcessManager::sendCommand(const QString&) {}
void EngineProcessManager::closeWriteChannel() {}
void EngineProcessManager::setShutdownState(ShutdownState) {}
void EngineProcessManager::setPostQuitInfoStringLinesLeft(int) {}
void EngineProcessManager::decrementPostQuitLines() {}
void EngineProcessManager::discardStdout() {}
void EngineProcessManager::discardStderr() {}
bool EngineProcessManager::canReadLine() const { return false; }
QByteArray EngineProcessManager::readLine() { return {}; }
QByteArray EngineProcessManager::readAllStderr() { return {}; }
void EngineProcessManager::setLogIdentity(const QString&, const QString&, const QString&) {}
QString EngineProcessManager::logPrefix() const { return {}; }
void EngineProcessManager::onReadyReadStdout() {}
void EngineProcessManager::onReadyReadStderr() {}
void EngineProcessManager::onProcessError(QProcess::ProcessError) {}
void EngineProcessManager::onProcessFinished(int, QProcess::ExitStatus) {}
void EngineProcessManager::scheduleMoreReading() {}

// ============================================================
// ThinkingInfoPresenter / ShogiEngineInfoParser スタブ
// ============================================================

ThinkingInfoPresenter::ThinkingInfoPresenter(QObject* parent) : QObject(parent) {}
ThinkingInfoPresenter::~ThinkingInfoPresenter() = default;
void ThinkingInfoPresenter::setGameController(ShogiGameController*) {}
void ThinkingInfoPresenter::setAnalysisMode(bool) {}
void ThinkingInfoPresenter::setPreviousMove(int, int) {}
void ThinkingInfoPresenter::setClonedBoardData(const QVector<QChar>&) {}
void ThinkingInfoPresenter::setPonderEnabled(bool) {}
void ThinkingInfoPresenter::setBaseSfen(const QString&) {}
QString ThinkingInfoPresenter::baseSfen() const { return {}; }
void ThinkingInfoPresenter::processInfoLine(const QString&) {}
void ThinkingInfoPresenter::requestClearThinkingInfo() {}
void ThinkingInfoPresenter::flushInfoBuffer() {}
void ThinkingInfoPresenter::logSentCommand(const QString&, const QString&) {}
void ThinkingInfoPresenter::logReceivedData(const QString&, const QString&) {}
void ThinkingInfoPresenter::logStderrData(const QString&, const QString&) {}
void ThinkingInfoPresenter::onInfoReceived(const QString&) {}

ShogiEngineInfoParser::ShogiEngineInfoParser() {}

// ============================================================
// UI Widget スタブ
// ============================================================

void BoardInteractionController::clearAllHighlights() {}
void EvaluationChartWidget::clearAll() {}
void EvaluationGraphController::clearScores() {}
void EvaluationGraphController::trimToPly(int) {}
QTableView* RecordPane::kifuView() const { return nullptr; }

BranchTreeManager::BranchTreeManager(QObject* parent) : QObject(parent) {}
BranchTreeManager::~BranchTreeManager() = default;
void BranchTreeManager::highlightBranchTreeAt(int, int, bool) {}
bool BranchTreeManager::eventFilter(QObject* obj, QEvent* ev) { return QObject::eventFilter(obj, ev); }

PvBoardDialog::PvBoardDialog(const QString&, const QStringList&, QWidget* parent)
    : QDialog(parent) {}
PvBoardDialog::~PvBoardDialog() = default;
void PvBoardDialog::setKanjiPv(const QString&) {}
void PvBoardDialog::setPlayerNames(const QString&, const QString&) {}
void PvBoardDialog::setLastMove(const QString&) {}
void PvBoardDialog::setPrevSfenForHighlight(const QString&) {}
void PvBoardDialog::setFlipMode(bool) {}
void PvBoardDialog::onGoFirst() {}
void PvBoardDialog::onGoBack() {}
void PvBoardDialog::onGoForward() {}
void PvBoardDialog::onGoLast() {}
void PvBoardDialog::onEnlargeBoard() {}
void PvBoardDialog::onReduceBoard() {}
void PvBoardDialog::onFlipBoard() {}
void PvBoardDialog::closeEvent(QCloseEvent*) {}
bool PvBoardDialog::eventFilter(QObject*, QEvent*) { return false; }

// ============================================================
// UsiProtocolHandler / ElideLabel スタブ
// ============================================================

UsiProtocolHandler::UsiProtocolHandler(QObject* parent) : QObject(parent) {}
UsiProtocolHandler::~UsiProtocolHandler() = default;
void UsiProtocolHandler::onDataReceived(const QString&) {}

ElideLabel::ElideLabel(QWidget* parent) : QLabel(parent) {}
QSize ElideLabel::sizeHint() const { return QSize(100, 20); }
void ElideLabel::paintEvent(QPaintEvent*) {}
void ElideLabel::resizeEvent(QResizeEvent*) {}
void ElideLabel::enterEvent(QEnterEvent*) {}
void ElideLabel::leaveEvent(QEvent*) {}
void ElideLabel::mousePressEvent(QMouseEvent*) {}
void ElideLabel::mouseReleaseEvent(QMouseEvent*) {}
void ElideLabel::mouseMoveEvent(QMouseEvent*) {}
void ElideLabel::onTimerTimeout() {}

// ============================================================
// EngineVsEngineStrategy スタブ
// ============================================================

#include "enginevsenginestrategy.h"

EngineVsEngineStrategy::EngineVsEngineStrategy(
    MatchCoordinator::StrategyContext& ctx,
    const MatchCoordinator::StartOptions&,
    QObject* parent)
    : QObject(parent), m_ctx(ctx) {}
void EngineVsEngineStrategy::start() {}
void EngineVsEngineStrategy::onHumanMove(const QPoint&, const QPoint&, const QString&) {}
QStringList* EngineVsEngineStrategy::sfenRecordForEvE() { return &m_eveSfenRecord; }
QVector<ShogiMove>& EngineVsEngineStrategy::gameMovesForEvE() { return m_eveGameMoves; }
void EngineVsEngineStrategy::kickNextEvETurn() {}
void EngineVsEngineStrategy::initPositionStringsForEvE(const QString&) {}
void EngineVsEngineStrategy::startEvEFirstMoveByBlack() {}
void EngineVsEngineStrategy::startEvEFirstMoveByWhite() {}

// ============================================================
// SennichiteDetector スタブ
// ============================================================

#include "sennichitedetector.h"

SennichiteDetector::Result SennichiteDetector::check(const QStringList&)
{
    return Result::None;
}

QString SennichiteDetector::positionKey(const QString& sfen)
{
    const auto lastSpace = sfen.lastIndexOf(QLatin1Char(' '));
    if (lastSpace <= 0) return sfen;
    return sfen.left(lastSpace);
}

// ============================================================
// SettingsService スタブ
// ============================================================

namespace SettingsCommon {
QString settingsFilePath() { return QStringLiteral("/tmp/tst_game_end_handler_stub.ini"); }
QSettings& openSettings() { static QSettings s(settingsFilePath(), QSettings::IniFormat); return s; }
}

// ============================================================
// MOC出力のインクルード
// ============================================================

#include "moc_matchcoordinator.cpp"
#include "moc_gameendhandler.cpp"
#include "moc_shogiclock.cpp"
#include "moc_shogiview.cpp"
#include "moc_shogiboard.cpp"
#include "moc_shogigamecontroller.cpp"
#include "moc_usi.cpp"
#include "moc_usicommlogmodel.cpp"
#include "moc_shogienginethinkingmodel.cpp"
#include "moc_shogiinforecord.cpp"
#include "moc_engineprocessmanager.cpp"
#include "moc_thinkinginfopresenter.cpp"
#include "moc_shogiengineinfoparser.cpp"
#include "moc_usiprotocolhandler.cpp"
#include "moc_pvboarddialog.cpp"
#include "moc_elidelabel.cpp"
#include "moc_branchtreemanager.cpp"
#include "moc_analysissessionhandler.cpp"
#include "moc_enginevsenginestrategy.cpp"
