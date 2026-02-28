/// @file test_stubs_matchcoordinator.cpp
/// @brief MatchCoordinator テスト用スタブ
///
/// matchcoordinator.cpp が参照する外部シンボルのスタブ実装を提供する。
/// テスト対象は matchcoordinator.cpp 本体。

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
#include "matchtimekeeper.h"
#include "matchundohandler.h"
#include "gameendhandler.h"
#include "gamestartorchestrator.h"
#include "analysissessionhandler.h"
#include "gamemodestrategy.h"
#include "humanvshumanstrategy.h"
#include "humanvsenginestrategy.h"
#include "enginevsenginestrategy.h"
#include "sennichitedetector.h"
#include "shogiclock.h"
#include "shogiview.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "usi.h"
#include "usimatchhandler.h"
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
// テスト用トラッカー（デリゲーション検証用）
// ============================================================
namespace MCTracker {

bool gameEndHandlerResignCalled = false;
bool gameEndHandlerBreakOffCalled = false;
int  gameEndHandlerEngineResignIdx = -1;
int  gameEndHandlerEngineWinIdx = -1;
bool gameStartOrchestratorConfigureCalled = false;
bool matchTimekeeperSetTimeControlCalled = false;
bool analysisSessionStartCalled = false;
bool analysisSessionStopCalled = false;

void reset()
{
    gameEndHandlerResignCalled = false;
    gameEndHandlerBreakOffCalled = false;
    gameEndHandlerEngineResignIdx = -1;
    gameEndHandlerEngineWinIdx = -1;
    gameStartOrchestratorConfigureCalled = false;
    matchTimekeeperSetTimeControlCalled = false;
    analysisSessionStartCalled = false;
    analysisSessionStopCalled = false;
}

} // namespace MCTracker

// ============================================================
// EngineLifecycleManager スタブ
// ============================================================

EngineLifecycleManager::EngineLifecycleManager(QObject* parent) : QObject(parent) {}
void EngineLifecycleManager::setRefs(const Refs&) {}
void EngineLifecycleManager::setHooks(const Hooks&) {}
void EngineLifecycleManager::updateUsiPtrs(Usi*, Usi*) {}
void EngineLifecycleManager::setModelPtrs(UsiCommLogModel*, ShogiEngineThinkingModel*,
                                          UsiCommLogModel*, ShogiEngineThinkingModel*) {}
Usi* EngineLifecycleManager::usi1() const { return m_usi1; }
Usi* EngineLifecycleManager::usi2() const { return m_usi2; }
void EngineLifecycleManager::setUsi1(Usi* u) { m_usi1 = u; }
void EngineLifecycleManager::setUsi2(Usi* u) { m_usi2 = u; }
bool EngineLifecycleManager::isShutdownInProgress() const { return m_engineShutdownInProgress; }
void EngineLifecycleManager::setShutdownInProgress(bool v) { m_engineShutdownInProgress = v; }
EngineLifecycleManager::EngineModelPair EngineLifecycleManager::ensureEngineModels(int) { return {}; }
void EngineLifecycleManager::initializeAndStartEngineFor(Player, const QString&, const QString&) {}
void EngineLifecycleManager::destroyEngine(int, bool) {}
void EngineLifecycleManager::destroyEngines(bool) {}
void EngineLifecycleManager::initEnginesForEvE(const QString&, const QString&) {}
bool EngineLifecycleManager::engineThinkApplyMove(Usi*, QString&, QString&, QPoint*, QPoint*) { return false; }
bool EngineLifecycleManager::engineMoveOnce(Usi*, QString&, QString&, bool, int, QPoint*) { return false; }
void EngineLifecycleManager::sendGoToEngine(Usi*, const GoTimes&) {}
void EngineLifecycleManager::sendStopToEngine(Usi*) {}
void EngineLifecycleManager::sendRawToEngine(Usi*, const QString&) {}
void EngineLifecycleManager::wireResignToArbiter(Usi*, bool) {}
void EngineLifecycleManager::wireWinToArbiter(Usi*, bool) {}
void EngineLifecycleManager::onEngine1Resign() {}
void EngineLifecycleManager::onEngine2Resign() {}
void EngineLifecycleManager::onEngine1Win() {}
void EngineLifecycleManager::onEngine2Win() {}

// ============================================================
// MatchTimekeeper スタブ
// ============================================================

namespace {
static MatchCoordinator::TimeControl s_stubTimeControl;
}

MatchTimekeeper::MatchTimekeeper(QObject* parent) : QObject(parent) {}
void MatchTimekeeper::setRefs(const Refs&) {}
void MatchTimekeeper::setHooks(const Hooks&) {}
void MatchTimekeeper::setClock(ShogiClock* c) { m_clock = c; }
ShogiClock* MatchTimekeeper::clock() const { return m_clock; }
const ShogiClock* MatchTimekeeper::clockConst() const { return m_clock; }
void MatchTimekeeper::setTimeControlConfig(bool useByoyomi,
                                           int byoyomiMs1, int byoyomiMs2,
                                           int incMs1, int incMs2,
                                           bool loseOnTimeout)
{
    MCTracker::matchTimekeeperSetTimeControlCalled = true;
    s_stubTimeControl.useByoyomi = useByoyomi;
    s_stubTimeControl.byoyomiMs1 = byoyomiMs1;
    s_stubTimeControl.byoyomiMs2 = byoyomiMs2;
    s_stubTimeControl.incMs1 = incMs1;
    s_stubTimeControl.incMs2 = incMs2;
    s_stubTimeControl.loseOnTimeout = loseOnTimeout;
}
const MatchTimekeeper::TimeControl& MatchTimekeeper::timeControl() const { return s_stubTimeControl; }
MatchTimekeeper::GoTimes MatchTimekeeper::computeGoTimes() const { return {}; }
void MatchTimekeeper::computeGoTimesForUSI(qint64& b, qint64& w) const { b = 0; w = 0; }
void MatchTimekeeper::refreshGoTimes() {}
void MatchTimekeeper::markTurnEpochNowFor(Player, qint64) {}
qint64 MatchTimekeeper::turnEpochFor(Player) const { return -1; }
void MatchTimekeeper::recomputeClockSnapshot(QString& t, QString& p1, QString& p2) const
{
    t.clear(); p1.clear(); p2.clear();
}
void MatchTimekeeper::pokeTimeUpdateNow() {}
void MatchTimekeeper::onClockTick() {}
void MatchTimekeeper::wireClock() {}
void MatchTimekeeper::unwireClock() {}
void MatchTimekeeper::emitTimeUpdateFromClock() {}

// ============================================================
// GameEndHandler スタブ
// ============================================================

GameEndHandler::GameEndHandler(QObject* parent) : QObject(parent) {}
void GameEndHandler::setRefs(const Refs&) {}
void GameEndHandler::setHooks(const Hooks&) {}
void GameEndHandler::handleResign() { MCTracker::gameEndHandlerResignCalled = true; }
void GameEndHandler::handleEngineResign(int idx) { MCTracker::gameEndHandlerEngineResignIdx = idx; }
void GameEndHandler::handleEngineWin(int idx) { MCTracker::gameEndHandlerEngineWinIdx = idx; }
void GameEndHandler::handleBreakOff()
{
    MCTracker::gameEndHandlerBreakOffCalled = true;
}
void GameEndHandler::handleNyugyokuDeclaration(Player, bool, bool) {}
void GameEndHandler::appendGameOverLineAndMark(Cause, Player) {}
void GameEndHandler::appendBreakOffLineAndMark() {}
void GameEndHandler::handleMaxMovesJishogi() {}
bool GameEndHandler::checkAndHandleSennichite() { return false; }
void GameEndHandler::handleSennichite() {}
void GameEndHandler::handleOuteSennichite(bool) {}

// ============================================================
// GameStartOrchestrator スタブ
// ============================================================

void GameStartOrchestrator::setRefs(const Refs&) {}
void GameStartOrchestrator::setHooks(const Hooks&) {}
void GameStartOrchestrator::configureAndStart(const StartOptions&)
{
    MCTracker::gameStartOrchestratorConfigureCalled = true;
}

MatchCoordinator::StartOptions GameStartOrchestrator::buildStartOptions(
    PlayMode mode, const QString& startSfenStr,
    const QStringList*, const StartGameDialog*)
{
    MatchCoordinator::StartOptions opt;
    opt.mode = mode;
    opt.sfenStart = startSfenStr;
    return opt;
}

void GameStartOrchestrator::ensureHumanAtBottomIfApplicable(const StartGameDialog*, bool) {}
void GameStartOrchestrator::prepareAndStartGame(PlayMode, const QString&,
                                                 const QStringList*, const StartGameDialog*, bool) {}

// ============================================================
// MatchUndoHandler スタブ
// ============================================================

void MatchUndoHandler::setRefs(const Refs&) {}
void MatchUndoHandler::setUndoBindings(const UndoRefs&, const UndoHooks&) {}
bool MatchUndoHandler::undoTwoPlies() { return false; }

bool MatchUndoHandler::isStandardStartposSfen(const QString& sfen)
{
    return sfen == QLatin1String("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

// ============================================================
// AnalysisSessionHandler スタブ
// ============================================================

AnalysisSessionHandler::AnalysisSessionHandler(QObject* parent) : QObject(parent) {}
void AnalysisSessionHandler::setHooks(const Hooks&) {}
bool AnalysisSessionHandler::startFullAnalysis(const MatchCoordinator::AnalysisOptions&)
{
    MCTracker::analysisSessionStartCalled = true;
    return true;
}
void AnalysisSessionHandler::stopFullAnalysis()
{
    MCTracker::analysisSessionStopCalled = true;
}
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
// Strategy スタブ
// ============================================================

HumanVsHumanStrategy::HumanVsHumanStrategy(MatchCoordinator::StrategyContext& ctx)
    : m_ctx(ctx) {}
void HumanVsHumanStrategy::start() {}
void HumanVsHumanStrategy::onHumanMove(const QPoint&, const QPoint&, const QString&) {}
void HumanVsHumanStrategy::armTurnTimerIfNeeded() {}
void HumanVsHumanStrategy::finishTurnTimerAndSetConsideration(int) {}

HumanVsEngineStrategy::HumanVsEngineStrategy(MatchCoordinator::StrategyContext& ctx,
                                              bool engineIsP1,
                                              const QString& enginePath,
                                              const QString& engineName)
    : m_ctx(ctx), m_engineIsP1(engineIsP1), m_enginePath(enginePath), m_engineName(engineName) {}
void HumanVsEngineStrategy::start() {}
void HumanVsEngineStrategy::onHumanMove(const QPoint&, const QPoint&, const QString&) {}
void HumanVsEngineStrategy::armTurnTimerIfNeeded() {}
void HumanVsEngineStrategy::finishTurnTimerAndSetConsideration(int) {}
void HumanVsEngineStrategy::disarmTurnTimer() {}
void HumanVsEngineStrategy::startInitialMoveIfNeeded() {}

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
// EngineGameOverNotifier スタブ
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
void ShogiGameController::applyTimeoutLossFor(int) {}
void ShogiGameController::finalizeGameResult() {}
void ShogiGameController::resetResult() {}

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

#include "pvboardcontroller.h"
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
// SettingsService スタブ
// ============================================================

namespace SettingsCommon {
QString settingsFilePath() { return QStringLiteral("/tmp/tst_matchcoordinator_stub.ini"); }
QSettings& openSettings() { static QSettings s(settingsFilePath(), QSettings::IniFormat); return s; }
}

// ============================================================
// MOC出力のインクルード
// ============================================================

#include "moc_matchcoordinator.cpp"
#include "moc_enginelifecyclemanager.cpp"
#include "moc_matchtimekeeper.cpp"
#include "moc_gameendhandler.cpp"
#include "moc_analysissessionhandler.cpp"
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
#include "moc_enginevsenginestrategy.cpp"
