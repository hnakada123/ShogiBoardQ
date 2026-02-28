/// @file test_stubs_wiring_contracts.cpp
/// @brief 配線契約テスト用スタブ
///
/// matchcoordinatorwiring.cpp が参照する外部シンボルのスタブ実装を提供する。

#include <QObject>
#include <QPoint>
#include <QVector>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "matchcoordinator.h"
#include "gamestartcoordinator.h"
#include "gamesessionfacade.h"
#include "gamesessionorchestrator.h"
#include "mainwindowappearancecontroller.h"
#include "playerinfowiring.h"
#include "kifunavigationcoordinator.h"
#include "branchnavigationwiring.h"
#include "uistatepolicymanager.h"
#include "evaluationgraphcontroller.h"
#include "timecontrolcontroller.h"
#include "timedisplaypresenter.h"
#include "boardinteractioncontroller.h"
#include "shogiclock.h"
#include "matchcoordinatorhooksfactory.h"
#include "shogiview.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "enginelifecyclemanager.h"
#include "matchtimekeeper.h"
#include "matchundohandler.h"

// complete types for MatchCoordinator unique_ptr members
#include "strategycontext.h"
#include "gamemodestrategy.h"
#include "analysissessionhandler.h"
#include "gamestartorchestrator.h"
#include "gameendhandler.h"

// ============================================================
// TestTracker — slot 呼び出しを追跡する名前空間
// ============================================================

namespace TestTracker {
    // wireForwardingSignals 経由の呼び出し追跡
    bool onRequestAppendGameOverMoveCalled = false;
    bool onBoardFlippedCalled = false;
    bool onGameOverStateChangedCalled = false;
    bool onMatchGameEndedCalled = false;
    bool onResignationTriggeredCalled = false;
    bool onPreStartCleanupRequestedCalled = false;
    bool onApplyTimeControlRequestedCalled = false;
    bool onMenuPlayerNamesResolvedCalled = false;
    bool onConsecutiveGamesConfiguredCalled = false;
    bool onGameStartedCalled = false;
    bool selectKifuRowCalled = false;
    bool onBranchTreeResetForNewGameCalled = false;

    int selectKifuRowValue = -1;

    void reset()
    {
        onRequestAppendGameOverMoveCalled = false;
        onBoardFlippedCalled = false;
        onGameOverStateChangedCalled = false;
        onMatchGameEndedCalled = false;
        onResignationTriggeredCalled = false;
        onPreStartCleanupRequestedCalled = false;
        onApplyTimeControlRequestedCalled = false;
        onMenuPlayerNamesResolvedCalled = false;
        onConsecutiveGamesConfiguredCalled = false;
        onGameStartedCalled = false;
        selectKifuRowCalled = false;
        onBranchTreeResetForNewGameCalled = false;
        selectKifuRowValue = -1;
    }
}

// ============================================================
// MatchCoordinator スタブ
// ============================================================

MatchCoordinator::MatchCoordinator(const Deps&, QObject* parent)
    : QObject(parent)
{
}
MatchCoordinator::~MatchCoordinator() = default;

MatchCoordinator::StrategyContext& MatchCoordinator::strategyCtx()
{
    static StrategyContext dummy(*this);
    return dummy;
}

void MatchCoordinator::updateUsiPtrs(Usi*, Usi*) {}
void MatchCoordinator::setPlayMode(PlayMode) {}
void MatchCoordinator::setUndoBindings(const UndoRefs&, const UndoHooks&) {}
void MatchCoordinator::handleResign() {}
void MatchCoordinator::handleEngineResign(int) {}
void MatchCoordinator::handleEngineWin(int) {}
void MatchCoordinator::flipBoard() {}
void MatchCoordinator::handleBreakOff() {}
void MatchCoordinator::handleNyugyokuDeclaration(Player, bool, bool) {}
void MatchCoordinator::setTimeControlConfig(bool, int, int, int, int, bool) {}
const MatchCoordinator::TimeControl& MatchCoordinator::timeControl() const
{
    static TimeControl tc;
    return tc;
}
void MatchCoordinator::markTurnEpochNowFor(Player, qint64) {}
qint64 MatchCoordinator::turnEpochFor(Player) const { return 0; }
void MatchCoordinator::armTurnTimerIfNeeded() {}
void MatchCoordinator::finishTurnTimerAndSetConsiderationFor(Player) {}
void MatchCoordinator::disarmHumanTimerIfNeeded() {}
void MatchCoordinator::computeGoTimesForUSI(qint64&, qint64&) const {}
void MatchCoordinator::refreshGoTimes() {}
void MatchCoordinator::startInitialEngineMoveIfNeeded() {}
bool MatchCoordinator::undoTwoPlies() { return false; }
void MatchCoordinator::initializeAndStartEngineFor(Player, const QString&, const QString&) {}
void MatchCoordinator::destroyEngine(int, bool) {}
void MatchCoordinator::destroyEngines(bool) {}
void MatchCoordinator::clearGameOverState() {}
void MatchCoordinator::setGameOver(const GameEndInfo&, bool, bool) {}
void MatchCoordinator::markGameOverMoveAppended() {}
void MatchCoordinator::startAnalysis(const AnalysisOptions&) {}
void MatchCoordinator::stopAnalysisEngine() {}
void MatchCoordinator::updateConsiderationMultiPV(int) {}
bool MatchCoordinator::updateConsiderationPosition(const QString&, int, int, const QString&) { return false; }
Usi* MatchCoordinator::primaryEngine() const { return nullptr; }
Usi* MatchCoordinator::secondaryEngine() const { return nullptr; }
void MatchCoordinator::configureAndStart(const StartOptions&) {}
MatchCoordinator::StartOptions MatchCoordinator::buildStartOptions(PlayMode, const QString&, const QStringList*, const StartGameDialog*) const { return {}; }
void MatchCoordinator::ensureHumanAtBottomIfApplicable(const StartGameDialog*, bool) {}
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
ShogiClock* MatchCoordinator::clock() { return nullptr; }
const ShogiClock* MatchCoordinator::clock() const { return nullptr; }
void MatchCoordinator::setClock(ShogiClock*) {}
void MatchCoordinator::recomputeClockSnapshot(QString&, QString&, QString&) const {}
void MatchCoordinator::initEnginesForEvE(const QString&, const QString&) {}
bool MatchCoordinator::engineThinkApplyMove(Usi*, QString&, QString&, QPoint*, QPoint*) { return false; }
bool MatchCoordinator::engineMoveOnce(Usi*, QString&, QString&, bool, int, QPoint*) { return false; }
bool MatchCoordinator::isEngineShutdownInProgress() const { return false; }
EngineLifecycleManager* MatchCoordinator::engineManager() { return nullptr; }
void MatchCoordinator::pokeTimeUpdateNow() {}
void MatchCoordinator::onUsiError(const QString&) {}
void MatchCoordinator::updateTurnDisplay(Player) {}
MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes() const { return {}; }
void MatchCoordinator::initPositionStringsFromSfen(const QString&) {}
void MatchCoordinator::createAndStartModeStrategy(const StartOptions&) {}
void MatchCoordinator::ensureEngineManager() {}
void MatchCoordinator::ensureTimekeeper() {}
void MatchCoordinator::ensureAnalysisSession() {}
void MatchCoordinator::ensureGameEndHandler() {}
void MatchCoordinator::ensureGameStartOrchestrator() {}
void MatchCoordinator::ensureUndoHandler() {}

// ============================================================
// GameStartCoordinator スタブ
// ============================================================

GameStartCoordinator::GameStartCoordinator(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_match(deps.match)
    , m_clock(deps.clock)
    , m_gc(deps.gc)
    , m_view(deps.view)
{
}

void GameStartCoordinator::start(const StartParams&) {}
void GameStartCoordinator::prepare(const Request&) {}
void GameStartCoordinator::prepareDataCurrentPosition(const Ctx&) {}
void GameStartCoordinator::prepareInitialPosition(const Ctx&) {}
void GameStartCoordinator::initializeGame(const Ctx&) {}
void GameStartCoordinator::setTimerAndStart(const Ctx&) {}
PlayMode GameStartCoordinator::setPlayMode(const Ctx&) const { return PlayMode::NotStarted; }
PlayMode GameStartCoordinator::determinePlayModeAlignedWithTurn(int, bool, bool, const QString&) { return PlayMode::NotStarted; }
void GameStartCoordinator::applyPlayersNamesForMode(ShogiView*, PlayMode, const QString&, const QString&, const QString&, const QString&) const {}
void GameStartCoordinator::applyResumePositionIfAny(ShogiGameController*, ShogiView*, const QString&) {}
void GameStartCoordinator::setMatch(MatchCoordinator*) {}
bool GameStartCoordinator::validate(const StartParams&, QString&) const { return true; }

// ============================================================
// GameSessionFacade スタブ
// ============================================================

GameSessionFacade::GameSessionFacade(const Deps&) {}
bool GameSessionFacade::initialize() { return true; }
MatchCoordinator* GameSessionFacade::match() const { return nullptr; }

// ============================================================
// GameSessionOrchestrator スタブ（TestTracker 連動）
// ============================================================

GameSessionOrchestrator::GameSessionOrchestrator(QObject* parent) : QObject(parent) {}
void GameSessionOrchestrator::updateDeps(const Deps&) {}
void GameSessionOrchestrator::initializeGame() {}
void GameSessionOrchestrator::handleResignation() {}
void GameSessionOrchestrator::handleBreakOffGame() {}
void GameSessionOrchestrator::movePieceImmediately() {}
void GameSessionOrchestrator::stopTsumeSearch() {}
void GameSessionOrchestrator::openWebsiteInExternalBrowser() {}

void GameSessionOrchestrator::onRequestAppendGameOverMove(const MatchCoordinator::GameEndInfo&)
{
    TestTracker::onRequestAppendGameOverMoveCalled = true;
}

void GameSessionOrchestrator::onGameOverStateChanged(const MatchCoordinator::GameOverState&)
{
    TestTracker::onGameOverStateChangedCalled = true;
}

void GameSessionOrchestrator::onMatchGameEnded(const MatchCoordinator::GameEndInfo&)
{
    TestTracker::onMatchGameEndedCalled = true;
}

void GameSessionOrchestrator::onResignationTriggered()
{
    TestTracker::onResignationTriggeredCalled = true;
}

void GameSessionOrchestrator::onPreStartCleanupRequested()
{
    TestTracker::onPreStartCleanupRequestedCalled = true;
}

void GameSessionOrchestrator::onApplyTimeControlRequested(const GameStartCoordinator::TimeControl&)
{
    TestTracker::onApplyTimeControlRequestedCalled = true;
}

void GameSessionOrchestrator::onGameStarted(const MatchCoordinator::StartOptions&)
{
    TestTracker::onGameStartedCalled = true;
}

void GameSessionOrchestrator::onConsecutiveStartRequested(const GameStartCoordinator::StartParams&) {}

void GameSessionOrchestrator::onConsecutiveGamesConfigured(int, bool)
{
    TestTracker::onConsecutiveGamesConfiguredCalled = true;
}

void GameSessionOrchestrator::startNextConsecutiveGame() {}
void GameSessionOrchestrator::startNewShogiGame() {}
void GameSessionOrchestrator::invokeStartGame() {}

// ============================================================
// MainWindowAppearanceController スタブ（TestTracker 連動）
// ============================================================

MainWindowAppearanceController::MainWindowAppearanceController(QObject* parent) : QObject(parent) {}
void MainWindowAppearanceController::updateDeps(const Deps&) {}
void MainWindowAppearanceController::setupCentralWidgetContainer(QWidget*) {}
void MainWindowAppearanceController::configureToolBarFromUi(QToolBar*, QAction*) {}
void MainWindowAppearanceController::installAppToolTips(QMainWindow*) {}
void MainWindowAppearanceController::setupBoardInCenter() {}
void MainWindowAppearanceController::setupNameAndClockFonts() {}
void MainWindowAppearanceController::onBoardSizeChanged(QSize) {}
void MainWindowAppearanceController::performDeferredEvalChartResize() {}
void MainWindowAppearanceController::flipBoardAndUpdatePlayerInfo() {}

void MainWindowAppearanceController::onBoardFlipped(bool)
{
    TestTracker::onBoardFlippedCalled = true;
}

void MainWindowAppearanceController::onActionFlipBoardTriggered() {}
void MainWindowAppearanceController::onActionEnlargeBoardTriggered() {}
void MainWindowAppearanceController::onActionShrinkBoardTriggered() {}
void MainWindowAppearanceController::onToolBarVisibilityToggled(bool) {}
void MainWindowAppearanceController::onTabCurrentChanged(int) {}

// ============================================================
// PlayerInfoWiring スタブ（TestTracker 連動）
// ============================================================

PlayerInfoWiring::PlayerInfoWiring(const Dependencies&, QObject* parent) : QObject(parent) {}
void PlayerInfoWiring::addGameInfoTabAtStartup() {}
void PlayerInfoWiring::updateGameInfoForCurrentMatch() {}
void PlayerInfoWiring::setOriginalGameInfo(const QList<KifGameInfoItem>&) {}
void PlayerInfoWiring::updateGameInfoPlayerNames(const QString&, const QString&) {}
void PlayerInfoWiring::setGameInfoForMatchStart(const QDateTime&, const QString&, const QString&, const QString&, bool, qint64, qint64, qint64) {}
void PlayerInfoWiring::updateGameInfoWithEndTime(const QDateTime&) {}
void PlayerInfoWiring::updateGameInfoWithTimeControl(bool, qint64, qint64, qint64) {}
void PlayerInfoWiring::ensureGameInfoController() {}
void PlayerInfoWiring::setTabWidget(QTabWidget*) {}
void PlayerInfoWiring::setAnalysisTab(EngineAnalysisTab*) {}
void PlayerInfoWiring::applyPlayersNamesForMode() {}
void PlayerInfoWiring::applyEngineNamesToLogModels() {}
void PlayerInfoWiring::applySecondEngineVisibility() {}
void PlayerInfoWiring::resolveNamesAndSetupGameInfo(const QString&, const QString&, const QString&, const QString&, int, const QString&, const TimeControlInfo&) {}
void PlayerInfoWiring::resolveNamesWithTimeController(const QString&, const QString&, const QString&, const QString&, int, const QString&, TimeControlController*) {}
void PlayerInfoWiring::onSetPlayersNames(const QString&, const QString&) {}
void PlayerInfoWiring::onSetEngineNames(const QString&, const QString&) {}
void PlayerInfoWiring::onPlayerNamesResolved(const QString&, const QString&, const QString&, const QString&, int) {}

void PlayerInfoWiring::onMenuPlayerNamesResolved(const QString&, const QString&,
                                                  const QString&, const QString&, int)
{
    TestTracker::onMenuPlayerNamesResolvedCalled = true;
}

void PlayerInfoWiring::ensurePlayerInfoController() {}
void PlayerInfoWiring::populateDefaultGameInfo() {}

// ============================================================
// KifuNavigationCoordinator スタブ（TestTracker 連動）
// ============================================================

KifuNavigationCoordinator::KifuNavigationCoordinator(QObject* parent) : QObject(parent) {}
void KifuNavigationCoordinator::updateDeps(const Deps&) {}
void KifuNavigationCoordinator::navigateToRow(int) {}
void KifuNavigationCoordinator::syncBoardAndHighlightsAtRow(int) {}
void KifuNavigationCoordinator::handleBranchNodeHandled(int, const QString&, int, int, const QString&) {}

void KifuNavigationCoordinator::selectKifuRow(int row)
{
    TestTracker::selectKifuRowCalled = true;
    TestTracker::selectKifuRowValue = row;
}

void KifuNavigationCoordinator::syncNavStateToPly(int) {}
void KifuNavigationCoordinator::beginBranchNavGuard() {}
void KifuNavigationCoordinator::clearBranchNavGuard() {}

// ============================================================
// BranchNavigationWiring スタブ（TestTracker 連動）
// ============================================================

BranchNavigationWiring::BranchNavigationWiring(QObject* parent) : QObject(parent) {}
void BranchNavigationWiring::updateDeps(const Deps&) {}
void BranchNavigationWiring::onBranchTreeBuilt() {}
void BranchNavigationWiring::onBranchNodeActivated(int, int) {}

void BranchNavigationWiring::onBranchTreeResetForNewGame()
{
    TestTracker::onBranchTreeResetForNewGameCalled = true;
}

// ============================================================
// UiStatePolicyManager スタブ
// ============================================================

UiStatePolicyManager::UiStatePolicyManager(QObject* parent) : QObject(parent) {}
void UiStatePolicyManager::applyState(AppState) {}
void UiStatePolicyManager::transitionToIdle() {}
void UiStatePolicyManager::transitionToDuringGame() {}
void UiStatePolicyManager::transitionToDuringAnalysis() {}
void UiStatePolicyManager::transitionToDuringCsaGame() {}
void UiStatePolicyManager::transitionToDuringTsumeSearch() {}
void UiStatePolicyManager::transitionToDuringConsideration() {}
void UiStatePolicyManager::transitionToDuringPositionEdit() {}
void UiStatePolicyManager::enableNavigationIfAllowed() {}

// ============================================================
// EvaluationGraphController スタブ
// ============================================================

EvaluationGraphController::EvaluationGraphController(QObject* parent) : QObject(parent) {}
EvaluationGraphController::~EvaluationGraphController() = default;
void EvaluationGraphController::setEvalChart(EvaluationChartWidget*) {}
void EvaluationGraphController::setMatchCoordinator(MatchCoordinator*) {}
void EvaluationGraphController::setSfenRecord(QStringList*) {}
void EvaluationGraphController::setEngine1Name(const QString&) {}
void EvaluationGraphController::setEngine2Name(const QString&) {}
void EvaluationGraphController::clearScores() {}
void EvaluationGraphController::trimToPly(int) {}
const QList<int>& EvaluationGraphController::scores() const { static QList<int> s; return s; }
QList<int>& EvaluationGraphController::scoresRef() { static QList<int> s; return s; }
void EvaluationGraphController::removeLastP1Score() {}
void EvaluationGraphController::removeLastP2Score() {}
void EvaluationGraphController::setCurrentPly(int) {}
void EvaluationGraphController::redrawEngine1Graph(int) {}
void EvaluationGraphController::redrawEngine2Graph(int) {}
void EvaluationGraphController::doRedrawEngine1Graph() {}
void EvaluationGraphController::doRedrawEngine2Graph() {}

// ============================================================
// TimeControlController スタブ
// ============================================================

TimeControlController::TimeControlController(QObject* parent) : QObject(parent) {}
TimeControlController::~TimeControlController() = default;
void TimeControlController::ensureClock() {}
ShogiClock* TimeControlController::clock() const { return nullptr; }
void TimeControlController::setMatchCoordinator(MatchCoordinator*) {}
void TimeControlController::setTimeDisplayPresenter(TimeDisplayPresenter*) {}
void TimeControlController::applyTimeControl(const GameStartCoordinator::TimeControl&, MatchCoordinator*, const QString&, const QString&, ShogiView*) {}
void TimeControlController::saveTimeControlSettings(bool, qint64, qint64, qint64) {}
const TimeControlController::TimeControlSettings& TimeControlController::settings() const { static TimeControlSettings s; return s; }
bool TimeControlController::hasTimeControl() const { return false; }
qint64 TimeControlController::baseTimeMs() const { return 0; }
qint64 TimeControlController::byoyomiMs() const { return 0; }
qint64 TimeControlController::incrementMs() const { return 0; }
qint64 TimeControlController::getRemainingMs(int) const { return 0; }
qint64 TimeControlController::getIncrementMs(int) const { return 0; }
qint64 TimeControlController::getByoyomiMs() const { return 0; }
void TimeControlController::recordGameStartTime() {}
void TimeControlController::recordGameEndTime() {}
QDateTime TimeControlController::gameStartDateTime() const { return {}; }
QDateTime TimeControlController::gameEndDateTime() const { return {}; }
void TimeControlController::clearGameStartTime() {}
void TimeControlController::clearGameEndTime() {}
void TimeControlController::onClockPlayer1TimeOut() {}
void TimeControlController::onClockPlayer2TimeOut() {}

// ============================================================
// TimeDisplayPresenter スタブ
// ============================================================

TimeDisplayPresenter::TimeDisplayPresenter(ShogiView*, QObject* parent) : QObject(parent) {}
void TimeDisplayPresenter::setClock(ShogiClock*) {}
void TimeDisplayPresenter::onMatchTimeUpdated(qint64, qint64, bool, qint64) {}

// ============================================================
// BoardInteractionController スタブ
// ============================================================

BoardInteractionController::BoardInteractionController(ShogiView*, ShogiGameController*, QObject* parent)
    : QObject(parent) {}
// setMode, mode, setMoveInputEnabled, isMoveInputEnabled are inline in header
void BoardInteractionController::clearSelectionHighlight() {}
void BoardInteractionController::showMoveHighlights(const QPoint&, const QPoint&) {}
void BoardInteractionController::clearAllHighlights() {}
void BoardInteractionController::cancelPendingClick() {}
void BoardInteractionController::onLeftClick(const QPoint&) {}
void BoardInteractionController::onRightClick(const QPoint&) {}
void BoardInteractionController::onMoveApplied(const QPoint&, const QPoint&, bool) {}
void BoardInteractionController::onHighlightsCleared() {}

// ============================================================
// ShogiClock スタブ
// ============================================================

ShogiClock::ShogiClock(QObject* parent) : QObject(parent) {}
void ShogiClock::setLoseOnTimeout(bool) {}
void ShogiClock::setPlayerTimes(int, int, int, int, int, int, bool) {}
void ShogiClock::setCurrentPlayer(int) {}

// ============================================================
// ShogiView スタブ
// ============================================================

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

// ============================================================
// ShogiGameController スタブ
// ============================================================

ShogiGameController::ShogiGameController(QObject* parent) : QObject(parent) {}
ShogiGameController::~ShogiGameController() = default;
ShogiBoard* ShogiGameController::board() const { return nullptr; }
void ShogiGameController::setCurrentPlayer(const Player) {}
void ShogiGameController::setPromote(bool) {}
void ShogiGameController::newGame(QString&) {}

// ============================================================
// ShogiBoard スタブ
// ============================================================

ShogiBoard::ShogiBoard(int ranks, int files, QObject* parent)
    : QObject(parent), m_ranks(ranks), m_files(files)
{
    m_boardData.fill(Piece::None, ranks * files);
}
const QVector<Piece>& ShogiBoard::boardData() const { return m_boardData; }

// ============================================================
// MatchCoordinatorHooksFactory スタブ
// ============================================================

MatchCoordinator::Hooks MatchCoordinatorHooksFactory::buildHooks(const HookDeps&)
{
    return {};
}

MatchCoordinator::UndoHooks MatchCoordinatorHooksFactory::buildUndoHooks(const UndoDeps&)
{
    return {};
}

// ============================================================
// EngineLifecycleManager スタブ
// ============================================================

EngineLifecycleManager::EngineLifecycleManager(QObject* parent) : QObject(parent) {}
void EngineLifecycleManager::setRefs(const Refs&) {}
void EngineLifecycleManager::setHooks(const Hooks&) {}
void EngineLifecycleManager::initializeAndStartEngineFor(Player, const QString&, const QString&) {}
void EngineLifecycleManager::destroyEngine(int, bool) {}
void EngineLifecycleManager::destroyEngines(bool) {}
void EngineLifecycleManager::initEnginesForEvE(const QString&, const QString&) {}
Usi* EngineLifecycleManager::usi1() const { return nullptr; }
Usi* EngineLifecycleManager::usi2() const { return nullptr; }
void EngineLifecycleManager::setUsi1(Usi*) {}
void EngineLifecycleManager::setUsi2(Usi*) {}
void EngineLifecycleManager::updateUsiPtrs(Usi*, Usi*) {}
bool EngineLifecycleManager::isShutdownInProgress() const { return false; }
void EngineLifecycleManager::setShutdownInProgress(bool) {}
EngineLifecycleManager::EngineModelPair EngineLifecycleManager::ensureEngineModels(int) { return {}; }
void EngineLifecycleManager::setModelPtrs(UsiCommLogModel*, ShogiEngineThinkingModel*, UsiCommLogModel*, ShogiEngineThinkingModel*) {}
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
void EngineLifecycleManager::sendRawTo(Usi*, const QString&) {}

// ============================================================
// MatchTimekeeper スタブ
// ============================================================

MatchTimekeeper::MatchTimekeeper(QObject* parent) : QObject(parent) {}
void MatchTimekeeper::setRefs(const Refs&) {}
void MatchTimekeeper::setHooks(const Hooks&) {}
void MatchTimekeeper::setClock(ShogiClock*) {}
ShogiClock* MatchTimekeeper::clock() const { return nullptr; }
const ShogiClock* MatchTimekeeper::clockConst() const { return nullptr; }
void MatchTimekeeper::setTimeControlConfig(bool, int, int, int, int, bool) {}
const MatchTimekeeper::TimeControl& MatchTimekeeper::timeControl() const { static TimeControl tc; return tc; }
MatchTimekeeper::GoTimes MatchTimekeeper::computeGoTimes() const { return {}; }
void MatchTimekeeper::computeGoTimesForUSI(qint64&, qint64&) const {}
void MatchTimekeeper::refreshGoTimes() {}
void MatchTimekeeper::markTurnEpochNowFor(Player, qint64) {}
qint64 MatchTimekeeper::turnEpochFor(Player) const { return 0; }
void MatchTimekeeper::recomputeClockSnapshot(QString&, QString&, QString&) const {}
void MatchTimekeeper::pokeTimeUpdateNow() {}
void MatchTimekeeper::onClockTick() {}
void MatchTimekeeper::wireClock() {}
void MatchTimekeeper::unwireClock() {}
void MatchTimekeeper::emitTimeUpdateFromClock() {}

// ============================================================
// MatchUndoHandler スタブ
// ============================================================

void MatchUndoHandler::setRefs(const Refs&) {}
void MatchUndoHandler::setUndoBindings(const UndoRefs&, const UndoHooks&) {}
bool MatchUndoHandler::undoTwoPlies() { return false; }
bool MatchUndoHandler::isStandardStartposSfen(const QString&) { return false; }
bool MatchUndoHandler::tryRemoveLastItems(QObject*, int) { return false; }
QString MatchUndoHandler::buildBasePositionUpToHands(const QString&, int, const QString&) { return {}; }

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
// GameStartOrchestrator スタブ
// ============================================================

void GameStartOrchestrator::setRefs(const Refs&) {}
void GameStartOrchestrator::setHooks(const Hooks&) {}
void GameStartOrchestrator::configureAndStart(const StartOptions&) {}
GameStartOrchestrator::StartOptions GameStartOrchestrator::buildStartOptions(PlayMode, const QString&, const QStringList*, const StartGameDialog*) { return {}; }
void GameStartOrchestrator::ensureHumanAtBottomIfApplicable(const StartGameDialog*, bool) {}
void GameStartOrchestrator::prepareAndStartGame(PlayMode, const QString&, const QStringList*, const StartGameDialog*, bool) {}
GameStartOrchestrator::HistorySearchResult GameStartOrchestrator::syncAndSearchGameHistory(const QString&) { return {}; }
void GameStartOrchestrator::applyStartOptionsAndHooks(const StartOptions&) {}
void GameStartOrchestrator::buildPositionStringsFromHistory(const StartOptions&, const QString&, const HistorySearchResult&) {}

// ============================================================
// GameEndHandler スタブ
// ============================================================

GameEndHandler::GameEndHandler(QObject* parent) : QObject(parent) {}
void GameEndHandler::setRefs(const Refs&) {}
void GameEndHandler::setHooks(const Hooks&) {}
void GameEndHandler::handleResign() {}
void GameEndHandler::handleEngineResign(int) {}
void GameEndHandler::handleEngineWin(int) {}
void GameEndHandler::handleNyugyokuDeclaration(Player, bool, bool) {}
void GameEndHandler::handleBreakOff() {}
void GameEndHandler::handleMaxMovesJishogi() {}
bool GameEndHandler::checkAndHandleSennichite() { return false; }
void GameEndHandler::handleSennichite() {}
void GameEndHandler::handleOuteSennichite(bool) {}
void GameEndHandler::appendBreakOffLineAndMark() {}
void GameEndHandler::appendGameOverLineAndMark(Cause, Player) {}
void GameEndHandler::displayResultsAndUpdateGui(const GameEndInfo&) {}
void GameEndHandler::sendRawToEngineHelper(Usi*, const QString&) {}

// ============================================================
// ShogiViewLayout / ShogiViewInteraction スタブ
// ============================================================

#include "shogiviewlayout.h"
#include "shogiviewinteraction.h"

ShogiViewLayout::ShogiViewLayout() {}
ShogiViewInteraction::ShogiViewInteraction() {}

// ============================================================
// MOC 出力のインクルード
// ============================================================

#include "moc_matchcoordinator.cpp"
#include "moc_gamestartcoordinator.cpp"
#include "moc_gamesessionorchestrator.cpp"
#include "moc_mainwindowappearancecontroller.cpp"
#include "moc_playerinfowiring.cpp"
#include "moc_kifunavigationcoordinator.cpp"
#include "moc_branchnavigationwiring.cpp"
#include "moc_uistatepolicymanager.cpp"
#include "moc_evaluationgraphcontroller.cpp"
#include "moc_timecontrolcontroller.cpp"
#include "moc_timedisplaypresenter.cpp"
#include "moc_boardinteractioncontroller.cpp"
#include "moc_shogiclock.cpp"
#include "moc_shogiview.cpp"
#include "moc_shogigamecontroller.cpp"
#include "moc_shogiboard.cpp"
#include "moc_enginelifecyclemanager.cpp"
#include "moc_matchtimekeeper.cpp"
#include "moc_analysissessionhandler.cpp"
#include "moc_gameendhandler.cpp"
