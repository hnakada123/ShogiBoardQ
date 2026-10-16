/// @file gamesession.cpp
/// @brief 対局の利用者の操作・開始の手順・棋譜の行・終局と中断の後処理

#include "gamesession.h"

#include "analysissession.h"
#include "appstate.h"
#include "consecutivegameseries.h"
#include "csagamecoordinator.h"
#include "errorbus.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuexportmetadata.h"
#include "kifufilecontroller.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "mainwindowappearancecontroller.h"
#include "playerinfopresenter.h"
#include "sessionresetcoordinator.h"
#include "sfenutils.h"
#include "shogiclock.h"
#include "shogiview.h"
#include "startgamedialog.h"
#include "timecontrolutil.h"
#include "timedisplaypresenter.h"
#include "uistatepolicymanager.h"

#include <QDateTime>

namespace {

/// 開始局面が平手でない（局面編集した・棋譜の局面図から始めた）か
bool isEditedStart(const QString& raw)
{
    const QString s = raw.trimmed();
    return !s.isEmpty() && !SfenUtils::isHirateStart(s) && s != QLatin1String("startpos");
}

/// SFEN の手番（2つ目の欄）
Turn sideToMove(const QString& sfen)
{
    return sfenToTurn(sfen.section(QLatin1Char(' '), 1, 1));
}

} // namespace

GameSession::GameSession(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
    // 棋譜を変えたら、中断した対局は再開できない
    connect(m_deps.navState->tree(), &KifuBranchTree::treeChanged, this, &GameSession::discardInterruptedGame);
}

bool GameSession::isCsaGameActive() const
{
    return m_deps.state->activity.is(AppActivityState::Activity::DuringCsaGame)
        && m_deps.csaGame;
}

QString GameSession::recordStartSfen() const
{
    return m_deps.document->startSfen();
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
    if (entry.displayText.isEmpty() || !m_deps.liveSession->canAddMove()) return;
    qCDebug(lcApp).noquote() << "appendRecord: text=" << entry.displayText;
    RecordEntry recorded = entry;
    // 棋譜の途中から続けた対局では、消費時間の累計を同じ側の直前の累計から続ける
    recorded.time = m_deps.liveSession->continuedTime(recorded.time);
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
// 対局の開始
// ============================================================

void GameSession::startGameFromDialog()
{
    const bool hasEditedStart = isEditedStart(recordStartSfen()) || isEditedStart(m_deps.match->record().startSfen);

    StartGameDialog dlg;
    if (hasEditedStart) dlg.forceCurrentPositionSelection();
    if (dlg.exec() != QDialog::Accepted) return;

    GameStartRequest request;
    request.dialogData = dlg.data();
    if (request.dialogData.startingPositionNumber != 0 && !m_deps.kifuFileController->confirmDiscardUnsaved()) return;

    // 表示中の手から始める
    request.recordLine = m_deps.navState->currentRecordLine();
    request.startNodeId = m_deps.navState->currentNode() ? m_deps.navState->currentNode()->nodeId() : -1;
    request.bottomSide = m_deps.shogiView->flipMode() ? Turn::White : Turn::Black;

    QString error;
    const std::optional<GameStartPlan> plan = GameStartOptionsBuilder::plan(request, &error);
    if (!plan) {
        if (!error.isEmpty()) ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, error);
        return;
    }
    startGame(*plan, /*beginSeries=*/true);
}

bool GameSession::startGame(const GameStartPlan& plan, bool beginSeries)
{
    // 1) 棋譜を開始局面に整え、ライブ対局を始める（表示中の手が開始局面になる）
    if (!m_deps.reset->prepareGameStart(plan.preparation)) {
        qCWarning(lcApp) << "startGame: the start node no longer exists";
        return false;
    }
    if (plan.flipBoard) m_deps.appearance->flipBoard();
    if (beginSeries) m_deps.consecutive->beginSeries(plan);

    // 2) 時計
    TimeControlUtil::applyToClock(*m_deps.clock, plan.time, sideToMove(plan.options.sfenStart));
    m_deps.timePresenter->onMatchTimeUpdated(m_deps.clock->remainingMs(Turn::Black),
                                             m_deps.clock->remainingMs(Turn::White),
                                             m_deps.clock->currentPlayer(), /*urgencyMs*/ 0);

    // 3) 棋譜の文書に対局者・持ち時間・開始日時を記録し、対局情報を作る。
    //    読み込んだ棋譜の途中から続ける対局では、棋譜の対局情報を残して分岐の最初の手にメモを付ける
    const MatchPlayers& players = plan.options.players;
    const bool continuesRecord = m_deps.liveSession->isActive() && m_deps.liveSession->anchorPly() > 0;
    m_deps.document->beginMatch(players, plan.time.rules, continuesRecord);
    if (continuesRecord) noteContinuedMatch(players);
    m_deps.playerInfo->showPlayers(players);

    // 4) 対局（盤・手順・Strategy。動作状態は対局中になる）
    m_deps.match->start(plan.options);
    discardInterruptedGame();

    // 5) 時計を動かし、初手がエンジンなら思考を始める
    m_deps.match->beginPlay();
    return true;
}

void GameSession::noteContinuedMatch(const MatchPlayers& players)
{
    // 続けた対局の対局者・日時は分岐の最初の手のメモにする
    const MatchTimeRecord& record = m_deps.document->timeRecord();
    const bool handicap = KifuExportMetadataBuilder::isHandicap(KifuExportMetadataBuilder::handicapLabel(recordStartSfen()));
    const QString timeText = record.hasTimeControl()
        ? KifuExportMetadataBuilder::timeControlText(record.timeControl(), handicap) : tr("無制限");
    const QDateTime start = record.startDateTime().isValid() ? record.startDateTime() : QDateTime::currentDateTime();
    m_deps.liveSession->setFirstMoveNote(tr("対局：▲%1 △%2（%3 開始、持ち時間 %4）")
                                             .arg(players.name(Turn::Black), players.name(Turn::White),
                                                  start.toString(QStringLiteral("yyyy/MM/dd HH:mm")), timeText));
}

void GameSession::startNextGame(const GameStartPlan& plan)
{
    // 次局は独立した棋譜として、同じ開始局面から始める（上書き保存先も持たない）
    m_deps.reset->resetToNewRecord(plan.preparation.rootSfen);
    startGame(plan, /*beginSeries=*/false);
}

// ============================================================
// 対局中の操作
// ============================================================

void GameSession::resign()
{
    if (isCsaGameActive()) {
        m_deps.csaGame->onResign();
        return;
    }
    m_deps.match->handleResign();
}

void GameSession::breakOff()
{
    if (isCsaGameActive()) {
        m_deps.csaGame->requestChudan();
        return;
    }
    m_deps.match->handleBreakOff();
}

void GameSession::makeEngineMoveNow()
{
    if (isCsaGameActive()) {
        m_deps.csaGame->forceImmediateMove();
        return;
    }
    m_deps.match->forceImmediateMove();
}

void GameSession::undoTwoMoves()
{
    // 未保存かどうかはツリーの変更で決まる
    (void)m_deps.match->undoTwoPlies();
}

// ============================================================
// 通信対局
// ============================================================

void GameSession::beginCsaGameRecord(const QString& startSfen, const QString& blackName, const QString& whiteName,
                                     const TimeControl& timeControl)
{
    // 通信対局の対局者はサーバーが知らせた名前（エンジンで指していても対局者名で記録する）
    MatchPlayers players;
    players.blackHumanName = blackName;
    players.whiteHumanName = whiteName;

    // 利用者が対局情報に書きかけた内容（棋戦・場所など）は、棋譜の作り直しで消さない。
    // 持ち時間と開始日時はサーバーの対局条件
    m_deps.reset->resetToNewRecord(startSfen, /*keepHeader=*/true);
    m_deps.document->beginMatch(players, timeControl, /*continuesRecord=*/false);
    m_deps.liveSession->startFromRoot();
}

// ============================================================
// 新規
// ============================================================

void GameSession::resetToInitialState()
{
    m_deps.analysis->stop();
    if (m_deps.csaGame) m_deps.csaGame->stopGame();
    m_deps.match->clearGameOverState();
    m_deps.consecutive->reset();
    discardInterruptedGame();

    m_deps.reset->clearBeforeKifuLoad();
    // 前の対局者・持ち時間・上書き保存先も残さない（盤には対局情報の初期値「先手」「後手」を出す）
    m_deps.reset->resetToNewRecord(SfenUtils::hirateSfen());
    m_deps.state->activity.setActivity(AppActivityState::Activity::Idle);
}

// ============================================================
// 終局
// ============================================================

void GameSession::onGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    qCDebug(lcApp).nospace() << "onGameEnded cause=" << static_cast<int>(info.cause) << " loser=" << info.loser;
    // 中断した対局は再開できるよう、ライブ対局を確定しない（再開で中断の行を除く）
    if (info.cause == MatchCoordinator::Cause::BreakOff) {
        m_deps.consecutive->reset();
    } else {
        m_deps.liveSession->commit();
    }
    // 終局時刻の記録とゲーム情報への反映（投了手を含めるため、終局手の追記の後で行う）
    recordGameEndTime();
}

void GameSession::endCsaGameRecord()
{
    // 終局行は司令塔が追記済み。確定しないと、変化の手・分岐の編集・評価値グラフの追従が止まったままになる
    m_deps.liveSession->commit();
    recordGameEndTime();
}

void GameSession::recordGameEndTime()
{
    m_deps.document->recordMatchEnd();
}

void GameSession::onGameEndProcessed(const MatchCoordinator::GameEndInfo& info)
{
    if (info.cause == MatchCoordinator::Cause::BreakOff) return;
    // 終局の知らせ・保存が終わってから次局を予約する（エンジン同士の対局のみ）
    if (m_deps.match->players().engineVsEngine() && m_deps.consecutive->shouldStartNextGame()) {
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
    KifuBranchTree* tree = m_deps.navState->tree();
    KifuBranchNode* terminal = tree->nodeAt(m_interruptedNodeId);
    if (!terminal || !terminal->isTerminal() || !terminal->parent()) {
        discardInterruptedGame();
        return;
    }
    KifuBranchNode* node = terminal->parent();
    // 再開する対局は m_interruptedNodeId を外しておき、中断の行を除く棋譜の変更で捨てない
    m_interruptedNodeId = -1;
    m_deps.uiStatePolicy->setResumeAvailable(false);

    // 閲覧位置にかかわらず中断した手の直前へ戻し、中断の行を除いて対局を続ける
    m_deps.navState->resetToNode(node);
    m_deps.liveSession->discard();
    tree->removeLeaf(terminal);
    m_deps.liveSession->startFromNode(node);

    // 終局時刻を消す（中断の行を除いたツリーの変更で未保存になる）
    m_deps.document->clearMatchEnd();
    if (!m_deps.match->resumeInterruptedGame()) return;
    m_deps.match->beginPlay();
}
