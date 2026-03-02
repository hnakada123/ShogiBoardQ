/// @file test_stubs_analysisflow.cpp
/// @brief AnalysisFlowController テスト用スタブ
///
/// analysisflowcontroller.cpp / analysiscoordinator.cpp が参照する
/// 外部シンボルのスタブ実装を提供する。

#include <QObject>
#include <QProcess>
#include <QSettings>
#include <QList>
#include <QMap>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "usi.h"
#include "usimatchhandler.h"
#include "kifuanalysisdialog.h"
#include "analysisresultspresenter.h"
#include "kifurecordlistmodel.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisresultsdisplay.h"
#include "kifudisplay.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "engineprocessmanager.h"
#include "thinkinginfopresenter.h"
#include "shogiengineinfoparser.h"
#include "pvboarddialog.h"
#include "branchtreemanager.h"
#include "settingscommon.h"
#include "shogiinforecord.h"

// === Usi スタブ ===
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
void Usi::setClonedBoardData(const QList<QChar>&) {}
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

// === KifuAnalysisDialog スタブ ===
KifuAnalysisDialog::KifuAnalysisDialog(QWidget* parent)
    : QDialog(parent), ui(nullptr), m_fontHelper({10, 8, 24, 2, nullptr}) {}
QList<KifuAnalysisDialog::Engine> KifuAnalysisDialog::engineList() const
{
    return { { QStringLiteral("TestEngine"), QStringLiteral("/usr/bin/test_engine") } };
}
bool KifuAnalysisDialog::initPosition() const { return true; }
int KifuAnalysisDialog::startPly() const { return m_startPly; }
int KifuAnalysisDialog::endPly() const { return m_endPly; }
void KifuAnalysisDialog::setMaxPly(int maxPly) { m_maxPly = maxPly; m_endPly = maxPly; }
int KifuAnalysisDialog::byoyomiSec() const { return 1; }
int KifuAnalysisDialog::engineNumber() const { return 0; }
QString KifuAnalysisDialog::engineName() const { return QStringLiteral("TestEngine"); }
void KifuAnalysisDialog::showEngineSettingsDialog() {}
void KifuAnalysisDialog::processEngineSettings() {}
void KifuAnalysisDialog::onRangeRadioToggled(bool) {}
void KifuAnalysisDialog::onStartPlyChanged(int) {}
void KifuAnalysisDialog::onFontIncrease() {}
void KifuAnalysisDialog::onFontDecrease() {}

// === AnalysisResultsPresenter スタブ ===
AnalysisResultsPresenter::AnalysisResultsPresenter(QObject* parent) : QObject(parent), m_reflowTimer(nullptr) {}
void AnalysisResultsPresenter::setDockWidget(QDockWidget*) {}
QWidget* AnalysisResultsPresenter::containerWidget() { return nullptr; }
void AnalysisResultsPresenter::showWithModel(KifuAnalysisListModel*) {}
void AnalysisResultsPresenter::setStopButtonEnabled(bool) {}
void AnalysisResultsPresenter::showAnalysisComplete(int) {}
void AnalysisResultsPresenter::reflowNow() {}
void AnalysisResultsPresenter::onModelReset() {}
void AnalysisResultsPresenter::onRowsInserted(const QModelIndex&, int, int) {}
void AnalysisResultsPresenter::onDataChanged(const QModelIndex&, const QModelIndex&, const QList<int>&) {}
void AnalysisResultsPresenter::onLayoutChanged() {}
void AnalysisResultsPresenter::onScrollRangeChanged(int, int) {}
void AnalysisResultsPresenter::onTableClicked(const QModelIndex&) {}
void AnalysisResultsPresenter::onTableSelectionChanged(const QModelIndex&, const QModelIndex&) {}
void AnalysisResultsPresenter::increaseFontSize() {}
void AnalysisResultsPresenter::decreaseFontSize() {}
void AnalysisResultsPresenter::saveWindowSize() {}
void AnalysisResultsPresenter::buildUi(KifuAnalysisListModel*) {}
void AnalysisResultsPresenter::setupHeaderConfiguration() {}
void AnalysisResultsPresenter::connectModelSignals(KifuAnalysisListModel*) {}
void AnalysisResultsPresenter::restoreFontSize() {}

// === KifuRecordListModel スタブ ===
KifuRecordListModel::KifuRecordListModel(QObject* parent) : AbstractListModel(parent) {}
int KifuRecordListModel::columnCount(const QModelIndex&) const { return 2; }
QVariant KifuRecordListModel::data(const QModelIndex&, int) const { return {}; }
QVariant KifuRecordListModel::headerData(int, Qt::Orientation, int) const { return {}; }
bool KifuRecordListModel::prependItem(KifuDisplay*) { return false; }
bool KifuRecordListModel::removeLastItem() { return false; }
bool KifuRecordListModel::removeLastItems(int) { return false; }
void KifuRecordListModel::clearAllItems() { AbstractListModel::clearAllItems(); }
void KifuRecordListModel::setBranchPlyMarks(const QSet<int>&) {}
void KifuRecordListModel::setCurrentHighlightRow(int) {}

// === KifuAnalysisListModel スタブ ===
KifuAnalysisListModel::KifuAnalysisListModel(QObject* parent) : AbstractListModel(parent) {}
int KifuAnalysisListModel::columnCount(const QModelIndex&) const { return 4; }
QVariant KifuAnalysisListModel::data(const QModelIndex&, int) const { return {}; }
QVariant KifuAnalysisListModel::headerData(int, Qt::Orientation, int) const { return {}; }

// === KifuAnalysisResultsDisplay スタブ ===
KifuAnalysisResultsDisplay::KifuAnalysisResultsDisplay(QObject* parent) : QObject(parent) {}
KifuAnalysisResultsDisplay::KifuAnalysisResultsDisplay(
    const QString& currentMove, const QString& evaluationValue,
    const QString& evaluationDifference, const QString& principalVariation, QObject* parent)
    : QObject(parent), m_currentMove(currentMove), m_evaluationValue(evaluationValue),
      m_evaluationDifference(evaluationDifference), m_principalVariation(principalVariation) {}
QString KifuAnalysisResultsDisplay::currentMove() const { return m_currentMove; }
QString KifuAnalysisResultsDisplay::evaluationValue() const { return m_evaluationValue; }
QString KifuAnalysisResultsDisplay::evaluationDifference() const { return m_evaluationDifference; }
QString KifuAnalysisResultsDisplay::principalVariation() const { return m_principalVariation; }

// === KifuDisplay スタブ ===
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

// === ShogiBoard スタブ ===
ShogiBoard::ShogiBoard(int ranks, int files, QObject* parent)
    : QObject(parent), m_ranks(ranks), m_files(files)
{
    m_boardData.fill(Piece::None, ranks * files);
}
const QList<Piece>& ShogiBoard::boardData() const { return m_boardData; }
std::optional<SfenComponents> ShogiBoard::parseSfen(const QString&) { return std::nullopt; }
void ShogiBoard::setSfen(const QString&)
{
    // スタブ: 81マスのダミーデータを設定
    m_boardData.fill(Piece::None, m_ranks * m_files);
}
Piece ShogiBoard::getPieceCharacter(int, int) { return Piece::None; }
const QMap<Piece, int>& ShogiBoard::getPieceStand() const { return m_pieceStand; }
Turn ShogiBoard::currentPlayer() const { return Turn::Black; }
QString ShogiBoard::convertBoardToSfen() { return {}; }
QString ShogiBoard::convertStandToSfen() const { return {}; }

// === ShogiGameController スタブ ===
ShogiGameController::ShogiGameController(QObject* parent) : QObject(parent) {}
ShogiGameController::~ShogiGameController() = default;
ShogiBoard* ShogiGameController::board() const { return nullptr; }
void ShogiGameController::setCurrentPlayer(const Player) {}
void ShogiGameController::setPromote(bool) {}
void ShogiGameController::newGame(QString&) {}

// === UsiCommLogModel スタブ ===
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

// === ShogiEngineThinkingModel スタブ ===
ShogiEngineThinkingModel::ShogiEngineThinkingModel(QObject* parent) : AbstractListModel(parent) {}
int ShogiEngineThinkingModel::columnCount(const QModelIndex&) const { return 6; }
QVariant ShogiEngineThinkingModel::data(const QModelIndex&, int) const { return {}; }
QVariant ShogiEngineThinkingModel::headerData(int, Qt::Orientation, int) const { return {}; }
QString ShogiEngineThinkingModel::usiPvAt(int) const { return {}; }
const ShogiInfoRecord* ShogiEngineThinkingModel::recordAt(int) const { return nullptr; }
int ShogiEngineThinkingModel::findRowByMultipv(int) const { return -1; }
void ShogiEngineThinkingModel::updateByMultipv(ShogiInfoRecord*, int) {}

// === ShogiInfoRecord スタブ ===
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

// === EngineProcessManager スタブ ===
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

// === ThinkingInfoPresenter スタブ ===
ThinkingInfoPresenter::ThinkingInfoPresenter(QObject* parent) : QObject(parent) {}
ThinkingInfoPresenter::~ThinkingInfoPresenter() = default;
void ThinkingInfoPresenter::setGameController(ShogiGameController*) {}
void ThinkingInfoPresenter::setAnalysisMode(bool) {}
void ThinkingInfoPresenter::setPreviousMove(int, int) {}
void ThinkingInfoPresenter::setClonedBoardData(const QList<QChar>&) {}
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

// === ShogiEngineInfoParser スタブ ===
ShogiEngineInfoParser::ShogiEngineInfoParser() {}

// === PvBoardDialog スタブ ===
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

// === ShogiViewLayout / ShogiViewInteraction スタブ ===
#include "shogiviewlayout.h"
#include "shogiviewinteraction.h"
ShogiViewLayout::ShogiViewLayout() {}
ShogiViewInteraction::ShogiViewInteraction() {}

// === ShogiView スタブ ===
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

// === ElideLabel スタブ ===
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

// === UsiProtocolHandler スタブ ===
UsiProtocolHandler::UsiProtocolHandler(QObject* parent) : QObject(parent) {}
UsiProtocolHandler::~UsiProtocolHandler() = default;
void UsiProtocolHandler::onDataReceived(const QString&) {}

// === BranchTreeManager スタブ ===
BranchTreeManager::BranchTreeManager(QObject* parent) : QObject(parent) {}
BranchTreeManager::~BranchTreeManager() = default;
void BranchTreeManager::highlightBranchTreeAt(int, int, bool) {}
bool BranchTreeManager::eventFilter(QObject* obj, QEvent* ev) { return QObject::eventFilter(obj, ev); }

// === SettingsCommon スタブ ===
namespace SettingsCommon {
QString settingsFilePath() { return QStringLiteral("/tmp/tst_analysisflow_stub.ini"); }
QSettings& openSettings() { static QSettings s(settingsFilePath(), QSettings::IniFormat); return s; }
}

// === MOC出力のインクルード ===
// 直接参照されるクラス
#include "moc_usi.cpp"
#include "moc_kifuanalysisdialog.cpp"
#include "moc_analysisresultspresenter.cpp"
#include "moc_kifurecordlistmodel.cpp"
#include "moc_kifuanalysislistmodel.cpp"
#include "moc_kifuanalysisresultsdisplay.cpp"
#include "moc_kifudisplay.cpp"
#include "moc_shogiboard.cpp"
#include "moc_shogigamecontroller.cpp"
#include "moc_usicommlogmodel.cpp"
#include "moc_shogienginethinkingmodel.cpp"
#include "moc_engineprocessmanager.cpp"
#include "moc_thinkinginfopresenter.cpp"
#include "moc_shogiengineinfoparser.cpp"
#include "moc_shogiinforecord.cpp"
#include "moc_branchtreemanager.cpp"
// usi.h / pvboarddialog.h 経由で間接参照されるクラス
#include "moc_usiprotocolhandler.cpp"
#include "moc_pvboarddialog.cpp"
#include "moc_shogiview.cpp"
#include "moc_elidelabel.cpp"
