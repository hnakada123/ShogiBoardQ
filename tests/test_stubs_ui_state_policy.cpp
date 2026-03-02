/// @file test_stubs_ui_state_policy.cpp
/// @brief UiStatePolicyManager テスト用スタブ
///
/// uistatepolicymanager.cpp が参照する外部シンボルのスタブ実装を提供する。

#include <QObject>
#include <QWidget>
#include <QLabel>
#include <QPoint>
#include <QColor>
#include <QList>
#include <QString>

#include "recordpane.h"
#include "engineanalysistab.h"
#include "boardinteractioncontroller.h"
#include "branchtreemanager.h"
#include "shogiview.h"
#include "elidelabel.h"
#include "shogigamecontroller.h"
#include "shogiviewlayout.h"
#include "shogiviewinteraction.h"
#include "shogiboard.h"

// ============================================================
// RecordPane スタブ
// ============================================================

RecordPane::RecordPane(QWidget* parent) : QWidget(parent) {}
void RecordPane::setModels(KifuRecordListModel*, KifuBranchListModel*) {}
QTableView* RecordPane::kifuView() const { return nullptr; }
QTableView* RecordPane::branchView() const { return nullptr; }
void RecordPane::setArrowButtonsEnabled(bool) {}
void RecordPane::setKifuViewEnabled(bool) {}
void RecordPane::setNavigationEnabled(bool) {}
CommentTextAdapter* RecordPane::commentLabel() { return nullptr; }
QPushButton* RecordPane::backToMainButton() { return nullptr; }
void RecordPane::setupKifuSelectionAppearance() {}
void RecordPane::setupBranchViewSelectionAppearance() {}
// public slots
void RecordPane::setBranchCommentText(const QString&) {}
void RecordPane::setBranchCommentHtml(const QString&) {}
void RecordPane::onFontIncrease(bool) {}
void RecordPane::onFontDecrease(bool) {}
void RecordPane::onToggleTimeColumn(bool) {}
void RecordPane::onToggleBookmarkColumn(bool) {}
void RecordPane::onToggleCommentColumn(bool) {}
// private slots
void RecordPane::onKifuRowsInserted(const QModelIndex&, int, int) {}
void RecordPane::onKifuCurrentRowChanged(const QModelIndex&, const QModelIndex&) {}
void RecordPane::onBranchCurrentRowChanged(const QModelIndex&, const QModelIndex&) {}
void RecordPane::connectKifuCurrentRowChanged() {}

// ============================================================
// EngineAnalysisTab スタブ
// ============================================================

EngineAnalysisTab::EngineAnalysisTab(QWidget* parent) : QWidget(parent) {}
EngineAnalysisTab::~EngineAnalysisTab() = default;
void EngineAnalysisTab::buildUi() {}
QWidget* EngineAnalysisTab::createThinkingPage(QWidget*) { return nullptr; }
QWidget* EngineAnalysisTab::createConsiderationPage(QWidget*) { return nullptr; }
QWidget* EngineAnalysisTab::createUsiLogPage(QWidget*) { return nullptr; }
QWidget* EngineAnalysisTab::createCsaLogPage(QWidget*) { return nullptr; }
QWidget* EngineAnalysisTab::createCommentPage(QWidget*) { return nullptr; }
QWidget* EngineAnalysisTab::createBranchTreePage(QWidget*) { return nullptr; }
void EngineAnalysisTab::setModels(ShogiEngineThinkingModel*, ShogiEngineThinkingModel*,
                                   UsiCommLogModel*, UsiCommLogModel*) {}
QTabWidget* EngineAnalysisTab::tab() const { return nullptr; }
void EngineAnalysisTab::setBranchTreeRows(const QList<ResolvedRowLite>&) {}
void EngineAnalysisTab::highlightBranchTreeAt(int, int, bool) {}
void EngineAnalysisTab::setSecondEngineVisible(bool) {}
void EngineAnalysisTab::setEngine1ThinkingModel(ShogiEngineThinkingModel*) {}
void EngineAnalysisTab::setEngine2ThinkingModel(ShogiEngineThinkingModel*) {}
void EngineAnalysisTab::setCommentText(const QString&) {}
void EngineAnalysisTab::appendCsaLog(const QString&) {}
void EngineAnalysisTab::clearCsaLog() {}
void EngineAnalysisTab::clearUsiLog() {}
void EngineAnalysisTab::appendUsiLogStatus(const QString&) {}
BranchTreeManager* EngineAnalysisTab::branchTreeManager() { return nullptr; }
CommentEditorPanel* EngineAnalysisTab::commentEditor() { return nullptr; }
ConsiderationTabManager* EngineAnalysisTab::considerationTabManager() { return nullptr; }
EngineInfoWidget* EngineAnalysisTab::considerationInfo() const { return nullptr; }
QTableView* EngineAnalysisTab::considerationView() const { return nullptr; }
void EngineAnalysisTab::setConsiderationThinkingModel(ShogiEngineThinkingModel*) {}
void EngineAnalysisTab::switchToConsiderationTab() {}
void EngineAnalysisTab::switchToThinkingTab() {}
int EngineAnalysisTab::considerationMultiPV() const { return 1; }
void EngineAnalysisTab::setConsiderationMultiPV(int) {}
void EngineAnalysisTab::clearThinkingViewSelection(int) {}
void EngineAnalysisTab::setConsiderationTimeLimit(bool, int) {}
void EngineAnalysisTab::setConsiderationEngineName(const QString&) {}
void EngineAnalysisTab::startElapsedTimer() {}
void EngineAnalysisTab::stopElapsedTimer() {}
void EngineAnalysisTab::resetElapsedTimer() {}
void EngineAnalysisTab::setConsiderationRunning(bool) {}
void EngineAnalysisTab::loadEngineList() {}
int EngineAnalysisTab::selectedEngineIndex() const { return 0; }
QString EngineAnalysisTab::selectedEngineName() const { return {}; }
bool EngineAnalysisTab::isUnlimitedTime() const { return false; }
int EngineAnalysisTab::byoyomiSec() const { return 0; }
void EngineAnalysisTab::loadConsiderationTabSettings() {}
void EngineAnalysisTab::saveConsiderationTabSettings() {}
bool EngineAnalysisTab::isShowArrowsChecked() const { return false; }
bool EngineAnalysisTab::hasUnsavedComment() const { return false; }
bool EngineAnalysisTab::confirmDiscardUnsavedComment() { return true; }
void EngineAnalysisTab::clearCommentDirty() {}
void EngineAnalysisTab::setBranchTreeClickEnabled(bool) {}
bool EngineAnalysisTab::isBranchTreeClickEnabled() const { return true; }
// public slots
void EngineAnalysisTab::setAnalysisVisible(bool) {}
void EngineAnalysisTab::setCommentHtml(const QString&) {}
void EngineAnalysisTab::setCurrentMoveIndex(int) {}
int EngineAnalysisTab::currentMoveIndex() const { return 0; }

// ============================================================
// BoardInteractionController スタブ
// ============================================================

BoardInteractionController::BoardInteractionController(ShogiView*, ShogiGameController*, QObject* parent)
    : QObject(parent) {}
void BoardInteractionController::clearSelectionHighlight() {}
void BoardInteractionController::showMoveHighlights(const QPoint&, const QPoint&) {}
void BoardInteractionController::clearAllHighlights() {}
void BoardInteractionController::cancelPendingClick() {}
// public slots
void BoardInteractionController::onLeftClick(const QPoint&) {}
void BoardInteractionController::onRightClick(const QPoint&) {}
void BoardInteractionController::onMoveApplied(const QPoint&, const QPoint&, bool) {}
void BoardInteractionController::onHighlightsCleared() {}

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
// ShogiViewLayout / ShogiViewInteraction スタブ
// ============================================================

ShogiViewLayout::ShogiViewLayout() {}
ShogiViewInteraction::ShogiViewInteraction() {}

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
const QList<Piece>& ShogiBoard::boardData() const { return m_boardData; }

// ============================================================
// BranchTreeManager スタブ
// ============================================================

BranchTreeManager::BranchTreeManager(QObject* parent) : QObject(parent) {}
BranchTreeManager::~BranchTreeManager() = default;
bool BranchTreeManager::eventFilter(QObject* obj, QEvent* ev)
{
    return QObject::eventFilter(obj, ev);
}

// ============================================================
// MOC出力のインクルード
// ============================================================

#include "moc_recordpane.cpp"
#include "moc_engineanalysistab.cpp"
#include "moc_boardinteractioncontroller.cpp"
#include "moc_branchtreemanager.cpp"
#include "moc_shogiview.cpp"
#include "moc_elidelabel.cpp"
#include "moc_shogigamecontroller.cpp"
#include "moc_shogiboard.cpp"
