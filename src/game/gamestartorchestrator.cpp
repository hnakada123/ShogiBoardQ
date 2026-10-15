/// @file gamestartorchestrator.cpp
/// @brief 選択された手順から対局を開始する
#include "gamestartorchestrator.h"
#include "enginelistsettings.h"
#include "gamesettings.h"
#include "sfenutils.h"
#include "shogigamecontroller.h"

void GameStartOrchestrator::setRefs(const Refs& refs) { m_refs = refs; }
void GameStartOrchestrator::setHooks(const Hooks& hooks) { m_hooks = hooks; }

void GameStartOrchestrator::configureAndStart(const StartOptions& opt)
{
    if (m_hooks.clearGameOverState) m_hooks.clearGameOverState();
    m_refs.session->begin(opt);

    if (m_hooks.initializeNewGame) m_hooks.initializeNewGame(opt.sfenStart);
    if (m_hooks.setPlayersNames) m_hooks.setPlayersNames(opt.humanName1, opt.humanName2);
    if (m_hooks.setEngineNames) m_hooks.setEngineNames(opt.engineName1, opt.engineName2);
    const Turn side = opt.sfenStart.contains(QLatin1String(" w ")) ? Turn::White : Turn::Black;
    if (m_refs.gc) m_refs.gc->setCurrentPlayer(side);
    if (m_hooks.renderBoardFromGc) m_hooks.renderBoardFromGc();
    if (m_hooks.updateTurnDisplay) m_hooks.updateTurnDisplay(side);
    if (m_hooks.createAndStartModeStrategy) m_hooks.createAndStartModeStrategy(opt);
}

GameStartOrchestrator::StartOptions GameStartOrchestrator::buildStartOptions(
    PlayMode mode,
    const QString& startSfenStr,
    const QStringList* sfenRecord,
    const StartGameDialogData* dlg)
{
    StartOptions opt;
    opt.mode = mode;

    // --- 開始SFEN（空なら既定=司令塔側で startpos 扱い）
    if (!startSfenStr.isEmpty()) {
        opt.sfenStart = startSfenStr;
    } else if (sfenRecord && !sfenRecord->isEmpty()) {
        opt.sfenStart = sfenRecord->first();
    } else {
        opt.sfenStart.clear();
    }

    // --- 対局ダイアログあり：そのまま採用
    if (dlg) {
        const auto& engines = dlg->engineList;

        const int idx1 = dlg->engineNumber1;
        if (idx1 >= 0 && idx1 < engines.size()) {
            opt.engineName1 = dlg->engineName1;
            opt.enginePath1 = engines.at(idx1).path;
        }

        const int idx2 = dlg->engineNumber2;
        if (idx2 >= 0 && idx2 < engines.size()) {
            opt.engineName2 = dlg->engineName2;
            opt.enginePath2 = engines.at(idx2).path;
        }

        // 最大手数を取得
        opt.maxMoves = dlg->maxMoves;

        // 棋譜自動保存設定を取得
        opt.autoSaveKifu = dlg->autoSaveKifu;
        opt.kifuSaveDir = dlg->kifuSaveDir;

        // 対局者名を取得
        opt.humanName1 = dlg->humanName1;
        opt.humanName2 = dlg->humanName2;

        return opt;
    }

    // --- 対局ダイアログなし：INI から直近選択を復元（StartGameDialog と同じ仕様）
    {
        // 直近の対局設定からエンジン名を取得し、登録一覧から名前でパスを引く
        const auto pathForName = &EngineListSettings::pathForName;
        const QString name1 = GameSettings::StartGame::player1.engineName();
        const QString name2 = GameSettings::StartGame::player2.engineName();

        if (!name1.isEmpty()) {
            opt.engineName1 = name1;
            opt.enginePath1 = pathForName(name1);
        }
        if (!name2.isEmpty()) {
            opt.engineName2 = name2;
            opt.enginePath2 = pathForName(name2);
        }
        // パスが空でもここでは許容（initializeAndStartEngineCommunication 内で失敗は通知）
        return opt;
    }
}

// ============================================================
// 盤面反転・対局準備
// ============================================================

void GameStartOrchestrator::ensureHumanAtBottomIfApplicable(
    const StartGameDialogData* dlg, Turn bottomSide)
{
    if (!dlg) return;

    // 「人を手前に表示する」がチェックされていない場合は何もしない
    if (!dlg->isShowHumanInFront) {
        return;
    }

    const bool humanP1  = dlg->isHuman1;
    const bool humanP2  = dlg->isHuman2;
    const bool oneHuman = (humanP1 ^ humanP2); // HvE / EvH のときだけ true

    if (!oneHuman) {
        // HvH / EvE は対象外（仕様どおり）
        return;
    }

    // 手前に表示している側と人間の側が食い違っていたら1回だけ反転
    const Turn humanSide = humanP1 ? Turn::Black : Turn::White;
    const bool needFlip = (humanSide != bottomSide);
    if (needFlip) {
        if (m_hooks.flipBoard) m_hooks.flipBoard();
    }
}

// ============================================================
// 対局開始メインエントリ
// ============================================================

