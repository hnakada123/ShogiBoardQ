/// @file test_stubs_game_start_flow.cpp
/// @brief GameStartCoordinator テスト用スタブ
///
/// gamestartcoordinator.cpp が参照する外部シンボルのスタブ実装を提供する。

#include <QObject>
#include <QProcess>
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
#include "startgamedialog.h"
#include "kifurecordlistmodel.h"
#include "kifuloadcoordinator.h"
#include "kifudisplay.h"
#include "playernameservice.h"
#include "timecontrolutil.h"
#include "shogiboard.h"
#include "engineprocessmanager.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "usi.h"
#include "thinkinginfopresenter.h"
#include "shogiengineinfoparser.h"
#include "boardinteractioncontroller.h"
#include "settingsservice.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "recordpane.h"
#include "branchtreemanager.h"
#include "pvboarddialog.h"

// ============================================================
// MatchCoordinator スタブ
// ============================================================

// テスト用のフラグ: start() 呼び出し追跡
namespace TestTracker {
    bool configureAndStartCalled = false;
    bool startInitialEngineMoveIfNeededCalled = false;
    bool setTimeControlConfigCalled = false;
    bool refreshGoTimesCalled = false;
    bool clearGameOverStateCalled = false;
    MatchCoordinator::StartOptions lastStartOptions;
    bool lastUseByoyomi = false;
    int lastByoyomiMs1 = 0;
    int lastByoyomiMs2 = 0;
    int lastIncMs1 = 0;
    int lastIncMs2 = 0;
    bool lastLoseOnTimeout = false;

    void reset() {
        configureAndStartCalled = false;
        startInitialEngineMoveIfNeededCalled = false;
        setTimeControlConfigCalled = false;
        refreshGoTimesCalled = false;
        clearGameOverStateCalled = false;
        lastStartOptions = {};
        lastUseByoyomi = false;
        lastByoyomiMs1 = 0;
        lastByoyomiMs2 = 0;
        lastIncMs1 = 0;
        lastIncMs2 = 0;
        lastLoseOnTimeout = false;
    }
}

// StrategyContext 定義 (matchcoordinator.h で前方宣言されている)
#include "strategycontext.h"

MatchCoordinator::MatchCoordinator(const Deps& d, QObject* parent)
    : QObject(parent)
    , m_gc(d.gc)
    , m_clock(d.clock)
    , m_view(d.view)
    , m_usi1(d.usi1)
    , m_usi2(d.usi2)
    , m_hooks(d.hooks)
    , m_comm1(d.comm1)
    , m_think1(d.think1)
    , m_comm2(d.comm2)
    , m_think2(d.think2)
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

void MatchCoordinator::configureAndStart(const StartOptions& opt)
{
    TestTracker::configureAndStartCalled = true;
    TestTracker::lastStartOptions = opt;
    m_playMode = opt.mode;
}

void MatchCoordinator::startInitialEngineMoveIfNeeded()
{
    TestTracker::startInitialEngineMoveIfNeededCalled = true;
}

void MatchCoordinator::setTimeControlConfig(bool useByoyomi,
                                            int byoyomiMs1, int byoyomiMs2,
                                            int incMs1, int incMs2,
                                            bool loseOnTimeout)
{
    TestTracker::setTimeControlConfigCalled = true;
    TestTracker::lastUseByoyomi = useByoyomi;
    TestTracker::lastByoyomiMs1 = byoyomiMs1;
    TestTracker::lastByoyomiMs2 = byoyomiMs2;
    TestTracker::lastIncMs1 = incMs1;
    TestTracker::lastIncMs2 = incMs2;
    TestTracker::lastLoseOnTimeout = loseOnTimeout;
    m_tc.useByoyomi = useByoyomi;
    m_tc.byoyomiMs1 = byoyomiMs1;
    m_tc.byoyomiMs2 = byoyomiMs2;
    m_tc.incMs1 = incMs1;
    m_tc.incMs2 = incMs2;
    m_tc.loseOnTimeout = loseOnTimeout;
}

const MatchCoordinator::TimeControl& MatchCoordinator::timeControl() const
{
    return m_tc;
}

void MatchCoordinator::refreshGoTimes()
{
    TestTracker::refreshGoTimesCalled = true;
}

void MatchCoordinator::clearGameOverState()
{
    TestTracker::clearGameOverStateCalled = true;
    m_gameOver = {};
}

ShogiClock* MatchCoordinator::clock() { return m_clock; }
const ShogiClock* MatchCoordinator::clock() const { return m_clock; }
void MatchCoordinator::setClock(ShogiClock* c) { m_clock = c; }
PlayMode MatchCoordinator::playMode() const { return m_playMode; }
void MatchCoordinator::setPlayMode(PlayMode m) { m_playMode = m; }
void MatchCoordinator::pokeTimeUpdateNow() {}

MatchCoordinator::StartOptions MatchCoordinator::buildStartOptions(
    PlayMode mode, const QString& startSfenStr,
    const QStringList* /*sfenRecord*/, const StartGameDialog* /*dlg*/) const
{
    StartOptions opt;
    opt.mode = mode;
    opt.sfenStart = startSfenStr;
    return opt;
}

void MatchCoordinator::ensureHumanAtBottomIfApplicable(const StartGameDialog*, bool) {}
void MatchCoordinator::initializePositionStringsForStart(const QString&) {}
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
void MatchCoordinator::handleTimeUpdated() {}
void MatchCoordinator::handlePlayerTimeOut(int) {}
void MatchCoordinator::handleGameEnded() {}
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
void MatchCoordinator::setUndoBindings(const UndoRefs&, const UndoHooks&) {}
bool MatchCoordinator::undoTwoPlies() { return false; }
void MatchCoordinator::initEnginesForEvE(const QString&, const QString&) {}
bool MatchCoordinator::engineThinkApplyMove(Usi*, QString&, QString&, QPoint*, QPoint*) { return false; }
bool MatchCoordinator::engineMoveOnce(Usi*, QString&, QString&, bool, int, QPoint*) { return false; }

// private methods
bool MatchCoordinator::tryRemoveLastItems(QObject*, int) { return false; }
void MatchCoordinator::setGameInProgressActions(bool) {}
void MatchCoordinator::updateTurnDisplay(Player) {}
MatchCoordinator::GoTimes MatchCoordinator::computeGoTimes() const { return {}; }
void MatchCoordinator::initPositionStringsFromSfen(const QString&) {}
void MatchCoordinator::wireResignToArbiter(Usi*, bool) {}
void MatchCoordinator::wireWinToArbiter(Usi*, bool) {}
void MatchCoordinator::emitTimeUpdateFromClock() {}
void MatchCoordinator::recomputeClockSnapshot(QString&, QString&, QString&) const {}
void MatchCoordinator::sendRawTo(Usi*, const QString&) {}
void MatchCoordinator::createAndStartModeStrategy(const StartOptions&) {}
void MatchCoordinator::ensureGameStartOrchestrator() {}
void MatchCoordinator::wireClock() {}
void MatchCoordinator::unwireClock() {}
void MatchCoordinator::ensureAnalysisSession() {}
void MatchCoordinator::ensureGameEndHandler() {}

// private slots
void MatchCoordinator::onEngine1Resign() {}
void MatchCoordinator::onEngine2Resign() {}
void MatchCoordinator::onEngine1Win() {}
void MatchCoordinator::onEngine2Win() {}
void MatchCoordinator::onClockTick() {}
void MatchCoordinator::onUsiError(const QString&) {}

// ============================================================
// AnalysisSessionHandler スタブ
// ============================================================

AnalysisSessionHandler::AnalysisSessionHandler(QObject* parent) : QObject(parent) {}
void AnalysisSessionHandler::setHooks(const Hooks&) {}
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
ShogiBoard* ShogiGameController::board() const { return nullptr; }
void ShogiGameController::setCurrentPlayer(const Player) {}
void ShogiGameController::setPromote(bool) {}
void ShogiGameController::newGame(QString&) {}

// ============================================================
// StartGameDialog スタブ
// ============================================================

StartGameDialog::StartGameDialog(QWidget* parent) : QDialog(parent), ui(nullptr) {}
StartGameDialog::~StartGameDialog() = default;
bool StartGameDialog::isHuman1() const { return m_isHuman1; }
bool StartGameDialog::isHuman2() const { return m_isHuman2; }
bool StartGameDialog::isEngine1() const { return m_isEngine1; }
bool StartGameDialog::isEngine2() const { return m_isEngine2; }
const QString& StartGameDialog::engineName1() const { return m_engineName1; }
const QString& StartGameDialog::engineName2() const { return m_engineName2; }
const QString& StartGameDialog::humanName1() const { return m_humanName1; }
const QString& StartGameDialog::humanName2() const { return m_humanName2; }
int StartGameDialog::engineNumber1() const { return m_engineNumber1; }
int StartGameDialog::engineNumber2() const { return m_engineNumber2; }
int StartGameDialog::basicTimeHour1() const { return m_basicTimeHour1; }
int StartGameDialog::basicTimeMinutes1() const { return m_basicTimeMinutes1; }
int StartGameDialog::byoyomiSec1() const { return m_byoyomiSec1; }
int StartGameDialog::addEachMoveSec1() const { return m_addEachMoveSec1; }
int StartGameDialog::basicTimeHour2() const { return m_basicTimeHour2; }
int StartGameDialog::basicTimeMinutes2() const { return m_basicTimeMinutes2; }
int StartGameDialog::byoyomiSec2() const { return m_byoyomiSec2; }
int StartGameDialog::addEachMoveSec2() const { return m_addEachMoveSec2; }
const QString& StartGameDialog::startingPositionName() const { return m_startingPositionName; }
int StartGameDialog::startingPositionNumber() const { return m_startingPositionNumber; }
void StartGameDialog::forceCurrentPositionSelection() { m_startingPositionNumber = 0; }
const QList<StartGameDialog::Engine>& StartGameDialog::getEngineList() const { return engineList; }
int StartGameDialog::maxMoves() const { return m_maxMoves; }
int StartGameDialog::consecutiveGames() const { return m_consecutiveGames; }
bool StartGameDialog::isShowHumanInFront() const { return m_isShowHumanInFront; }
bool StartGameDialog::isAutoSaveKifu() const { return m_isAutoSaveKifu; }
const QString& StartGameDialog::kifuSaveDir() const { return m_kifuSaveDir; }
bool StartGameDialog::isLoseOnTimeout() const { return m_isLoseOnTimeout; }
bool StartGameDialog::isSwitchTurnEachGame() const { return m_isSwitchTurnEachGame; }
int StartGameDialog::jishogiRule() const { return m_jishogiRule; }
void StartGameDialog::loadEngineConfigurations() {}
void StartGameDialog::populatePlayerComboBoxes() {}
void StartGameDialog::showEngineSettingsDialog(int) {}
void StartGameDialog::updatePlayerUI(int, int) {}
void StartGameDialog::connectSignalsAndSlots() {}
void StartGameDialog::loadGameSettings() {}
void StartGameDialog::updateGameSettingsFromDialog() {}
void StartGameDialog::onFirstPlayerSettingsClicked() {}
void StartGameDialog::onSecondPlayerSettingsClicked() {}
void StartGameDialog::swapSides() {}
void StartGameDialog::saveGameSettings() {}
void StartGameDialog::resetSettingsToDefault() {}
void StartGameDialog::handleByoyomiSecChanged(int) {}
void StartGameDialog::handleAddEachMoveSecChanged(int) {}
void StartGameDialog::increaseFontSize() {}
void StartGameDialog::decreaseFontSize() {}
void StartGameDialog::onPlayer1SelectionChanged(int) {}
void StartGameDialog::onPlayer2SelectionChanged(int) {}
void StartGameDialog::onSelectKifuDirClicked() {}
void StartGameDialog::updateConsecutiveGamesEnabled() {}
void StartGameDialog::applyFontSize(int) {}
void StartGameDialog::loadFontSizeSettings() {}
void StartGameDialog::saveFontSizeSettings() {}

// ============================================================
// KifuRecordListModel スタブ
// ============================================================

KifuRecordListModel::KifuRecordListModel(QObject* parent) : AbstractListModel(parent) {}
int KifuRecordListModel::columnCount(const QModelIndex&) const { return 2; }
QVariant KifuRecordListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || role != Qt::DisplayRole) return {};
    KifuDisplay* d = item(index.row());
    return d ? d->currentMove() : QVariant();
}
QVariant KifuRecordListModel::headerData(int, Qt::Orientation, int) const { return {}; }
bool KifuRecordListModel::prependItem(KifuDisplay* i)
{
    AbstractListModel::prependItem(i);
    return true;
}
bool KifuRecordListModel::removeLastItem() { return false; }
bool KifuRecordListModel::removeLastItems(int) { return false; }
void KifuRecordListModel::clearAllItems() { AbstractListModel::clearAllItems(); }
void KifuRecordListModel::setBranchPlyMarks(const QSet<int>&) {}
void KifuRecordListModel::setCurrentHighlightRow(int) {}

// ============================================================
// KifuDisplay スタブ
// ============================================================

KifuDisplay::KifuDisplay(QObject* parent) : QObject(parent) {}
KifuDisplay::KifuDisplay(const QString& currentMove, const QString& timeSpent, QObject* parent)
    : QObject(parent), m_currentMove(currentMove), m_timeSpent(timeSpent) {}
KifuDisplay::KifuDisplay(const QString& currentMove, const QString& timeSpent, const QString& comment, QObject* parent)
    : QObject(parent), m_currentMove(currentMove), m_timeSpent(timeSpent), m_comment(comment) {}
KifuDisplay::KifuDisplay(const QString& currentMove, const QString& timeSpent, const QString& comment, const QString& bookmark, QObject* parent)
    : QObject(parent), m_currentMove(currentMove), m_timeSpent(timeSpent), m_comment(comment), m_bookmark(bookmark) {}
QString KifuDisplay::currentMove() const { return m_currentMove; }
QString KifuDisplay::timeSpent() const { return m_timeSpent; }
QString KifuDisplay::comment() const { return m_comment; }
void KifuDisplay::setComment(const QString& comment) { m_comment = comment; }
QString KifuDisplay::bookmark() const { return m_bookmark; }
void KifuDisplay::setBookmark(const QString& bookmark) { m_bookmark = bookmark; }

// ============================================================
// KifuLoadCoordinator スタブ（最小限）
// ============================================================

// gamestartcoordinator.cpp は KifuLoadCoordinator::resetBranchTreeForNewGame() だけ呼ぶ
// コンストラクタは大量引数だがスタブ実装なので空
// 参照メンバの初期化にはダミーの static 変数を使用
// NOLINTBEGIN(misc-definitions-in-headers) - stub dummy variables for reference members
[[maybe_unused]] static QVector<ShogiMove> s_dummyGameMoves;
[[maybe_unused]] static QStringList s_dummyPositionStrList;
[[maybe_unused]] static int s_dummyActivePly = 0;
[[maybe_unused]] static int s_dummyCurrentSelectedPly = 0;
[[maybe_unused]] static int s_dummyCurrentMoveIndex = 0;
// NOLINTEND(misc-definitions-in-headers)

KifuLoadCoordinator::KifuLoadCoordinator(QVector<ShogiMove>& gameMoves, QStringList& positionStrList,
                                          int& activePly, int& currentSelectedPly, int& currentMoveIndex,
                                          QStringList*, QTableWidget*, QDockWidget*, QTabWidget*,
                                          RecordPane*, KifuRecordListModel*, KifuBranchListModel*,
                                          QObject* parent)
    : QObject(parent)
    , m_gameMoves(gameMoves)
    , m_positionStrList(positionStrList)
    , m_activePly(activePly)
    , m_currentSelectedPly(currentSelectedPly)
    , m_currentMoveIndex(currentMoveIndex) {}

void KifuLoadCoordinator::resetBranchTreeForNewGame() {}
void KifuLoadCoordinator::resetBranchContext() {}
void KifuLoadCoordinator::setBranchTreeManager(BranchTreeManager*) {}
void KifuLoadCoordinator::loadKifuFromFile(const QString&) {}
void KifuLoadCoordinator::loadJkfFromFile(const QString&) {}
void KifuLoadCoordinator::loadCsaFromFile(const QString&) {}
void KifuLoadCoordinator::loadKi2FromFile(const QString&) {}
void KifuLoadCoordinator::loadUsenFromFile(const QString&) {}
void KifuLoadCoordinator::loadUsiFromFile(const QString&) {}
bool KifuLoadCoordinator::loadKifuFromString(const QString&) { return false; }
bool KifuLoadCoordinator::loadPositionFromSfen(const QString&) { return false; }
bool KifuLoadCoordinator::loadPositionFromBod(const QString&) { return false; }
KifuLoadCoordinator::BranchRowDelegate::BranchRowDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}
KifuLoadCoordinator::BranchRowDelegate::~BranchRowDelegate() = default;
void KifuLoadCoordinator::BranchRowDelegate::paint(QPainter*, const QStyleOptionViewItem&,
                                                    const QModelIndex&) const {}

// ============================================================
// TimeControlUtil スタブ
// ============================================================

namespace TimeControlUtil {
void applyToClock(ShogiClock*, const GameStartCoordinator::TimeControl&,
                  const QString&, const QString&) {}
}

// ============================================================
// Usi スタブ（MatchCoordinator が参照）
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
// ThinkingInfoPresenter スタブ
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

// ============================================================
// ShogiEngineInfoParser スタブ
// ============================================================

ShogiEngineInfoParser::ShogiEngineInfoParser() {}

// ============================================================
// BoardInteractionController スタブ
// ============================================================

void BoardInteractionController::clearAllHighlights() {}

// ============================================================
// EvaluationChartWidget / EvaluationGraphController スタブ
// ============================================================

void EvaluationChartWidget::clearAll() {}
void EvaluationGraphController::clearScores() {}
void EvaluationGraphController::trimToPly(int) {}

// ============================================================
// RecordPane スタブ
// ============================================================

QTableView* RecordPane::kifuView() const { return nullptr; }

// ============================================================
// BranchTreeManager スタブ
// ============================================================

BranchTreeManager::BranchTreeManager(QObject* parent) : QObject(parent) {}
BranchTreeManager::~BranchTreeManager() = default;
void BranchTreeManager::highlightBranchTreeAt(int, int, bool) {}
bool BranchTreeManager::eventFilter(QObject* obj, QEvent* ev) { return QObject::eventFilter(obj, ev); }

// ============================================================
// PvBoardDialog スタブ
// ============================================================

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
// UsiProtocolHandler スタブ
// ============================================================

UsiProtocolHandler::UsiProtocolHandler(QObject* parent) : QObject(parent) {}
UsiProtocolHandler::~UsiProtocolHandler() = default;
void UsiProtocolHandler::onDataReceived(const QString&) {}

// ============================================================
// ElideLabel スタブ
// ============================================================

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

namespace SettingsService {
QString settingsFilePath() { return QStringLiteral("/tmp/tst_game_start_flow_stub.ini"); }
}

// ============================================================
// MOC出力のインクルード
// ============================================================

#include "moc_matchcoordinator.cpp"
#include "moc_shogiclock.cpp"
#include "moc_shogiview.cpp"
#include "moc_shogiboard.cpp"
#include "moc_shogigamecontroller.cpp"
#include "moc_startgamedialog.cpp"
#include "moc_kifurecordlistmodel.cpp"
#include "moc_kifudisplay.cpp"
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
#include "moc_kifuloadcoordinator.cpp"
#include "moc_analysissessionhandler.cpp"
