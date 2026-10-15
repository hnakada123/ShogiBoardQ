/// @file matchcoordinator.cpp
/// @brief ローカル対局の司令塔: 開始・一手の確定・エンジンへの思考の依頼・時計の通知

#include "matchcoordinator.h"

#include <QSignalBlocker>
#include <limits>

#include "appactivitystate.h"
#include "boardcoordinates.h"
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
    emit gamePrepared(resolved);

    createStrategy(resolved);
}

void MatchCoordinator::beginPlay()
{
    m_clock->startClock();
    if (m_strategy) m_strategy->startInitialMoveIfNeeded();
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
    return completeMove(played, false);
}

bool MatchCoordinator::applyEngineMove(const UsiMove& move, Turn mover, qint64 elapsedMs)
{
    if (isGameOver() || (m_sink && !m_sink->canAppendRecord())) return false;
    const auto update = m_gc->positionUpdate();
    const std::optional<RecordEntry> played = m_gc->applyMove(move);
    if (!played) {
        handleEngineIllegalMove(mover);
        return false;
    }
    m_clock->setMeasuredConsiderationTime(mover, elapsedMs);
    return completeMove(*played, true);
}

bool MatchCoordinator::completeMove(RecordEntry played, bool engineMove)
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
    m_record.entries.append(played);
    if (m_sink) m_sink->appendRecord(played);

    emit boardChanged();
    emit moveHighlightRequested(BoardCoordinates::fromSquare(played.move.move, mover),
                                BoardCoordinates::toSquare(played.move.move));
    emit turnChanged(m_gc->currentPlayer());
    pokeTimeUpdateNow();
    if (engineMove) emit engineMovePlayed(mover, currentPly());
    m_gc->notifyMoveCommitted(mover, currentPly());

    if (checkAndHandleSennichite()) return false;
    if (m_session.maxMoves() > 0 && currentPly() >= m_session.maxMoves()) {
        handleMaxMovesJishogi();
        return false;
    }
    return !isGameOver();
}

void MatchCoordinator::refreshBoardAndTurn()
{
    emit boardChanged();
    emit turnChanged(m_gc->currentPlayer());
}

void MatchCoordinator::requestEngineMove(Turn side)
{
    Usi* engine = m_engines->engineFor(side);
    if (!engine) return;
    engine->requestMatchMove(m_record.positionCommand(currentPly()), m_session.ponder(side), {});
}

void MatchCoordinator::setEnginePonder(Turn side, const QString& ponder)
{
    m_session.setPonder(side, ponder);
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

namespace {
Turn opponent(Turn side) { return side == Turn::Black ? Turn::White : Turn::Black; }
} // namespace

// ============================================================
// 終局の原因ごとの処理
// ============================================================

void MatchCoordinator::handleResign()
{
    if (isGameOver()) return;
    const GameEndInfo info{Cause::Resignation, m_gc->currentPlayer()};
    m_engines->notifyGameOver(info.loser);
    finishGame(info);
    announceResult(info);
}

void MatchCoordinator::handleEngineResign(Turn side)
{
    if (isGameOver()) return;
    m_clock->stopClock();
    const GameEndInfo info{Cause::Resignation, side};
    m_engines->notifyGameOver(info.loser);
    finishGame(info);
    announceResult(info);
}

void MatchCoordinator::handleEngineIllegalMove(Turn side)
{
    if (isGameOver()) return;
    m_clock->stopClock();
    // エンジンが反則手を返した。盤には指さずに、そのエンジンの反則負けとする
    const GameEndInfo info{Cause::IllegalMove, side};
    m_engines->notifyGameOver(info.loser);
    finishGame(info);
    announceResult(info);
}

void MatchCoordinator::handlePlayerTimeOut(Turn player)
{
    if (isGameOver()) return;
    const GameEndInfo info{Cause::Timeout, player};
    m_clock->markGameOver();
    m_clock->stopClock();
    disarmTurnTimer();
    m_engines->notifyGameOver(info.loser);
    finishGame(info);
    announceResult(info);
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
    disarmTurnTimer();
    m_clock->stopClock();

    GameEndInfo info;
    if (isDraw) {
        info = {Cause::Jishogi, declarer};
    } else if (success) {
        info = {Cause::NyugyokuWin, opponent(declarer)};
    } else {
        info = {Cause::IllegalMove, declarer};
    }
    m_engines->notifyGameOver(isDraw ? std::nullopt : std::optional<Turn>(info.loser));
    finishGame(info);
    if (m_session.options().autoSaveKifu && !m_session.options().kifuSaveDir.isEmpty())
        emit autoSaveRequested(m_session.options().kifuSaveDir);
    emit gameEndProcessed(info);
}

void MatchCoordinator::handleBreakOff()
{
    if (isGameOver()) return;
    captureInterruptedGame();
    // 時計の精算時に時間切れが確定した場合、中断に上書きしない。
    if (isGameOver()) return;
    // 中断の後に届く bestmove resign を終局にしない
    m_engines->disconnectArbiterSignals();

    const GameEndInfo info{Cause::BreakOff, m_gc->currentPlayer()};
    m_clock->markGameOver();
    m_clock->stopClock();
    disarmTurnTimer();
    // quit に応答しない場合も terminate/kill まで進める。中断後の思考表示は保持する。
    m_engines->notifyGameOver(std::nullopt);
    finishGame(info);
    if (m_interruptedGame) emit gameInterrupted();
}

void MatchCoordinator::handleMaxMovesJishogi()
{
    if (isGameOver()) return;
    qCInfo(lcGame) << "handleMaxMovesJishogi(): max moves reached";
    disarmTurnTimer();
    m_engines->notifyGameOver(std::nullopt);
    const GameEndInfo info{Cause::Jishogi, Turn::Black};
    finishGame(info);
    announceResult(info);
}

bool MatchCoordinator::checkAndHandleSennichite()
{
    if (isGameOver() || m_record.moveCount() < 3) return false;
    GameEndInfo info;
    std::optional<Turn> loser;
    switch (SennichiteDetector::check(m_record.sfens())) {
    case SennichiteDetector::Result::None:
        return false;
    case SennichiteDetector::Result::Draw:
        qCInfo(lcGame) << "draw by repetition";
        info = {Cause::Sennichite, Turn::Black};
        break;
    case SennichiteDetector::Result::ContinuousCheckByBlack:
        info = {Cause::OuteSennichite, Turn::Black};
        loser = Turn::Black;
        break;
    case SennichiteDetector::Result::ContinuousCheckByWhite:
        info = {Cause::OuteSennichite, Turn::White};
        loser = Turn::White;
        break;
    }
    disarmTurnTimer();
    m_engines->notifyGameOver(loser);
    finishGame(info);
    announceResult(info);
    return true;
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
    const bool wasOver = isGameOver();
    m_session.clearGameOver();
    if (wasOver) emit gameOverStateChanged(m_session.gameOver());
}

void MatchCoordinator::finishGame(const GameEndInfo& info)
{
    if (!m_session.finish(info)) {
        qCDebug(lcGame) << "finishGame() ignored: already over";
        return;
    }
    emit gameOverStateChanged(m_session.gameOver());
    appendTerminalRecord(info);
    // 終局の行を棋譜に書いた後で、時刻と UI を更新させる
    emit gameEnded(info);
}

void MatchCoordinator::appendTerminalRecord(const GameEndInfo& info)
{
    m_clock->stopClock();

    // 千日手・持将棋（引き分け）と連続王手の千日手の終局行は、終局した局面で手番の側の行として記録する。
    // 棋譜ファイルを読み込んだときも終局行には手番側の印が付くので、それと合わせる
    Turn lineOwner = info.loser;
    if (info.cause == Cause::Sennichite || info.cause == Cause::Jishogi || info.cause == Cause::OuteSennichite)
        lineOwner = m_gc->currentPlayer();

    TerminalType terminal = TerminalType::None;
    switch (info.cause) {
    case Cause::Jishogi: terminal = TerminalType::Impasse; break;
    case Cause::NyugyokuWin: terminal = TerminalType::DeclarationWin; break;
    case Cause::IllegalMove: terminal = TerminalType::IllegalLoss; break;
    case Cause::Sennichite: terminal = TerminalType::Repetition; break;
    case Cause::OuteSennichite:
        terminal = info.loser == lineOwner ? TerminalType::IllegalLoss : TerminalType::IllegalWin;
        break;
    case Cause::BreakOff: terminal = TerminalType::Interrupt; break;
    case Cause::Resignation: terminal = TerminalType::Resign; break;
    case Cause::Timeout: terminal = TerminalType::Timeout; break;
    }

    // 終局行の消費時間は、その行（投了・宣言など）を行った手番側のもの。
    // 入玉宣言勝ちでは宣言した勝者が手番側になる。
    const Turn mover = (info.cause == Cause::NyugyokuWin) ? opponent(info.loser) : lineOwner;
    m_clock->commitMove(mover);
    const RecordEntry entry = RecordEntry::ending(terminal, mover, m_clock->recordTime(mover));
    if (m_sink && m_sink->canAppendRecord()) m_sink->appendRecord(entry);
    disarmTurnTimer();

    if (m_session.markTerminalRecorded()) emit gameOverStateChanged(m_session.gameOver());
}

void MatchCoordinator::announceResult(const GameEndInfo& info)
{
    emit gameOverMessage(tr("対局終了"), resultMessage(info));
    qCDebug(lcGame) << "Game ended";
    if (m_session.options().autoSaveKifu && !m_session.options().kifuSaveDir.isEmpty()) {
        qCInfo(lcGame) << "auto save: dir=" << m_session.options().kifuSaveDir;
        emit autoSaveRequested(m_session.options().kifuSaveDir);
    }
    emit gameEndProcessed(info);
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

    const auto update = m_gc->positionUpdate();
    {
        const QSignalBlocker clockNotifications(m_clock);
        const int targetPly = currentPly() - 2;

        // 盤面と手番を戻す。「同」表記の基準は、戻した局面に至った手の移動先
        m_gc->board()->setSfen(m_record.sfenAt(targetPly));
        m_gc->setCurrentPlayer(m_gc->board()->currentPlayer());
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
    m_gc->setCurrentPlayer(state->clock.currentPlayer);
    m_gc->setPreviousMoveDestination(state->previousTo);
    // 時計のスナップショットは持ち時間の決まりと時間切れの扱いも含む
    m_clock->restoreSnapshot(state->clock);

    createStrategy(state->options);
    if (m_strategy) m_strategy->armTurnTimerIfNeeded();
    // 対局開始通知でUIを整えてから、呼び出し側が初手探索を起動する。
    return true;
}
