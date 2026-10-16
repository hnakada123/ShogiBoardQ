/// @file matchcoordinator.cpp
/// @brief ローカル対局の司令塔: 開始・一手の確定・エンジンへの思考の依頼・時計の通知

#include "matchcoordinator.h"

#include <QSignalBlocker>
#include <limits>

#include "appactivitystate.h"

#include "enginelifecyclemanager.h"
#include "enginevsenginestrategy.h"
#include "humanvsenginestrategy.h"
#include "humanvshumanstrategy.h"
#include "logcategories.h"
#include "matchrecordsink.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "usi.h"
#include "usimatchhandler.h"
#include "nyugyokujudgement.h"
#include "sennichitedetector.h"

// ============================================================
// 初期化
// ============================================================

MatchCoordinator::MatchCoordinator(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_gc(deps.gc)
    , m_clock(deps.clock)
    , m_activity(deps.activity)
    , m_engines(deps.engines)
{
    Q_ASSERT(m_gc && m_clock && m_activity && m_engines);

    connect(m_engines, &EngineLifecycleManager::engineResigned, this, &MatchCoordinator::handleEngineResign);
    connect(m_engines, &EngineLifecycleManager::engineDeclaredWin, this, &MatchCoordinator::handleEngineWin);
    // 起動の失敗は対局開始の処理中に届くことがあるため、開始処理を終えてから中断する
    connect(m_engines, &EngineLifecycleManager::engineError,
            this, &MatchCoordinator::handleEngineError, Qt::QueuedConnection);
    connect(m_clock, &ShogiClock::timeUpdated, this, &MatchCoordinator::emitTimeUpdate);
    connect(m_clock, &ShogiClock::playerTimeOut, this, &MatchCoordinator::handlePlayerTimeOut);
}

MatchCoordinator::~MatchCoordinator() = default;

// ============================================================
// 開始
// ============================================================

void MatchCoordinator::start(const StartOptions& opt)
{
    discardInterruptedGame();

    StartOptions resolved = opt;
    if (resolved.recordLine.isEmpty()) {
        const QString initial = SfenUtils::normalizePositionLikeSfen(opt.sfenStart);
        resolved.recordLine.startSfen = initial.isEmpty() ? SfenUtils::hirateSfen() : initial;
    }
    resolved.recordLine = resolved.recordLine.upTo(resolved.recordLine.currentPly);
    resolved.sfenStart = resolved.recordLine.lastSfen();

    clearGameOverState();
    m_session.begin(resolved);
    m_record = resolved.recordLine;
    m_activity->setActivity(AppActivityState::Activity::DuringGame);

    m_gc->newGame(resolved.sfenStart);
    m_playing = false;
    m_waitingForEngines = false;
    createStrategy(resolved);
}

void MatchCoordinator::beginPlay()
{
    m_playing = true;
    if (!m_waitingForEngines) m_clock->startClock();
    if (m_strategy) m_strategy->startInitialMoveIfNeeded();
}

void MatchCoordinator::setWaitingForEngines(bool waiting)
{
    m_waitingForEngines = waiting;
    if (waiting) m_clock->stopClock();
    else if (m_playing && !isGameOver()) m_clock->startClock();
}

Turn MatchCoordinator::sideToMove() const
{
    return m_gc->currentPlayer();
}

void MatchCoordinator::recordConsiderationTime(Turn mover, qint64 elapsedMs)
{
    m_clock->setMeasuredConsiderationTime(mover, elapsedMs);
}

void MatchCoordinator::createStrategy(const StartOptions& opt)
{
    m_strategy.reset();
    const MatchPlayers& players = opt.players;
    if (players.engineVsEngine()) {
        m_strategy = std::make_unique<EngineVsEngineStrategy>(
            strategyHost(), EngineVsEngineStrategy::Engines{opt.enginePath1, players.blackEngineName,
                                                            opt.enginePath2, players.whiteEngineName});
    } else if (players.blackIsEngine) {
        m_strategy = std::make_unique<HumanVsEngineStrategy>(strategyHost(), Turn::Black, opt.enginePath1,
                                                             players.blackEngineName);
    } else if (players.whiteIsEngine) {
        m_strategy = std::make_unique<HumanVsEngineStrategy>(strategyHost(), Turn::White, opt.enginePath2,
                                                             players.whiteEngineName);
    } else {
        m_strategy = std::make_unique<HumanVsHumanStrategy>(strategyHost());
    }
    m_strategy->start();
}

bool MatchCoordinator::isHumanSide(Turn side) const
{
    return m_session.started() && players().isHuman(side);
}

// ============================================================
// 着手
// ============================================================

bool MatchCoordinator::canAcceptMove() const
{
    return !isGameOver() && (!m_sink || m_sink->canAppendRecord());
}

void MatchCoordinator::onHumanMove(const RecordEntry& played)
{
    if (m_strategy) m_strategy->onHumanMove(played);
}

bool MatchCoordinator::completeHumanMove(const RecordEntry& played)
{
    return completeMove(played, std::nullopt);
}

bool MatchCoordinator::applyEngineMove(const UsiMatchMove& move)
{
    if (isGameOver() || (m_sink && !m_sink->canAppendRecord())) return false;
    const std::optional<RecordEntry> played = m_gc->applyMove(move.move);
    if (!played) {
        handleEngineIllegalMove(move.side);
        return false;
    }
    // 次の手番の思考で先読みの当たり・外れを判断する
    m_session.setPonder(move.side, move.ponderPosition);
    m_clock->setMeasuredConsiderationTime(move.side, move.elapsedMs);
    return completeMove(*played, move.score);
}

bool MatchCoordinator::completeMove(RecordEntry played, std::optional<UsiScore> engineScore)
{
    if (isGameOver()) return false;
    const Turn mover = played.mover;
    {
        // 時計の値は一手を確定し終えてからまとめて知らせる
        const QSignalBlocker clockNotifications(m_clock);
        m_clock->commitMove(mover);
        played.time = m_clock->recordTime(mover);
        m_clock->setConsiderationTimeMs(mover, 0);
        m_clock->setCurrentPlayer(m_gc->currentPlayer());
    }
    // 棋譜に書くと表示中の手が最新の手になり、盤・ハイライト・手番の表示が合う
    m_record.entries.append(played);
    if (m_sink) m_sink->appendRecord(played);
    pokeTimeUpdateNow();
    if (engineScore) emit engineMovePlayed(mover, currentPly(), *engineScore);
    m_gc->notifyMoveCommitted(mover, currentPly());

    if (checkAndHandleSennichite()) return false;
    if (m_session.maxMoves() > 0 && currentPly() >= m_session.maxMoves()) {
        handleMaxMovesJishogi();
        return false;
    }
    return !isGameOver();
}

void MatchCoordinator::requestEngineMove(Turn side)
{
    Usi* engine = m_engines->engineFor(side);
    if (!engine) return;
    engine->requestMatchMove(m_record.positionCommand(currentPly()), m_session.ponder(side), {});
}

void MatchCoordinator::forceImmediateMove()
{
    if (isGameOver()) return;
    // 人間の手番に「すぐ指させる」を押しても先読みは止めない
    if (Usi* engine = m_engines->engineFor(m_gc->currentPlayer())) engine->requestImmediateMove();
}

void MatchCoordinator::disarmTurnTimer()
{
    if (m_strategy) m_strategy->disarmTurnTimer();
}

// ============================================================
// エンジン・時計
// ============================================================

void MatchCoordinator::pokeTimeUpdateNow()
{
    emitTimeUpdate();
}

void MatchCoordinator::emitTimeUpdate()
{
    const qint64 blackMs = m_clock->remainingMs(Turn::Black);
    const qint64 whiteMs = m_clock->remainingMs(Turn::White);
    const Turn side = m_gc->currentPlayer();
    const bool enableUrgency = !m_clock->hasByoyomi(side) || m_clock->byoyomiApplied(side);
    const qint64 activeMs = (side == Turn::Black) ? blackMs : whiteMs;
    emit timeUpdated(blackMs, whiteMs, side, enableUrgency ? activeMs : std::numeric_limits<qint64>::max());
}

// ============================================================
// ローカル対局の司令塔: 終局の判定・記録・知らせ
// ============================================================

// ============================================================
// 終局の原因ごとの処理
// ============================================================

void MatchCoordinator::handleResign()
{
    if (isGameOver()) return;
    const Turn loser = m_gc->currentPlayer();
    endGame({Cause::Resignation, loser}, loser, Notice::Announce);
}

void MatchCoordinator::handleEngineResign(Turn side)
{
    if (isGameOver()) return;
    endGame({Cause::Resignation, side}, side, Notice::Announce);
}

void MatchCoordinator::handleEngineIllegalMove(Turn side)
{
    if (isGameOver()) return;
    // エンジンが反則手を返した。盤には指さずに、そのエンジンの反則負けとする
    endGame({Cause::IllegalMove, side}, side, Notice::Announce);
}

void MatchCoordinator::handlePlayerTimeOut(Turn player)
{
    if (isGameOver()) return;
    endGame({Cause::Timeout, player}, player, Notice::Announce);
}

void MatchCoordinator::handleEngineWin(Turn side)
{
    if (isGameOver()) return;
    // エンジンの入玉宣言（bestmove win）も人間の宣言と同じく、盤面から条件と点数を判定する。
    // 条件を満たさなければ宣言したエンジンの負け、24点法の24〜30点は持将棋。
    const NyugyokuJudgement::Result result =
        NyugyokuJudgement::judgeEngineDeclaration(*m_gc->board(), side == Turn::Black, m_handicapNames);
    handleNyugyokuDeclaration(side, result.success, result.isDraw);
    // 人間の宣言は宣言の結果ダイアログで知らせる。エンジンの宣言はここで判定結果を知らせる
    if (isGameOver()) emit gameOverMessage(NyugyokuJudgement::resultTitle(), result.message);
}

void MatchCoordinator::handleNyugyokuDeclaration(Turn declarer, bool success, bool isDraw)
{
    if (isGameOver()) return;
    qCInfo(lcGame) << "handleNyugyokuDeclaration(): declarer=" << declarer
                   << " success=" << success << " isDraw=" << isDraw;
    if (isDraw) {
        endGame({Cause::Jishogi, declarer}, std::nullopt, Notice::Declaration);
    } else if (success) {
        endGame({Cause::NyugyokuWin, opponent(declarer)}, opponent(declarer), Notice::Declaration);
    } else {
        endGame({Cause::IllegalMove, declarer}, declarer, Notice::Declaration);
    }
}

void MatchCoordinator::handleBreakOff()
{
    if (isGameOver()) return;
    captureInterruptedGame();
    // 時計の精算時に時間切れが確定した場合、中断に上書きしない。
    if (isGameOver()) return;
    // 中断の後に届く bestmove resign を終局にしない
    m_engines->disconnectArbiterSignals();
    // quit に応答しない場合も terminate/kill まで進める。中断後の思考表示は保持する。
    endGame({Cause::BreakOff, m_gc->currentPlayer()}, std::nullopt, Notice::Interrupt);
}

void MatchCoordinator::handleMaxMovesJishogi()
{
    if (isGameOver()) return;
    qCInfo(lcGame) << "handleMaxMovesJishogi(): max moves reached";
    endGame({Cause::Jishogi, Turn::Black}, std::nullopt, Notice::Announce);
}

bool MatchCoordinator::checkAndHandleSennichite()
{
    if (isGameOver() || m_record.moveCount() < 3) return false;
    switch (SennichiteDetector::check(m_record.sfens())) {
    case SennichiteDetector::Result::None:
        return false;
    case SennichiteDetector::Result::Draw:
        qCInfo(lcGame) << "draw by repetition";
        endGame({Cause::Sennichite, Turn::Black}, std::nullopt, Notice::Announce);
        return true;
    case SennichiteDetector::Result::ContinuousCheckByBlack:
        endGame({Cause::OuteSennichite, Turn::Black}, Turn::Black, Notice::Announce);
        return true;
    case SennichiteDetector::Result::ContinuousCheckByWhite:
        endGame({Cause::OuteSennichite, Turn::White}, Turn::White, Notice::Announce);
        return true;
    }
    return false;
}

void MatchCoordinator::handleEngineError(const QString& message)
{
    if (isGameOver() || !m_strategy) return;
    qCWarning(lcGame).noquote() << "[USI-ERROR]" << message;
    for (Usi* engine : {m_engines->engineInSlot(1), m_engines->engineInSlot(2)}) {
        if (engine) engine->cancelCurrentOperation();
    }
    if (!players().hasEngine()) return;
    handleBreakOff();
    emit gameOverMessage(tr("対局中断"), tr("エンジンエラー: %1").arg(message));
}

// ============================================================
// 終局の記録
// ============================================================

void MatchCoordinator::clearGameOverState()
{
    m_session.clearGameOver();
}

void MatchCoordinator::endGame(const GameEndInfo& info, std::optional<Turn> engineLoser, Notice notice)
{
    if (!m_session.finish(info)) {
        qCDebug(lcGame) << "endGame() ignored: already over";
        return;
    }
    // 時計を止める。終局の行の時間を確定しても、秒読み・加算はしない
    m_clock->markGameOver();
    m_clock->stopClock();
    disarmTurnTimer();
    m_engines->notifyGameOver(engineLoser);
    appendTerminalRecord(info);
    m_activity->setActivity(AppActivityState::Activity::Idle);
    // 終局の行を棋譜に書いた後で、時刻と UI を更新させる
    emit gameEnded(info);

    switch (notice) {
    case Notice::Announce:
        emit gameOverMessage(tr("対局終了"), resultMessage(info));
        [[fallthrough]];
    case Notice::Declaration:
        // 入玉宣言は宣言の結果ダイアログ（人間）か handleEngineWin（エンジン）で知らせる
        if (m_session.options().autoSaveKifu && !m_session.options().kifuSaveDir.isEmpty()) {
            qCInfo(lcGame) << "auto save: dir=" << m_session.options().kifuSaveDir;
            emit autoSaveRequested(m_session.options().kifuSaveDir);
        }
        emit gameEndProcessed(info);
        break;
    case Notice::Interrupt:
        if (m_interruptedGame) emit gameInterrupted();
        break;
    }
}

void MatchCoordinator::appendTerminalRecord(const GameEndInfo& info)
{
    const Turn sideToMove = m_gc->currentPlayer();
    TerminalType terminal = TerminalType::None;
    switch (info.cause) {
    case Cause::Jishogi: terminal = TerminalType::Impasse; break;
    case Cause::NyugyokuWin: terminal = TerminalType::DeclarationWin; break;
    case Cause::IllegalMove: terminal = TerminalType::IllegalLoss; break;
    case Cause::Sennichite: terminal = TerminalType::Repetition; break;
    case Cause::OuteSennichite:
        // 連続王手の千日手は終局した局面の手番側の行（王手をかけていた側の反則負け）
        terminal = info.loser == sideToMove ? TerminalType::IllegalLoss : TerminalType::IllegalWin;
        break;
    case Cause::BreakOff: terminal = TerminalType::Interrupt; break;
    case Cause::Resignation: terminal = TerminalType::Resign; break;
    case Cause::Timeout: terminal = TerminalType::Timeout; break;
    }

    // 終局行の印と消費時間は、その行（投了・宣言など）を行った側のもの
    const Turn mover = RecordEntry::endingSide(terminal, sideToMove, info.loser);
    m_clock->commitMove(mover);
    const RecordEntry entry = RecordEntry::ending(terminal, mover, m_clock->recordTime(mover));
    if (m_sink && m_sink->canAppendRecord()) m_sink->appendRecord(entry);
    m_session.markTerminalRecorded();
}

QString MatchCoordinator::resultMessage(const GameEndInfo& info) const
{
    // 駒落ちは棋譜の局面図・終局行と同じく下手・上手と呼ぶ
    const QString blackJP = m_handicapNames ? tr("下手") : tr("先手");
    const QString whiteJP = m_handicapNames ? tr("上手") : tr("後手");
    const QString loserJP = (info.loser == Turn::Black) ? blackJP : whiteJP;
    const QString winnerJP = (info.loser == Turn::Black) ? whiteJP : blackJP;

    switch (info.cause) {
    case Cause::Resignation:
        return tr("%1の投了。%2の勝ちです。").arg(loserJP, winnerJP);
    case Cause::Timeout:
        return tr("%1の時間切れ。%2の勝ちです。").arg(loserJP, winnerJP);
    case Cause::Jishogi:
        return tr("最大手数に達しました。持将棋です。");
    case Cause::IllegalMove:
        return tr("%1の反則負け。%2の勝ちです。").arg(loserJP, winnerJP);
    case Cause::Sennichite:
        return tr("千日手が成立しました。");
    case Cause::OuteSennichite:
        return tr("%1の連続王手の千日手。%2の勝ちです。").arg(loserJP, winnerJP);
    case Cause::NyugyokuWin:  // 入玉宣言は宣言の判定結果で知らせる
    case Cause::BreakOff:     // 中断は知らせない
        break;
    }
    return tr("対局が終了しました。");
}

// ============================================================
// ローカル対局の司令塔: 待ったと、中断した対局の保存・再開
// ============================================================

// ============================================================
// 待った
// ============================================================

bool MatchCoordinator::undoTwoPlies()
{
    // 開始局面を残すために2手分の手順が要る。人間の手番のときだけ戻す
    if (isGameOver() || m_record.moveCount() < 2 || !m_gc->board()) return false;
    if (!isHumanSide(m_gc->currentPlayer())) return false;
    if (m_sink && !m_sink->canUndoRecords(2)) return false;

    {
        const QSignalBlocker clockNotifications(m_clock);

        const int targetPly = currentPly() - 2;

        // 盤面と手番を戻す。「同」表記の基準は、戻した局面に至った手の移動先
        m_gc->board()->setSfen(m_record.sfenAt(targetPly));
        m_gc->setForcedPromotion(false);
        m_gc->setPreviousMoveDestination(m_record.destinationOf(targetPly));
        m_record.entries.resize(targetPly);

        // 時計を取り消した自分の手を指し始めたときへ戻し、考慮時間は今から測り直す
        m_clock->undo();
        if (m_strategy) {
            m_strategy->disarmTurnTimer();
            m_strategy->armTurnTimerIfNeeded();
        }
        if (m_sink) m_sink->undoRecords(2);
        m_session.clearPonder();
    }
    emit movesUndone(currentPly());
    return true;
}

// ============================================================
// 中断・再開
// ============================================================

void MatchCoordinator::captureInterruptedGame()
{
    if (!m_session.started()) return;
    if (m_record.isEmpty()) return;

    auto state = std::make_unique<InterruptedGameState>();
    // 時計を止めて精算する（この間に時間切れになれば、中断ではなく時間切れで終局する）
    state->clock = m_clock->pauseAndSnapshot();
    if (isGameOver()) return;
    state->options = m_session.options();
    state->options.sfenStart = m_record.lastSfen();
    state->options.resumeFromBreakOff = true;
    state->record = m_record;
    state->previousTo = m_gc->previousMoveDestination();
    m_interruptedGame = std::move(state);
}

void MatchCoordinator::discardInterruptedGame()
{
    m_interruptedGame.reset();
}

bool MatchCoordinator::resumeInterruptedGame()
{
    if (!m_interruptedGame || !m_gc->board()) return false;
    const auto state = std::move(m_interruptedGame);

    clearGameOverState();
    m_session.begin(state->options);
    m_activity->setActivity(AppActivityState::Activity::DuringGame);
    m_record = state->record;
    m_gc->board()->setSfen(state->options.sfenStart);
    m_gc->setPreviousMoveDestination(state->previousTo);

    // 時計のスナップショットは持ち時間の決まりと時間切れの扱いも含む
    m_clock->restoreSnapshot(state->clock);

    createStrategy(state->options);
    if (m_strategy) m_strategy->armTurnTimerIfNeeded();
    // 対局開始通知でUIを整えてから、呼び出し側が初手探索を起動する。
    return true;
}
