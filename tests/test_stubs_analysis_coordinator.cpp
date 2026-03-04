/// @file test_stubs_analysis_coordinator.cpp
/// @brief AnalysisCoordinator + AnalysisResultHandler テスト用スタブ
///
/// analysiscoordinator.cpp / analysisresulthandler.cpp が参照する
/// 外部シンボルのスタブ実装を提供する。

#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QWidget>

#include "kifuanalysislistmodel.h"
#include "kifuanalysisresultsdisplay.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "analysisresultspresenter.h"
#include "pvboarddialog.h"
#include "pvboardcontroller.h"
#include "branchtreemanager.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "settingscommon.h"

// === BranchTreeManager スタブ ===
BranchTreeManager::BranchTreeManager(QObject* parent) : QObject(parent) {}
BranchTreeManager::~BranchTreeManager() = default;
void BranchTreeManager::highlightBranchTreeAt(int, int, bool) {}
bool BranchTreeManager::eventFilter(QObject* obj, QEvent* ev) { return QObject::eventFilter(obj, ev); }

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

// === PvBoardDialog スタブ ===
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

// === ShogiView スタブ ===
// pvboarddialog.h が shogiview.h をインクルードするため必要
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
// shogiview.h が elidelabel.h をインクルードするため必要
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

// === ShogiViewLayout / ShogiViewInteraction スタブ ===
#include "shogiviewlayout.h"
#include "shogiviewinteraction.h"
ShogiViewLayout::ShogiViewLayout() {}
ShogiViewInteraction::ShogiViewInteraction() {}

// === ShogiBoard スタブ ===
// shogiview.h → shogigamecontroller.h 経由で必要
ShogiBoard::ShogiBoard(int ranks, int files, QObject* parent)
    : QObject(parent), m_ranks(ranks), m_files(files)
{
    m_boardData.fill(Piece::None, ranks * files);
}
const QList<Piece>& ShogiBoard::boardData() const { return m_boardData; }
std::optional<SfenComponents> ShogiBoard::parseSfen(const QString&) { return std::nullopt; }
void ShogiBoard::setSfen(const QString&) { m_boardData.fill(Piece::None, m_ranks * m_files); }
Piece ShogiBoard::getPieceCharacter(int, int) { return Piece::None; }
const QMap<Piece, int>& ShogiBoard::getPieceStand() const { return m_pieceStand; }
int ShogiBoard::pieceStandCount(Piece piece) const { return m_pieceStand.value(piece, 0); }
void ShogiBoard::addStandPiece(Piece piece, int delta) { if (delta > 0) m_pieceStand[piece] += delta; }
bool ShogiBoard::consumeStandPiece(Piece piece) { if (m_pieceStand.value(piece, 0) <= 0) return false; --m_pieceStand[piece]; return true; }
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

// === SettingsCommon スタブ ===
namespace SettingsCommon {
QString settingsFilePath() { return QStringLiteral("/tmp/tst_analysis_coordinator_stub.ini"); }
QSettings& openSettings() { static QSettings s(settingsFilePath(), QSettings::IniFormat); return s; }
}

// === MOC出力のインクルード ===
#include "moc_branchtreemanager.cpp"
#include "moc_kifuanalysislistmodel.cpp"
#include "moc_kifuanalysisresultsdisplay.cpp"
#include "moc_kifurecordlistmodel.cpp"
#include "moc_kifudisplay.cpp"
#include "moc_analysisresultspresenter.cpp"
#include "moc_pvboarddialog.cpp"
#include "moc_shogiview.cpp"
#include "moc_elidelabel.cpp"
#include "moc_shogiboard.cpp"
#include "moc_shogigamecontroller.cpp"
