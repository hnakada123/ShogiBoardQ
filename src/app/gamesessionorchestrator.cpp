/// @file gamesessionorchestrator.cpp
/// @brief UI 起点の対局操作オーケストレータ実装

#include "gamesessionorchestrator.h"

#include "csagamecoordinator.h"
#include "dialogcoordinator.h"
#include "gamestatecontroller.h"
#include "logcategories.h"
#include "mainwindowgamestartservice.h"
#include "matchcoordinator.h"
#include "replaycontroller.h"
#include "sfenutils.h"
#include "startgamedatabridge.h"
#include "startgamedialog.h"

#include <utility>

namespace {

template<typename T>
T* deref(T** pp) { return pp ? *pp : nullptr; }

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
        eng.name = e.name;
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
    data.jishogiRule = dlg.jishogiRule();

    return data;
}

} // namespace

GameSessionOrchestrator::GameSessionOrchestrator(QObject* parent)
    : QObject(parent)
{
}

void GameSessionOrchestrator::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void GameSessionOrchestrator::initializeGame()
{
    if (m_deps.ensureGameStartCoordinator) m_deps.ensureGameStartCoordinator();
    auto* gameStart = deref(m_deps.gameStart);
    if (!gameStart) return;

    auto* match = deref(m_deps.match);
    QStringList* sfenRecord = match ? match->sfenRecordPtr() : nullptr;

    MainWindowGameStartService::PrepareDeps prep;
    prep.branchTree = m_deps.branchTree;
    prep.navState = m_deps.navState;
    prep.sfenRecord = sfenRecord;
    prep.currentSelectedPly = m_deps.currentSelectedPly ? *m_deps.currentSelectedPly : 0;
    prep.currentSfenStr = m_deps.currentSfenStr;

    const MainWindowGameStartService gameStartService;
    gameStartService.prepareForInitializeGame(prep);

    qCDebug(lcApp).noquote() << "initializeGame: FINAL currentSfenStr="
                             << (m_deps.currentSfenStr ? m_deps.currentSfenStr->left(50) : QStringLiteral("null"))
                             << " startSfenStr="
                             << (m_deps.startSfenStr ? m_deps.startSfenStr->left(50) : QStringLiteral("null"))
                             << " currentSelectedPly="
                             << (m_deps.currentSelectedPly ? *m_deps.currentSelectedPly : -1);

    const bool hasEditedStart = detectHasEditedStart(m_deps.startSfenStr, sfenRecord);

    StartGameDialog dlg;
    if (hasEditedStart) {
        dlg.forceCurrentPositionSelection();
    }
    if (dlg.exec() != QDialog::Accepted) {
        return;
    }

    StartGameDialogData dialogData = extractDialogData(dlg);
    if (hasEditedStart && dialogData.startingPositionNumber != 0) {
        dialogData.startingPositionNumber = 0;
    }

    MainWindowGameStartService::ContextDeps ctx;
    ctx.gc = m_deps.gameController;
    ctx.clock = match ? match->clock() : nullptr;
    ctx.sfenRecord = sfenRecord;
    ctx.kifuModel = m_deps.kifuModel;
    ctx.kifuLoadCoordinator = m_deps.kifuLoadCoordinator;
    ctx.currentSfenStr = m_deps.currentSfenStr;
    ctx.startSfenStr = m_deps.startSfenStr;
    ctx.selectedPly = m_deps.currentSelectedPly ? *m_deps.currentSelectedPly : 0;
    ctx.isReplayMode = m_deps.replayController ? m_deps.replayController->isReplayMode() : false;
    ctx.bottomIsP1 = m_deps.bottomIsP1 ? *m_deps.bottomIsP1 : true;

    GameStartCoordinator::Ctx c = gameStartService.buildContext(ctx);
    c.dialogData = dialogData;
    gameStart->initializeGame(c);
}

void GameSessionOrchestrator::handleResignation()
{
    auto* csa = deref(m_deps.csaGameCoordinator);
    if (m_deps.playMode && *m_deps.playMode == PlayMode::CsaNetworkMode && csa) {
        csa->onResign();
        return;
    }

    if (m_deps.ensureGameStateController) m_deps.ensureGameStateController();
    auto* gsc = deref(m_deps.gameStateController);
    if (gsc) {
        gsc->handleResignation();
    }
}

void GameSessionOrchestrator::handleBreakOffGame()
{
    if (m_deps.ensureGameStateController) m_deps.ensureGameStateController();
    auto* gsc = deref(m_deps.gameStateController);
    if (gsc) {
        gsc->handleBreakOffGame();
    }
}

void GameSessionOrchestrator::movePieceImmediately()
{
    auto* mc = deref(m_deps.match);
    if (mc) {
        mc->forceImmediateMove();
    }
}

void GameSessionOrchestrator::stopTsumeSearch()
{
    qCDebug(lcApp).noquote() << "stopTsumeSearch called";

    auto* mc = deref(m_deps.match);
    if (mc) {
        mc->stopAnalysisEngine();
    }
}

void GameSessionOrchestrator::openWebsiteInExternalBrowser()
{
    if (m_deps.ensureDialogCoordinator) m_deps.ensureDialogCoordinator();
    auto* dc = deref(m_deps.dialogCoordinator);
    if (dc) {
        dc->openProjectWebsite();
    }
}

void GameSessionOrchestrator::onResignationTriggered()
{
    handleResignation();
}
