/// @file gamesessionorchestrator.cpp
/// @brief UI 起点の対局操作オーケストレータ実装

#include "gamesessionorchestrator.h"
#include "kifunavigationstate.h"

#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "kifufilecontroller.h"
#include "dialogcoordinator.h"
#include "errorbus.h"
#include "gamestatecontroller.h"
#include "logcategories.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "matchcoordinator.h"
#include "replaycontroller.h"
#include "sfenutils.h"
#include "startgamedatabridge.h"
#include "startgamedialog.h"

#include <utility>

namespace {

bool detectHasEditedStart(const QString* startSfenStr, const QStringList* sfenRecord)
{
    const auto isEditedStart = [](const QString& raw) -> bool {
        const QString s = raw.trimmed();
        return !s.isEmpty()
               && !SfenUtils::isHirateStart(s)
               && s != QLatin1String("startpos");
    };

    if (startSfenStr && isEditedStart(*startSfenStr)) {
        return true;
    }
    if (sfenRecord && !sfenRecord->isEmpty() && isEditedStart(sfenRecord->first())) {
        return true;
    }
    return false;
}

StartGameDialogData extractDialogData(const StartGameDialog& dlg)
{
    StartGameDialogData data;
    data.isHuman1 = dlg.isHuman1();
    data.isHuman2 = dlg.isHuman2();
    data.isEngine1 = dlg.isEngine1();
    data.isEngine2 = dlg.isEngine2();

    data.engineName1 = dlg.engineName1();
    data.engineName2 = dlg.engineName2();
    data.humanName1 = dlg.humanName1();
    data.humanName2 = dlg.humanName2();
    data.engineNumber1 = dlg.engineNumber1();
    data.engineNumber2 = dlg.engineNumber2();

    for (const auto& e : std::as_const(dlg.engineList())) {
        StartGameDialogData::Engine eng;
        eng.path = e.path;
        data.engineList.append(eng);
    }

    data.basicTimeHour1 = dlg.basicTimeHour1();
    data.basicTimeMinutes1 = dlg.basicTimeMinutes1();
    data.byoyomiSec1 = dlg.byoyomiSec1();
    data.addEachMoveSec1 = dlg.addEachMoveSec1();
    data.basicTimeHour2 = dlg.basicTimeHour2();
    data.basicTimeMinutes2 = dlg.basicTimeMinutes2();
    data.byoyomiSec2 = dlg.byoyomiSec2();
    data.addEachMoveSec2 = dlg.addEachMoveSec2();
    data.isLoseOnTimeout = dlg.isLoseOnTimeout();

    data.startingPositionNumber = dlg.startingPositionNumber();
    data.maxMoves = dlg.maxMoves();
    data.autoSaveKifu = dlg.isAutoSaveKifu();
    data.kifuSaveDir = dlg.kifuSaveDir();
    data.isShowHumanInFront = dlg.isShowHumanInFront();
    data.consecutiveGames = dlg.consecutiveGames();
    data.isSwitchTurnEachGame = dlg.isSwitchTurnEachGame();

    return data;
}

} // namespace

GameSessionOrchestrator::GameSessionOrchestrator(QObject* parent)
    : QObject(parent)
{
}

void GameSessionOrchestrator::updateDeps(const Deps& deps)
{
    if (m_deps.branchTree != deps.branchTree && m_deps.branchTree) {
        disconnect(m_deps.branchTree, &KifuBranchTree::treeChanged,
                   this, &GameSessionOrchestrator::discardInterruptedGame);
    }
    m_deps = deps;
    if (m_deps.branchTree) {
        connect(m_deps.branchTree, &KifuBranchTree::treeChanged,
                this, &GameSessionOrchestrator::discardInterruptedGame, Qt::UniqueConnection);
    }
}

void GameSessionOrchestrator::initializeGame()
{
    auto* gameStart = m_deps.gameStart;
    if (!gameStart) return;

    auto* match = m_deps.match;
    QStringList* sfenRecord = match ? match->sfenRecordPtr() : nullptr;

    const bool hasEditedStart = detectHasEditedStart(m_deps.startSfenStr, sfenRecord);

    StartGameDialog dlg;
    if (hasEditedStart) {
        dlg.forceCurrentPositionSelection();
    }
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    const StartGameDialogData dialogData = extractDialogData(dlg);

    if (dialogData.startingPositionNumber != 0 && m_deps.kifuFileController
        && !m_deps.kifuFileController->confirmDiscardUnsaved()) return;

    // 表示中の手数（分岐ツリーのカーソル）から開始する
    const RecordLine line = m_deps.navState ? m_deps.navState->currentRecordLine() : RecordLine{};
    GameStartCoordinator::Ctx c;
    c.clock = match ? match->clock() : nullptr;
    c.currentSfenStr = m_deps.currentSfenStr;
    c.startSfenStr = m_deps.startSfenStr;
    c.recordLine = line;
    c.startNodeId = m_deps.navState && m_deps.navState->currentNode()
        ? m_deps.navState->currentNode()->nodeId() : -1;
    c.bottomSide = m_deps.bottomSide ? *m_deps.bottomSide : Turn::Black;

    c.dialogData = dialogData;
    QString errorMessage;
    if (!gameStart->initializeGame(c, errorMessage) && !errorMessage.isEmpty()) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, errorMessage);
    }
}

void GameSessionOrchestrator::handleResignation()
{
    auto* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr;
    if (m_deps.playMode && *m_deps.playMode == PlayMode::CsaNetworkMode && csa) {
        csa->onResign();
        return;
    }

    auto* gsc = m_deps.gameStateController;
    if (gsc) {
        gsc->handleResignation();
    }
}

void GameSessionOrchestrator::handleBreakOffGame()
{
    auto* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr;
    if (m_deps.playMode && *m_deps.playMode == PlayMode::CsaNetworkMode && csa) {
        csa->requestChudan();
        return;
    }
    auto* gsc = m_deps.gameStateController;
    if (gsc) {
        gsc->handleBreakOffGame();
    }
}

void GameSessionOrchestrator::movePieceImmediately()
{
    auto* csa = m_deps.csaGameWiring ? m_deps.csaGameWiring->coordinator() : nullptr;
    if (m_deps.playMode && *m_deps.playMode == PlayMode::CsaNetworkMode && csa) {
        csa->forceImmediateMove();
        return;
    }
    auto* mc = m_deps.match;
    if (mc) {
        mc->forceImmediateMove();
    }
}

void GameSessionOrchestrator::stopTsumeSearch()
{
    qCDebug(lcApp).noquote() << "stopTsumeSearch called";

    auto* mc = m_deps.match;
    if (mc) {
        mc->stopAnalysisEngine();
    }
}

void GameSessionOrchestrator::openWebsiteInExternalBrowser()
{
    auto* dc = m_deps.dialogCoordinator;
    if (dc) {
        dc->openProjectWebsite();
    }
}
