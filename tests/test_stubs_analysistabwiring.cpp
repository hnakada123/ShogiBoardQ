/// @file test_stubs_analysistabwiring.cpp
/// @brief AnalysisTabWiring 実行時テスト用スタブ

#include <QObject>
#include <QString>
#include <QVariant>
#include <optional>

#include "branchnavigationwiring.h"
#include "commentcoordinator.h"
#include "pvclickcontroller.h"
#include "usicommandcontroller.h"
#include "considerationwiring.h"
#include "shogienginethinkingmodel.h"
#include "recordpaneappearancemanager.h"

namespace AnalysisTabWiringTracker {
bool branchNavigationCalled = false;
int branchRow = -1;
int branchPly = -1;

bool commentUpdatedCalled = false;
int commentMoveIndex = -1;
QString commentText;

bool pvClickedCalled = false;
int pvEngineIndex = -1;
int pvRow = -1;

bool usiCommandCalled = false;
int usiTarget = -1;
QString usiCommand;

bool considerationDialogCalled = false;
bool engineSettingsCalled = false;
int engineSettingsNumber = -1;
QString engineSettingsName;

bool engineChangedCalled = false;
int changedEngineIndex = -1;
QString changedEngineName;

void reset()
{
    branchNavigationCalled = false;
    branchRow = -1;
    branchPly = -1;

    commentUpdatedCalled = false;
    commentMoveIndex = -1;
    commentText.clear();

    pvClickedCalled = false;
    pvEngineIndex = -1;
    pvRow = -1;

    usiCommandCalled = false;
    usiTarget = -1;
    usiCommand.clear();

    considerationDialogCalled = false;
    engineSettingsCalled = false;
    engineSettingsNumber = -1;
    engineSettingsName.clear();

    engineChangedCalled = false;
    changedEngineIndex = -1;
    changedEngineName.clear();
}
} // namespace AnalysisTabWiringTracker

BranchNavigationWiring::BranchNavigationWiring(QObject* parent)
    : QObject(parent)
{
}

void BranchNavigationWiring::updateDeps(const Deps&) {}

void BranchNavigationWiring::initialize() {}

void BranchNavigationWiring::onBranchTreeBuilt() {}

void BranchNavigationWiring::onBranchNodeActivated(int row, int ply)
{
    AnalysisTabWiringTracker::branchNavigationCalled = true;
    AnalysisTabWiringTracker::branchRow = row;
    AnalysisTabWiringTracker::branchPly = ply;
}

void BranchNavigationWiring::onBranchTreeResetForNewGame() {}

CommentCoordinator::CommentCoordinator(QObject* parent)
    : QObject(parent)
{
}

void CommentCoordinator::broadcastComment(const QString&, bool) {}

bool CommentCoordinator::handleRecordRowChangeRequest(int, const QString&)
{
    return true;
}

void CommentCoordinator::onCommentUpdated(int moveIndex, const QString& newComment)
{
    AnalysisTabWiringTracker::commentUpdatedCalled = true;
    AnalysisTabWiringTracker::commentMoveIndex = moveIndex;
    AnalysisTabWiringTracker::commentText = newComment;
}

void CommentCoordinator::onGameRecordCommentChanged(int, const QString&) {}

void CommentCoordinator::onCommentUpdateCallback(int, const QString&) {}

void CommentCoordinator::onBookmarkEditRequested() {}

void CommentCoordinator::onBookmarkUpdateCallback(int, const QString&) {}

void CommentCoordinator::onNavigationCommentUpdate(int, const QString&, bool) {}

PvClickController::PvClickController(QObject* parent)
    : QObject(parent)
{
}

PvClickController::~PvClickController() = default;

void PvClickController::setThinkingModels(ShogiEngineThinkingModel*, ShogiEngineThinkingModel*) {}
void PvClickController::setConsiderationModel(ShogiEngineThinkingModel*) {}
void PvClickController::setLogModels(UsiCommLogModel*, UsiCommLogModel*) {}
void PvClickController::setSfenRecord(QStringList*) {}
void PvClickController::setGameMoves(const QList<ShogiMove>*) {}
void PvClickController::setUsiMoves(const QStringList*) {}
void PvClickController::setStateRefs(const StateRefs&) {}
void PvClickController::setShogiView(ShogiView*) {}

void PvClickController::onPvRowClicked(int engineIndex, int row)
{
    AnalysisTabWiringTracker::pvClickedCalled = true;
    AnalysisTabWiringTracker::pvEngineIndex = engineIndex;
    AnalysisTabWiringTracker::pvRow = row;
}

void PvClickController::onPvDialogFinished(int) {}

QStringList PvClickController::searchUsiMovesFromLog(UsiCommLogModel*) const
{
    return {};
}

QString PvClickController::resolveCurrentSfen(const QString& baseSfen) const
{
    return baseSfen;
}

void PvClickController::resolvePlayerNames(QString&, QString&) const {}

QString PvClickController::resolveLastUsiMove() const
{
    return {};
}

ShogiEngineThinkingModel* PvClickController::modelForEngineIndex(int) const
{
    return nullptr;
}

void PvClickController::resolveHighlightContext(const QString&, QString&, QString&) const {}

void PvClickController::syncFromRefs() {}

UsiCommandController::UsiCommandController(QObject* parent)
    : QObject(parent)
{
}

void UsiCommandController::setMatchCoordinator(MatchCoordinator*) {}

void UsiCommandController::setAnalysisTab(EngineAnalysisTab*) {}

void UsiCommandController::sendCommand(int target, const QString& command)
{
    AnalysisTabWiringTracker::usiCommandCalled = true;
    AnalysisTabWiringTracker::usiTarget = target;
    AnalysisTabWiringTracker::usiCommand = command;
}

void UsiCommandController::appendStatus(const QString&) {}

ConsiderationWiring::ConsiderationWiring(const Deps&, QObject* parent)
    : QObject(parent)
{
}

void ConsiderationWiring::updateDeps(const Deps&) {}

void ConsiderationWiring::setDialogCoordinator(DialogCoordinator*) {}

void ConsiderationWiring::setMatchCoordinator(MatchCoordinator*) {}

void ConsiderationWiring::displayConsiderationDialog()
{
    AnalysisTabWiringTracker::considerationDialogCalled = true;
}

void ConsiderationWiring::onEngineSettingsRequested(int engineNumber, const QString& engineName)
{
    AnalysisTabWiringTracker::engineSettingsCalled = true;
    AnalysisTabWiringTracker::engineSettingsNumber = engineNumber;
    AnalysisTabWiringTracker::engineSettingsName = engineName;
}

void ConsiderationWiring::onEngineChanged(int engineIndex, const QString& engineName)
{
    AnalysisTabWiringTracker::engineChangedCalled = true;
    AnalysisTabWiringTracker::changedEngineIndex = engineIndex;
    AnalysisTabWiringTracker::changedEngineName = engineName;
}

void ConsiderationWiring::onModeStarted() {}

void ConsiderationWiring::onTimeSettingsReady(bool, int) {}

void ConsiderationWiring::onDialogMultiPVReady(int) {}

void ConsiderationWiring::onMultiPVChangeRequested(int) {}

void ConsiderationWiring::updateArrows() {}

void ConsiderationWiring::onShowArrowsChanged(bool) {}

bool ConsiderationWiring::updatePositionIfNeeded(int, const QString&,
                                                 const QList<ShogiMove>*,
                                                 KifuRecordListModel*)
{
    return false;
}

void ConsiderationWiring::handleStopRequest() {}

void ConsiderationWiring::ensureUIController() {}

ShogiEngineThinkingModel::ShogiEngineThinkingModel(QObject* parent)
    : AbstractListModel(parent)
{
}

int ShogiEngineThinkingModel::columnCount(const QModelIndex&) const
{
    return 6;
}

QVariant ShogiEngineThinkingModel::data(const QModelIndex&, int) const
{
    return {};
}

QVariant ShogiEngineThinkingModel::headerData(int, Qt::Orientation, int) const
{
    return {};
}

QString ShogiEngineThinkingModel::usiPvAt(int) const
{
    return {};
}

const ShogiInfoRecord* ShogiEngineThinkingModel::recordAt(int) const
{
    return nullptr;
}

std::optional<int> ShogiEngineThinkingModel::findRowByMultipv(int) const
{
    return std::nullopt;
}

void ShogiEngineThinkingModel::updateByMultipv(ShogiInfoRecord*, int) {}

void ShogiEngineThinkingModel::sortByScore() {}

void ShogiEngineThinkingModel::trimToMaxRows(int) {}

RecordPaneAppearanceManager::RecordPaneAppearanceManager(int initialFontSize)
    : m_fontSize(initialFontSize)
{
}

bool RecordPaneAppearanceManager::tryIncreaseFontSize()
{
    return false;
}

bool RecordPaneAppearanceManager::tryDecreaseFontSize()
{
    return false;
}

void RecordPaneAppearanceManager::applyFontToViews(QTableView*, QTableView*) {}

void RecordPaneAppearanceManager::toggleTimeColumn(QTableView*, bool) {}

void RecordPaneAppearanceManager::toggleBookmarkColumn(QTableView*, bool) {}

void RecordPaneAppearanceManager::toggleCommentColumn(QTableView*, bool) {}

void RecordPaneAppearanceManager::applyColumnVisibility(QTableView*) {}

void RecordPaneAppearanceManager::updateColumnResizeModes(QTableView*) {}

void RecordPaneAppearanceManager::setupSelectionPalette(QTableView*) {}

QString RecordPaneAppearanceManager::kifuTableStyleSheet(int)
{
    return {};
}

QString RecordPaneAppearanceManager::branchTableStyleSheet(int)
{
    return {};
}

void RecordPaneAppearanceManager::setKifuViewEnabled(QTableView*, bool, bool&) {}

#include "moc_branchnavigationwiring.cpp"
#include "moc_commentcoordinator.cpp"
#include "moc_pvclickcontroller.cpp"
#include "moc_usicommandcontroller.cpp"
#include "moc_considerationwiring.cpp"
#include "moc_shogienginethinkingmodel.cpp"
