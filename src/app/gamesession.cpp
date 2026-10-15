/// @file gamesession.cpp
/// @brief 対局の流れの UI 側の受け持ち: 利用者の操作・開始・棋譜の行・待った

#include "gamesession.h"

#include "analysissession.h"
#include "appstate.h"
#include "boardinteractioncontroller.h"
#include "boardsyncpresenter.h"
#include "consecutivegamescontroller.h"
#include "csagamecoordinator.h"
#include "csagamewiring.h"
#include "errorbus.h"
#include "evaluationgraphcontroller.h"
#include "gamerecordmodel.h"
#include "josekiwindowcontroller.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifuexportmetadata.h"
#include "kifufilecontroller.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "playerinfopresenter.h"
#include "gameinfopanecontroller.h"
#include "prestartcleanuphandler.h"
#include "replaycontroller.h"
#include "sfenutils.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "startgamedatabridge.h"
#include "startgamedialog.h"
#include "timecontrolcontroller.h"
#include "turnstatesyncservice.h"

#include <QDateTime>
#include <utility>
#include <QAbstractItemView>
#include <QTableView>
#include "kifunavigationcoordinator.h"
#include "kifurecordlistmodel.h"
#include "recordpane.h"
#include "shogiclock.h"
#include "uistatepolicymanager.h"

namespace {

/// 開始局面が平手でない（局面編集した・棋譜の局面図から始めた）か
bool isEditedStart(const QString& raw)
{
    const QString s = raw.trimmed();
    return !s.isEmpty() && !SfenUtils::isHirateStart(s) && s != QLatin1String("startpos");
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

GameSession::GameSession(QObject* parent)
    : QObject(parent)
{
}

void GameSession::setDeps(const Deps& deps)
{
    if (m_deps.branchTree && m_deps.branchTree != deps.branchTree)
        disconnect(m_deps.branchTree, &KifuBranchTree::treeChanged, this, &GameSession::discardInterruptedGame);
    m_deps = deps;
    // 棋譜を変えたら、中断した対局は再開できない
    if (m_deps.branchTree)
        connect(m_deps.branchTree, &KifuBranchTree::treeChanged,
                this, &GameSession::discardInterruptedGame, Qt::UniqueConnection);
}

bool GameSession::isCsaGameActive() const
{
    return m_deps.state->activity.is(AppActivityState::Activity::DuringCsaGame)
        && m_deps.csa && m_deps.csa->coordinator();
}

// ============================================================
// 棋譜の行（MatchRecordSink）
// ============================================================

bool GameSession::canAppendRecord() const
{
    return m_deps.liveSession->canAddMove();
}

void GameSession::appendRecord(const RecordEntry& entry)
{
    if (entry.displayText.isEmpty()) return;
    qCDebug(lcApp).noquote() << "appendRecord: text=" << entry.displayText;
    // 定跡・検討の局面は、指した後の局面に合わせる
    if (!entry.sfen.isEmpty()) m_deps.state->currentSfenStr = entry.sfen;
    if (!m_deps.liveSession->canAddMove()) return;
    RecordEntry recorded = entry;
    // 棋譜の途中から続けた対局では、消費時間の累計を同じ側の直前の累計から続ける
    recorded.time = m_deps.liveSession->continuedTime(recorded.time);
    m_deps.gameRecord->markDirty();
    m_deps.liveSession->appendRecord(recorded);
}

bool GameSession::canUndoRecords(int count) const
{
    // 途中局面からの対局では、開始位置を越えて巻き戻さない
    return !m_deps.liveSession->isActive() || m_deps.liveSession->canUndoMoves(count);
}

void GameSession::undoRecords(int count)
{
    if (m_deps.liveSession->isActive()) {
        const bool undone = m_deps.liveSession->undoMoves(count);
        Q_ASSERT(undone);
        Q_UNUSED(undone)
    }
}

// ============================================================
// 利用者の操作
// ============================================================

void GameSession::startGameFromDialog()
{
    const bool hasEditedStart = isEditedStart(m_deps.state->startSfenStr)
        || isEditedStart(m_deps.match->record().startSfen);

    StartGameDialog dlg;
    if (hasEditedStart) dlg.forceCurrentPositionSelection();
    if (dlg.exec() != QDialog::Accepted) return;

    const StartGameDialogData dialogData = extractDialogData(dlg);
    if (dialogData.startingPositionNumber != 0 && !m_deps.kifuFileController->confirmDiscardUnsaved()) return;

    // 表示中の手数（分岐ツリーのカーソル）から開始する
    GameStartCoordinator::Ctx c;
    c.currentSfen = m_deps.state->currentSfenStr;
    c.startSfen = m_deps.state->startSfenStr;
    c.recordLine = m_deps.navState->currentRecordLine();
    c.startNodeId = m_deps.navState->currentNode() ? m_deps.navState->currentNode()->nodeId() : -1;
    c.bottomSide = m_deps.shogiView->flipMode() ? Turn::White : Turn::Black;
    c.dialogData = dialogData;

    QString errorMessage;
    if (!m_deps.gameStart->initializeGame(c, errorMessage) && !errorMessage.isEmpty()) {
        ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, errorMessage);
    }
}

void GameSession::resign()
{
    if (isCsaGameActive()) {
        m_deps.csa->coordinator()->onResign();
        return;
    }
    m_deps.match->handleResign();
}

void GameSession::breakOff()
{
    if (isCsaGameActive()) {
        m_deps.csa->coordinator()->requestChudan();
        return;
    }
    m_deps.match->handleBreakOff();
}

void GameSession::makeEngineMoveNow()
{
    if (isCsaGameActive()) {
        m_deps.csa->coordinator()->forceImmediateMove();
        return;
    }
    m_deps.match->forceImmediateMove();
}

void GameSession::undoTwoMoves()
{
    if (m_deps.match->undoTwoPlies()) m_deps.gameRecord->markDirty();
}

void GameSession::onMovesUndone(int ply)
{
    // 定跡・検討の局面は、戻した局面に合わせる
    m_deps.state->currentSfenStr = m_deps.match->record().lastSfen();
    // 盤・ハイライト・手番と時計の表示を、戻した局面に合わせる
    if (m_deps.boardController) {
        m_deps.boardController->cancelPendingClick();
        m_deps.boardController->clearAllHighlights();
    }
    m_deps.boardSync->syncBoardAndHighlightsAtRow(ply);
    m_deps.turnSync->updateTurnAndTimekeepingDisplay();
    // 取り消した手の評価値を消し、縦線（カーソルライン）を戻した手数に合わせる
    m_deps.evalGraph->trimToPly(ply);
    m_deps.evalGraph->setCurrentPly(ply);
    // 人間の手番なら盤を操作できるようにする
    m_deps.shogiView->setMouseClickMode(m_deps.match->isHumanSide(m_deps.gameController->currentPlayer()));
}

// ============================================================
// 対局の開始
// ============================================================

void GameSession::prepareStart(const GameStartPreparation& preparation)
{
    KifuBranchNode* node = m_deps.preStartCleanup->performCleanup(preparation);
    MainWindowResetService().clearSessionDependentUi(m_deps.sessionUi);
    // 対局者・持ち時間の対局情報は開始局面（平手か駒落ちか）で見出しが変わるので、対局を準備する前に記録する
    if (!preparation.rootSfen.isEmpty()) m_deps.state->startSfenStr = preparation.rootSfen;
    if (preparation.mode != GameStartPreparation::Mode::Reset && node) {
        m_deps.liveSession->startFromNode(node);
    }
}

void GameSession::applyTimeControl(const GameStartCoordinator::TimeSettings& tc)
{
    qCDebug(lcApp).noquote()
        << "applyTimeControl:" << " enabled=" << tc.enabled
        << " P1{base=" << tc.rules.black.mainMs << " byoyomi=" << tc.rules.black.byoyomiMs
        << " inc=" << tc.rules.black.incrementMs << "}"
        << " P2{base=" << tc.rules.white.mainMs << " byoyomi=" << tc.rules.white.byoyomiMs
        << " inc=" << tc.rules.white.incrementMs << "}";

    // 連続対局の次局にも同じ時間の決まりを使う
    m_deps.consecutive->setTimeSettings(tc);
    m_deps.timeController->presentTimeControl(tc, m_deps.shogiView);
    // 対局情報ドックに持ち時間を追加
    if (tc.enabled) m_deps.playerInfo->updateGameInfoWithTimeControl(tc.rules);
}

void GameSession::onGamePrepared(const MatchStartOptions& opt)
{
    // 対局の手順の開始局面と、対局を始める局面
    m_deps.state->startSfenStr = opt.recordLine.startSfen;
    m_deps.state->currentSfenStr = opt.sfenStart;
    renderBoard();
    m_deps.state->players = opt.players;
    m_deps.playerInfo->showMatchPlayers();
    m_deps.turnSync->onTurnManagerChanged(m_deps.gameController->currentPlayer());
}

void GameSession::renderBoard()
{
    // 対局を準備するたびに盤（ShogiBoard）を作り直すので、描く盤もそのたびに渡す
    if (m_deps.gameController->board()) m_deps.shogiView->applyBoardAndRender(m_deps.gameController->board());
}

void GameSession::onGameStarted(const MatchStartOptions& opt)
{
    // 終局の知らせで対局者を下手・上手と呼ぶか（対局情報の見出しと開始局面で決まる）
    m_deps.match->setHandicapNames(usesHandicapNames());

    discardInterruptedGame();
    m_deps.josekiController->updateJosekiWindow();
    m_deps.consecutive->onGameStarted(opt);
}

void GameSession::startNextGame(const GameStartCoordinator::StartParams& params)
{
    // 次局は独立した棋譜として、同じ開始局面から始める
    const QString start = params.opt.sfenStart;
    const QString sfen = start.isEmpty() || start == QLatin1String("startpos") ? SfenUtils::hirateSfen() : start;
    m_deps.state->startSfenStr = sfen;
    m_deps.state->currentSfenStr = sfen;
    resetGameState();
    resetModels(sfen);
    prepareStart({GameStartPreparation::Mode::NewGame, sfen, -1});
    m_deps.shogiView->setGameOverStyleLock(false);
    m_deps.gameStart->start(params);
}

bool GameSession::usesHandicapNames() const
{
    const auto* gameInfo = m_deps.playerInfo->gameInfoController();
    return KifuExportMetadataBuilder::usesHandicapNames(
        gameInfo ? gameInfo->gameInfo() : QList<KifGameInfoItem>(), m_deps.state->startSfenStr);
}

// ============================================================
// 通信対局
// ============================================================

void GameSession::beginCsaGameRecord(const QString& startSfen, const QString& blackName, const QString& whiteName)
{
    m_deps.state->startSfenStr = startSfen;
    m_deps.state->currentSfenStr = startSfen;
    // 通信対局の対局者はサーバーが知らせた名前（エンジンで指していても対局者名で記録する）
    MatchPlayers players;
    players.blackHumanName = blackName;
    players.whiteHumanName = whiteName;
    m_deps.state->players = players;

    // 利用者が対局情報に書きかけた内容は、棋譜の作り直しで消さない
    GameInfoPaneController* gameInfo = m_deps.playerInfo->gameInfoController();
    gameInfo->commitPendingEditor();
    const QList<KifGameInfoItem> preparedInfo = gameInfo->gameInfo();
    resetModels(startSfen);
    gameInfo->setGameInfo(preparedInfo);

    // 持ち時間と開始日時は CsaGameWiring がサーバーの対局条件で TimeControlController に設定済み
    const TimeControlController* tc = m_deps.timeController;
    const QDateTime startTime = tc->gameStartDateTime().isValid() ? tc->gameStartDateTime()
                                                                  : QDateTime::currentDateTime();
    m_deps.playerInfo->setGameInfoForMatchStart(startTime, blackName, whiteName,
                                                KifuExportMetadataBuilder::handicapLabel(startSfen),
                                                tc->timeControl());
    m_deps.liveSession->startFromRoot();
}

// ============================================================
// リセット
// ============================================================

void GameSession::stopEnginesAndNetwork()
{
    m_deps.analysis->stop();
    m_deps.match->clearGameOverState();
    if (CsaGameCoordinator* csa = m_deps.csa ? m_deps.csa->coordinator() : nullptr) csa->stopGame();
    m_deps.consecutive->reset();
}

void GameSession::resetToInitialState()
{
    const QString hirate = SfenUtils::hirateSfen();
    stopEnginesAndNetwork();
    prepareStart({GameStartPreparation::Mode::Reset, hirate, -1});
    resetGameState();
    resetModels(hirate);
    m_deps.state->startSfenStr = hirate;
    m_deps.state->currentSfenStr = hirate;
    resetUiState(hirate);
}

void GameSession::resetGameState()
{
    m_deps.state->activity.setActivity(AppActivityState::Activity::Idle);
    MainWindowResetService().clearGameStateFields(*m_deps.state);
    m_deps.replayController->setReplayMode(false);
    m_deps.replayController->setResumeFromCurrent(false);
}

void GameSession::resetModels(const QString& sfen)
{
    MainWindowResetService().resetModels(m_deps.models, sfen);
}

void GameSession::clearUiBeforeKifuLoad()
{
    MainWindowResetService().clearUiBeforeKifuLoad(m_deps.sessionUi);
}

void GameSession::clearEvalState()
{
    m_deps.evalGraph->clear();
}

void GameSession::resetUiState(const QString& sfen)
{
    MainWindowResetService::UiResetDeps deps;
    deps.gameController = m_deps.gameController;
    deps.shogiView = m_deps.shogiView;
    deps.uiStatePolicy = m_deps.uiStatePolicy;
    deps.josekiController = m_deps.josekiController;
    deps.playerInfo = m_deps.playerInfo;
    MainWindowResetService().resetUiState(deps, sfen);
}

// ============================================================
// 対局の流れの UI 側の受け持ち: 終局・中断と再開
// ============================================================

// ============================================================
// 終局
// ============================================================

void GameSession::onGameOverStateChanged(const MatchCoordinator::GameOverState& st)
{
    if (!st.isOver) return;

    // 棋譜追記に伴う閲覧モードへの遷移は gameEnded より先に起こるため、
    // 終局を検知した時点で名前・時計の手番ハイライトを保護する。
    m_deps.shogiView->setGameOverStyleLock(true);
    // 終局の行を書くまでは表示の後処理を待つ
    if (!st.moveAppended) return;

    // 表示中の手数を最終行（終局行）に合わせ、閲覧モードへ移る
    const int rows = m_deps.kifuRecordModel->rowCount();
    if (rows > 0) m_deps.kifuNav->syncNavStateToPly(rows - 1);
    m_deps.uiStatePolicy->transitionToIdle();
    m_deps.replayController->setReplayMode(true);
    m_deps.shogiView->removeHighlightAllData();
}

void GameSession::onGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    qCDebug(lcApp).nospace() << "onGameEnded cause=" << static_cast<int>(info.cause) << " loser=" << info.loser;

    // 中断した対局は再開できるよう、ライブ対局のセッションを確定しない（再開で中断の行を除く）
    if (info.cause != MatchCoordinator::Cause::BreakOff && m_deps.liveSession->isActive()) {
        qCDebug(lcApp) << "onGameEnded: committing live game session";
        m_deps.liveSession->commit();
    }

    // 盤の操作を止め、棋譜欄を1行選択に戻す
    m_deps.timeController->clock()->stopClock();
    m_deps.shogiView->setMouseClickMode(false);
    m_deps.shogiView->update();
    if (QTableView* view = m_deps.recordPane->kifuView()) view->setSelectionMode(QAbstractItemView::SingleSelection);
    m_deps.replayController->setReplayMode(true);

    // 終局時刻の記録とゲーム情報への反映（投了手を含めるため、終局手の追記の後で行う）
    recordGameEndTime();

    if (info.cause == MatchCoordinator::Cause::BreakOff) m_deps.consecutive->reset();
    m_deps.uiStatePolicy->transitionToIdle();
}

void GameSession::recordGameEndTime()
{
    m_deps.timeController->recordGameEndTime();
    const QDateTime endTime = m_deps.timeController->gameEndDateTime();
    if (endTime.isValid()) m_deps.playerInfo->updateGameInfoWithEndTime(endTime);
}

void GameSession::onGameEndProcessed(const MatchCoordinator::GameEndInfo& info)
{
    if (info.cause == MatchCoordinator::Cause::BreakOff) return;
    // 終局の知らせ・保存が終わってから次局を予約する（エンジン同士の対局のみ）
    const bool isEvE = m_deps.match->players().engineVsEngine();
    if (isEvE && m_deps.consecutive->shouldStartNextGame()) {
        qCDebug(lcApp) << "EvE game ended, starting next consecutive game...";
        m_deps.consecutive->startNextGame();
    }
}

// ============================================================
// 中断と再開
// ============================================================

void GameSession::onGameInterrupted()
{
    const KifuBranchNode* node = m_deps.navState->currentNode();
    if (!m_deps.match->interruptedGame() || !node || !node->isTerminal()) return;
    m_interruptedNodeId = node->nodeId();
    m_deps.uiStatePolicy->setResumeAvailable(true);
}

void GameSession::discardInterruptedGame()
{
    if (m_interruptedNodeId < 0) return;
    m_interruptedNodeId = -1;
    m_deps.match->discardInterruptedGame();
    m_deps.uiStatePolicy->setResumeAvailable(false);
}

void GameSession::resumeInterruptedGame()
{
    if (!m_deps.match->interruptedGame()) return;
    KifuBranchNode* terminal = m_deps.branchTree->nodeAt(m_interruptedNodeId);
    if (!terminal || !terminal->isTerminal() || !terminal->parent()) {
        discardInterruptedGame();
        return;
    }
    KifuBranchNode* node = terminal->parent();
    const QString sfenStart = m_deps.match->interruptedGame()->options.sfenStart;
    m_interruptedNodeId = -1;
    m_deps.uiStatePolicy->setResumeAvailable(false);

    // 閲覧位置にかかわらず中断した手の直前へ戻す。削除前に全選択を退避する。
    KifuNavigationState* nav = m_deps.navState;
    nav->setCurrentNode(node);
    nav->resetPreferredLineIndex();
    nav->clearLineSelectionMemory();
    nav->rememberPathSelections(node);
    m_deps.liveSession->discard();
    // 中断の行を除く（再開する対局は m_interruptedNodeId を外してあるので、棋譜の変更の知らせで捨てない）
    m_deps.branchTree->removeLeaf(terminal);
    // sessionStarted の表示同期で、手数・時間見出しを含めた棋譜欄を復元する。
    // ここで簡略な表示に作り直すと列幅が縮み、ウィンドウ全体が再配置される。
    m_deps.liveSession->startFromNode(node);

    m_deps.state->currentSfenStr = sfenStart;
    m_deps.replayController->setReplayMode(false);
    // 終局表示を解き、盤を操作できるようにして、終局時刻を消す
    m_deps.shogiView->setGameOverStyleLock(false);
    m_deps.shogiView->setMouseClickMode(true);
    m_deps.timeController->clearGameEndTime();
    m_deps.playerInfo->updateGameInfoWithEndTime(QDateTime());
    m_deps.gameRecord->markDirty();
    m_deps.gameStart->resumeInterruptedGame();
}
