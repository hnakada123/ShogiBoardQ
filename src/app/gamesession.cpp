/// @file gamesession.cpp
/// @brief 対局の利用者の操作・開始の手順・棋譜の行・終局と中断の後処理

#include "gamesession.h"
#include "enginelifecyclemanager.h"
#include "josekibooksession.h"

#include "analysissession.h"
#include "appactivitystate.h"
#include "consecutivegameseries.h"
#include "csagamechannel.h"
#include "csagamecoordinator.h"
#include "errorbus.h"
#include "gamechannel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuexportmetadata.h"
#include "kifufilecontroller.h"
#include "kifunavigationstate.h"
#include "livegamesession.h"
#include "logcategories.h"
#include "mainwindowappearancecontroller.h"
#include "nyugyokudeclarationhandler.h"
#include "playerinfopresenter.h"
#include "sessionresetcoordinator.h"
#include "sfenutils.h"
#include "timecontroltext.h"
#include "shogiclock.h"
#include "shogiview.h"
#include "startgamedialog.h"
#include "timecontrolutil.h"
#include "timedisplaypresenter.h"

#include <QDateTime>
#include <QFileInfo>

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
    Q_ASSERT(m_deps.activity && m_deps.match && m_deps.clock && m_deps.csaGame);
    m_localChannel = std::make_unique<LocalGameChannel>(m_deps.match, m_deps.clock);
    m_csaChannel = std::make_unique<CsaGameChannel>(m_deps.csaGame);

    // 棋譜を変えたら、中断した対局は再開できない
    KifuBranchTree* tree = m_deps.navState->tree();
    connect(tree, &KifuBranchTree::nodeAppended, this, &GameSession::discardInterruptedGame);
    connect(tree, &KifuBranchTree::childrenReordered, this, &GameSession::discardInterruptedGame);
    connect(tree, &KifuBranchTree::subtreeRemoved, this, &GameSession::discardInterruptedGame);
    connect(tree, &KifuBranchTree::reset, this, &GameSession::discardInterruptedGame);

    // 通信対局の棋譜（画面への反映は CsaGameSession。棋譜を先に作るため、CsaGameSession より先に接続する）
    CsaGameCoordinator* const csa = m_deps.csaGame;
    connect(csa, &CsaGameCoordinator::gameStarted, this, &GameSession::onCsaGameStarted);
    connect(csa, &CsaGameCoordinator::recordEntryReady, this, &GameSession::onCsaRecordEntry);
    connect(csa, &CsaGameCoordinator::gameEnded, this, &GameSession::onCsaGameEnded);
}

GameSession::~GameSession() = default;

GameChannel* GameSession::activeChannel() const
{
    const AppActivityState& activity = *m_deps.activity;
    if (activity.is(AppActivityState::Activity::DuringCsaGame)) return m_csaChannel.get();
    if (activity.is(AppActivityState::Activity::DuringGame)) return m_localChannel.get();
    return nullptr;
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
    // 終局でも指し手でもない行（終局の種類が決まらなかった終局）は記録しない
    if ((!entry.isTerminal() && !entry.move.move.isValid()) || !m_deps.liveSession->canAddMove()) return;
    qCDebug(lcApp).noquote() << "appendRecord: text=" << entry.displayText;
    RecordEntry recorded = entry;
    // 棋譜の途中から続けた対局では、消費時間の累計を同じ側の直前の累計から続ける
    recorded.time = m_deps.liveSession->continuedTime(recorded.time);
    const KifuBranchNode* before = m_deps.liveSession->liveNode();
    const QString sfenBefore = before ? before->sfen() : QString();
    m_deps.liveSession->appendRecord(recorded);
    if (!recorded.isTerminal() && recorded.move.move.isValid() && !sfenBefore.isEmpty())
        emit gameMoveRecorded(sfenBefore, recorded.move.move.toString());
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
    dlg.setJosekiBookName(josekiBookName());
    if (dlg.exec() != QDialog::Accepted) return;

    GameStartRequest request;
    request.dialogData = dlg.data();
    if (!request.dialogData.startsFromCurrentPosition() && !m_deps.kifuFileController->confirmDiscardUnsaved()) return;

    // 表示中の手から始める
    request.recordLine = m_deps.navState->currentRecordLine();
    request.startNodeId = m_deps.navState->currentNode() ? m_deps.navState->currentNode()->nodeId() : -1;
    request.bottomSide = m_deps.shogiView->flipMode() ? Turn::White : Turn::Black;

    QString error;
    std::optional<GameStartPlan> plan = GameStartOptionsBuilder::plan(request, &error);
    if (!plan) {
        if (!error.isEmpty()) ErrorBus::instance().postMessage(ErrorBus::ErrorLevel::Warning, error);
        return;
    }
    // 定跡を使うエンジンには、始めた時点の定跡の写しを渡す（対局中に定跡ウィンドウで変えても影響しない）
    const auto& joseki = plan->options.josekiOptions;
    if (m_deps.josekiBook && (joseki[0].enabled || joseki[1].enabled))
        plan->options.josekiBook = m_deps.josekiBook->snapshot();
    startGame(*plan, /*beginSeries=*/true);
}

QString GameSession::josekiBookName() const
{
    if (!m_deps.josekiBook || m_deps.josekiBook->isEmpty()) return QString();
    const QString path = m_deps.josekiBook->filePath();
    return path.isEmpty() ? tr("保存していない定跡") : QFileInfo(path).fileName();
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
    m_deps.timePresenter->showClock();

    // 3) 棋譜の文書に対局者・持ち時間・開始日時を記録し、対局情報を作る。
    //    読み込んだ棋譜の途中から続ける対局では、棋譜の対局情報を残して分岐の最初の手にメモを付ける
    const bool continuesRecord = m_deps.liveSession->isActive() && m_deps.liveSession->anchorPly() > 0;
    beginMatchRecord(plan.options.players, plan.time.rules, continuesRecord);
    m_autoSave = plan.options.autoSave;

    // 4) 対局（盤・手順・Strategy。動作状態は対局中になる）
    m_deps.match->start(plan.options);
    discardInterruptedGame();

    // 5) 時計を動かし、初手がエンジンなら思考を始める
    m_deps.match->beginPlay();
    return true;
}

void GameSession::beginMatchRecord(const MatchPlayers& players, const TimeControl& timeControl,
                                   bool continuesRecord)
{
    m_deps.document->beginMatch(players, timeControl, continuesRecord);
    if (continuesRecord) noteContinuedMatch(players);
    m_deps.playerInfo->showPlayers(players);
}

void GameSession::noteContinuedMatch(const MatchPlayers& players)
{
    // 続けた対局の対局者・日時は分岐の最初の手のメモにする
    const MatchTimeRecord& record = m_deps.document->timeRecord();
    const bool handicap = KifuExportMetadataBuilder::isHandicap(KifuExportMetadataBuilder::handicapLabel(recordStartSfen()));
    const QString timeText = record.hasTimeControl()
        ? TimeControlText::format(record.timeControl(), handicap) : tr("無制限");
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
    if (GameChannel* channel = activeChannel()) channel->resign();
}

void GameSession::breakOff()
{
    if (GameChannel* channel = activeChannel()) channel->breakOff();
}

void GameSession::makeEngineMoveNow()
{
    if (GameChannel* channel = activeChannel()) channel->moveNow();
}

void GameSession::declareNyugyoku()
{
    m_deps.nyugyoku->handleDeclaration(activeChannel());
}

void GameSession::undoTwoMoves()
{
    // 未保存かどうかはツリーの変更で決まる
    (void)m_deps.match->undoTwoPlies();
}

// ============================================================
// 通信対局
// ============================================================

void GameSession::onCsaGameStarted(const QString& blackName, const QString& whiteName,
                                   const QList<RecordEntry>& initialRecords)
{
    qCDebug(lcApp) << "onCsaGameStarted:" << blackName << "vs" << whiteName;
    // 前の対局・棋譜解析の評価値グラフ・思考・解析結果を消す（ローカル対局の開始と同じ）
    m_deps.reset->clearBeforeKifuLoad();
    // 利用者が対局情報に書きかけた内容（棋戦・場所など）は、棋譜の作り直しで消さない。
    // 対局者・持ち時間・開始日時はサーバーの対局条件
    m_deps.reset->resetToNewRecord(m_deps.csaGame->startSfen(), /*keepHeader=*/true);
    m_deps.liveSession->startFromRoot();
    beginMatchRecord(m_csaChannel->players(), m_csaChannel->timeControl(), /*continuesRecord=*/false);
    m_autoSave = m_deps.csaGame->options().autoSave;
    // 再開局面の既存手順を棋譜へ入れる（以後の着手は recordEntryReady で届く）
    for (const RecordEntry& entry : initialRecords) appendRecord(entry);
}

void GameSession::onCsaRecordEntry(const RecordEntry& entry)
{
    appendRecord(entry);
}

void GameSession::onCsaGameEnded(CsaClient::GameResult result, CsaClient::GameEndCause cause, int consumedTimeMs)
{
    Q_UNUSED(result)
    Q_UNUSED(cause)
    Q_UNUSED(consumedTimeMs)
    // 終局行は司令塔が recordEntryReady で渡し済み。次の対局が始まる前に確定して書き出す
    finishMatchRecord(/*commit=*/true);
}

// ============================================================
// 新規
// ============================================================

void GameSession::resetToInitialState()
{
    m_deps.analysis->stop();
    if (m_deps.csaGame) m_deps.csaGame->stopGame();
    // 対局・棋譜解析のエンジンも止める（思考情報の表示は作り直しで消す）
    if (m_deps.engines) m_deps.engines->destroyEngines(false);
    m_deps.match->clearGameOverState();
    m_deps.consecutive->reset();
    discardInterruptedGame();

    m_deps.reset->clearBeforeKifuLoad();
    // 前の対局者・持ち時間・上書き保存先も残さない（盤には対局情報の初期値「先手」「後手」を出す）
    // 動作状態は、止めた検討・通信対局・解析がそれぞれ手放す（「新規」は待機中だけ）
    m_deps.reset->resetToNewRecord(SfenUtils::hirateSfen());
}

// ============================================================
// 終局
// ============================================================

void GameSession::onGameEnded(const MatchCoordinator::GameEndInfo& info)
{
    qCDebug(lcApp).nospace() << "onGameEnded cause=" << static_cast<int>(info.cause) << " loser=" << info.loser;
    // 中断した対局は再開できるよう、ライブ対局を確定せず、自動保存もしない（再開で中断の行を除く）
    const bool breakOff = info.cause == MatchCoordinator::Cause::BreakOff;
    if (breakOff) m_deps.consecutive->reset();
    finishMatchRecord(/*commit=*/!breakOff);
}

void GameSession::finishMatchRecord(bool commit)
{
    // 終局の行は追記済み。確定しないと、変化の手・分岐の編集・評価値グラフの追従が止まったままになる
    if (commit) m_deps.liveSession->commit();
    // 終局時刻の記録と対局情報への反映（投了手を含めるため、終局手の追記の後で行う）
    m_deps.document->recordMatchEnd();
    // 連続対局の次局・通信対局の次の対局が始まる前に書き出す
    if (commit && m_autoSave.isActive())
        m_deps.kifuFileController->autoSaveKifu(m_autoSave.dir, m_autoSave.extension);
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
    // 中断の行（表示中の手）を司令塔の中断した対局に結びつける。以後、棋譜を変えたら再開できない
    const KifuBranchNode* node = m_deps.navState->currentNode();
    if (!m_deps.match->interruptedGame() || !node || !node->isTerminal()) return;
    m_deps.match->attachInterruptedRecord(node->nodeId());
}

void GameSession::discardInterruptedGame()
{
    // 中断の行を書く前の棋譜の変更（中断の行の追記そのもの）では捨てない
    if (!m_deps.match->canResumeInterruptedGame()) return;
    m_deps.match->discardInterruptedGame();
}

void GameSession::resumeInterruptedGame()
{
    if (!m_deps.match->canResumeInterruptedGame()) return;
    KifuBranchTree* tree = m_deps.navState->tree();
    KifuBranchNode* terminal = tree->nodeAt(m_deps.match->interruptedGame()->terminalNodeId);
    if (!terminal || !terminal->isTerminal() || !terminal->parent()) {
        discardInterruptedGame();
        return;
    }
    KifuBranchNode* node = terminal->parent();
    // 再開する対局は中断の行との結びつきを外し、中断の行を除く棋譜の変更で捨てない
    m_deps.match->attachInterruptedRecord(-1);

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
