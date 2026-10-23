/// @file matchcoordinator.cpp
/// @brief ローカル対局の司令塔: 開始・一手の確定・エンジンへの思考の依頼・時計の通知

#include "matchcoordinator.h"

#include "kifudocument.h"

#include <QSignalBlocker>

#include "appactivitystate.h"

#include "enginelifecyclemanager.h"
#include "enginevsenginestrategy.h"
#include "humanvsenginestrategy.h"
#include "humanvshumanstrategy.h"
#include "logcategories.h"
#include "matchengineseat.h"
#include "matchrecordsink.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "usi.h"
#include "usimatchhandler.h"
#include "nyugyokujudgement.h"
#include "sennichitedetector.h"
#include "josekimovepicker.h"
#include "kifupresentation.h"

#include <QRandomGenerator>

namespace {
/// 定跡手を指すまで待つ時間（定跡手でもエンジンが指したと分かるように、最低これだけ置く）
constexpr int kBookMoveDelayMs = 1000;
} // namespace

// ============================================================
// 初期化
// ============================================================

MatchCoordinator::MatchCoordinator(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_gc(deps.gc)
    , m_clock(deps.clock)
    , m_activity(deps.activity)
    , m_engines(deps.engines)
    , m_clockTime(deps.clock)
    , m_document(deps.document)
{
    Q_ASSERT(m_gc && m_clock && m_activity && m_engines);
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

    clearGameEnd();
    m_session.begin(resolved);
    m_record = resolved.recordLine;
    m_activityLease = m_activity->begin(AppActivityState::Activity::DuringGame, this);

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

void MatchCoordinator::createStrategy(const StartOptions& opt)
{
    m_strategy.reset();
    const MatchPlayers& players = opt.players;
    if (players.engineVsEngine()) {
        m_strategy = std::make_unique<EngineVsEngineStrategy>(
            strategyHost(), EngineVsEngineStrategy::Engines{opt.engine(Turn::Black).path, players.engineName(Turn::Black),
                                                            opt.engine(Turn::White).path, players.engineName(Turn::White)});
    } else if (players.hasEngine()) {
        const Turn side = players.isEngine(Turn::Black) ? Turn::Black : Turn::White;
        m_strategy = std::make_unique<HumanVsEngineStrategy>(strategyHost(), side, opt.engine(side).path,
                                                             players.engineName(side));
    } else {
        m_strategy = std::make_unique<HumanVsHumanStrategy>(strategyHost());
    }
    // エンジンの投了・宣言勝ちと、起動の失敗・異常終了を受ける（起動の失敗は start から戻った後に届くので、
    // そのまま中断してよい）
    for (const Turn side : {Turn::Black, Turn::White}) {
        MatchEngineSeat* seat = m_strategy->engineSeat(side);
        if (!seat) continue;
        connect(seat, &MatchEngineSeat::resigned, this, &MatchCoordinator::handleEngineResign);
        connect(seat, &MatchEngineSeat::declaredWin, this, &MatchCoordinator::handleEngineWin);
        connect(seat, &MatchEngineSeat::errorOccurred, this, &MatchCoordinator::handleEngineError);
    }
    m_strategy->start();
}

Usi* MatchCoordinator::engineFor(Turn side) const
{
    const MatchEngineSeat* seat = m_strategy ? m_strategy->engineSeat(side) : nullptr;
    return seat ? seat->engine() : nullptr;
}

bool MatchCoordinator::usesHandicapNames() const
{
    return m_document && m_document->usesHandicapNames();
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
    const std::optional<RecordEntry> played = m_gc->applyMove(move.move, lastMoveDestination());
    if (!played) {
        handleEngineIllegalMove(move.side);
        return false;
    }
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
    // 時計の表示は時計を見る部品（TimeDisplayPresenter）が合わせる
    m_clock->notifyTimeUpdated();
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
    Usi* engine = engineFor(side);
    if (!engine) return;
    const std::optional<UsiPosition> position = m_record.usiPosition(currentPly());
    if (!position) {
        // 対局の手順は指せた手だけなので起きないはず。エンジンに前の局面を探索させない
        handleEngineError(tr("エンジンに送る局面を作れません"));
        return;
    }
    // 定跡を使う対局者なら、条件に合う定跡手をエンジンに探索させずに指す
    const StartOptions& opt = m_session.options();
    const JosekiPlayOptions& joseki = opt.engine(side).joseki;
    if (joseki.enabled) {
        const QString sfen = m_record.lastSfen();
        if (const std::optional<JosekiPick> pick =
                JosekiMovePicker::pick(opt.josekiBook, sfen, joseki, *QRandomGenerator::global())) {
            if (const std::optional<UsiMove> move = UsiMove::parse(pick->move.move)) {
                engine->requestBookMove(*position, *move, kBookMoveDelayMs, bookMoveLabel(sfen, *pick));
                return;
            }
        }
    }
    engine->requestMatchMove(*position);
}

QString MatchCoordinator::bookMoveLabel(const QString& sfen, const JosekiPick& pick)
{
    const QString move = KifuPresentation::move(sfen, pick.move.move, KifuPresentation::options());
    // 出現頻度の無い定跡（すべて0）は割合を出さない
    if (pick.ratioPercent <= 0.0) return tr("%1（定跡）").arg(move);
    return tr("%1（定跡 %2%）").arg(move, QString::number(pick.ratioPercent, 'f', 1));
}

void MatchCoordinator::forceImmediateMove()
{
    if (isGameOver()) return;
    // 人間の手番に「すぐ指させる」を押しても先読みは止めない
    if (Usi* engine = engineFor(m_gc->currentPlayer())) engine->requestImmediateMove();
}

// ============================================================
// エンジン・時計
// ============================================================

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
        NyugyokuJudgement::judgeEngineDeclaration(m_gc->board()->position(), side == Turn::Black, usesHandicapNames());
    handleNyugyokuDeclaration(side, result.success, result.isDraw);
    // 人間の宣言は宣言の結果ダイアログで知らせる。エンジンの宣言はここで判定結果を知らせる
    if (isGameOver()) emit gameOverMessage(NyugyokuJudgement::resultTitle(), result.message);
}

NyugyokuJudgement::Result MatchCoordinator::declareNyugyoku(Turn declarer, JishogiCalculator::Rule rule)
{
    const NyugyokuJudgement::Result result =
        NyugyokuJudgement::judge(m_gc->board()->position(), declarer == Turn::Black, rule, usesHandicapNames());
    handleNyugyokuDeclaration(declarer, result.success, result.isDraw);
    return result;
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
    // 中断の後に届く bestmove resign は、終局の知らせ（MatchEngineSeat::finish）の後なので受けない。
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
    for (const Turn side : {Turn::Black, Turn::White}) {
        if (Usi* engine = engineFor(side)) engine->cancelCurrentOperation();
    }
    if (!players().hasEngine()) return;
    handleBreakOff();
    emit gameOverMessage(tr("対局中断"), tr("エンジンエラー: %1").arg(message));
}

// ============================================================
// 終局の記録
// ============================================================

void MatchCoordinator::clearGameEnd()
{
    m_session.clearEnd();
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
    // 対局のエンジンに終局を知らせて終える（負けた側・勝った側・引き分け）
    for (const Turn side : {Turn::Black, Turn::White}) {
        MatchEngineSeat* seat = m_strategy ? m_strategy->engineSeat(side) : nullptr;
        if (!seat) continue;
        seat->finish(!engineLoser ? GameOverResult::Draw
                                  : (*engineLoser == side ? GameOverResult::Lose : GameOverResult::Win));
    }
    appendTerminalRecord(info);
    m_activityLease.reset();
    // 終局の行を棋譜に書いた後で、時刻と UI を更新させる
    emit gameEnded(info);

    switch (notice) {
    case Notice::Announce:
        emit gameOverMessage(tr("対局終了"), resultMessage(info));
        [[fallthrough]];
    case Notice::Declaration:
        // 入玉宣言は宣言の結果ダイアログ（人間）か handleEngineWin（エンジン）で知らせる。
        // 棋譜の自動保存は終局の受け手（gameEnded）が行う
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
    case Cause::OuteSennichite: terminal = RecordEntry::perpetualCheckTerminal(info.loser, sideToMove); break;
    case Cause::BreakOff: terminal = TerminalType::Interrupt; break;
    case Cause::Resignation: terminal = TerminalType::Resign; break;
    case Cause::Timeout: terminal = TerminalType::Timeout; break;
    }

    // 終局行の印と消費時間は、その行（投了・宣言など）を行った側のもの
    const Turn mover = RecordEntry::endingSide(terminal, sideToMove, info.loser);
    m_clock->commitMove(mover);
    const RecordEntry entry = RecordEntry::ending(terminal, mover, m_clock->recordTime(mover));
    if (m_sink && m_sink->canAppendRecord()) m_sink->appendRecord(entry);
}

QString MatchCoordinator::resultMessage(const GameEndInfo& info) const
{
    // 駒落ちは棋譜の局面図・終局行と同じく下手・上手と呼ぶ
    const QString blackJP = usesHandicapNames() ? tr("下手") : tr("先手");
    const QString whiteJP = usesHandicapNames() ? tr("上手") : tr("後手");
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

        // 盤面と手番を戻す（「同」表記の基準は手順の最後の手から導く）
        m_gc->board()->setSfen(m_record.sfenAt(targetPly));
        m_record.entries.resize(targetPly);

        // 時計を取り消した自分の手を指し始めたときへ戻し、考慮時間は今から測り直す
        m_clock->undo();
        if (m_sink) m_sink->undoRecords(2);
        // 戻す前の局面で始めた先読みは当たらない
        for (const Turn side : {Turn::Black, Turn::White})
            if (Usi* engine = engineFor(side)) engine->forgetPonder();
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
    state->record = m_record;
    m_interruptedGame = std::move(state);
}

void MatchCoordinator::attachInterruptedRecord(int terminalNodeId)
{
    if (!m_interruptedGame) return;
    m_interruptedGame->terminalNodeId = terminalNodeId;
    emit resumeAvailabilityChanged();
}

void MatchCoordinator::discardInterruptedGame()
{
    if (!m_interruptedGame) return;
    const bool couldResume = canResumeInterruptedGame();
    m_interruptedGame.reset();
    if (couldResume) emit resumeAvailabilityChanged();
}

bool MatchCoordinator::resumeInterruptedGame()
{
    if (!m_interruptedGame || !m_gc->board()) return false;
    const auto state = std::move(m_interruptedGame);
    emit resumeAvailabilityChanged();

    clearGameEnd();
    m_session.begin(state->options);
    m_activityLease = m_activity->begin(AppActivityState::Activity::DuringGame, this);
    m_record = state->record;
    m_gc->board()->setSfen(m_record.lastSfen());

    // 時計のスナップショットは持ち時間の決まりと時間切れの扱いも含む
    m_clock->restoreSnapshot(state->clock);

    createStrategy(state->options);
    // 対局開始通知でUIを整えてから、呼び出し側が初手探索を起動する。
    return true;
}
