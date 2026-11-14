/// @file csagamecoordinator_moves.cpp
/// @brief CSA通信対局コーディネータの指し手進行（相手の手・自分の手の確認・エンジン思考・裁定待ち）

#include "csagamecoordinator.h"

#include "csamoveconverter.h"
#include "csaverdictwait.h"
#include "logcategories.h"
#include "matchengineseat.h"
#include "matchrecordsink.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "gameboard.h"
#include "usi.h"
#include <QSignalBlocker>

// ============================================================
// 裁定待ち（解釈できない指し手行）
// ============================================================

void CsaGameCoordinator::deferVerdict(const QString& logText, const QString& failureText)
{
    // 仕様 3.4: 不正な着手・不正な書式の指し手も指し手行として届き、続けて #ILLEGAL_MOVE と
    // 勝敗が送られる。ここで終局させず、サーバーの裁定を待つ
    emit logMessage(logText, true);
    emit logMessage(tr("サーバーの裁定を待ちます"), false);
    m_verdict->begin(failureText);
}

bool CsaGameCoordinator::failIfVerdictPending()
{
    if (!m_verdict->isPending()) return false;
    failGame(m_verdict->take());
    return true;
}

void CsaGameCoordinator::onVerdictTimedOut(const QString& failureText)
{
    if (m_phase != Phase::Playing) return;
    emit logMessage(tr("サーバーの裁定が届きませんでした"), true);
    failGame(failureText);
}

// ============================================================
// 時計
// ============================================================

void CsaGameCoordinator::startDisplayClock(ShogiClock* clock, const CsaGameSummary& summary,
                                           qint64 blackRemainingMs, qint64 whiteRemainingMs)
{
    if (!clock) return;
    // 仕様 3.2.2: Time 階層（または全時間要素）の省略は時間無制限。決まりが空なら消費時間だけ数える
    clock->setTimeControl(summary.timeControl());
    if (!summary.isTimeUnlimited()) clock->setRemainingTimes(blackRemainingMs, whiteRemainingMs);
}

void CsaGameCoordinator::syncClockAfterMove(bool startMyTurnClock)
{
    if (!m_clock) return;

    // 表示用の時計をサーバーが示した残り時間に合わせる
    if (!m_progress.summary.isTimeUnlimited()) {
        m_clock->setRemainingTimes(m_progress.blackRemainingMs, m_progress.whiteRemainingMs);
    }

    const Turn mySide = m_progress.isBlackSide ? Turn::Black : Turn::White;
    const Turn opponent = (mySide == Turn::Black) ? Turn::White : Turn::Black;
    m_clock->setCurrentPlayer(startMyTurnClock ? mySide : opponent);
    m_clock->startClock();
}

// ============================================================
// 着手の記録
// ============================================================


std::optional<RecordEntry> CsaGameCoordinator::playCsaMove(const QString& csaMove)
{
    ShogiBoard* board = m_gameBoard ? m_gameBoard->board() : nullptr;
    if (!board) return std::nullopt;
    const std::optional<UsiMove> move = CsaMoveConverter::toUsi(csaMove, board->position());
    if (!move) return std::nullopt;
    return m_gameBoard->applyRecordedMove(*move, m_progress.previousTo());
}

void CsaGameCoordinator::finishMove(const RecordEntry& played, int consumedTimeMs, bool nowMyTurn)
{
    CsaGameProgress& p = m_progress;
    const bool isBlackMove = played.mover == Turn::Black;
    p.recordMoveTime(isBlackMove, consumedTimeMs);
    // 次の手番の考慮時間を数え始める
    m_turnTimer.start();
    {
        // 時計の値は、一手を確定し終えてからまとめて知らせる（ローカル対局と同じ）
        const QSignalBlocker clockNotifications(m_clock);
        syncClockAfterMove(nowMyTurn);
    }

    RecordEntry entry = played;
    entry.time = p.moveTime(isBlackMove, consumedTimeMs);
    p.record.entries.append(entry);
    if (m_sink) m_sink->appendRecord(entry);
    // 時計の表示は時計を見る部品（TimeDisplayPresenter）が合わせる。ここで知らせないと、サーバーが示した残り時間が
    // 動いている側の秒の変わり目（最大1秒後）まで表示に出ない
    if (m_clock) m_clock->notifyTimeUpdated();
    // 盤面・棋譜・時計を確定し終えた着手を知らせる（駒音。通知は盤面の更新の後）。人間が盤で指した自分の手は、
    // 送ったとき（sendHumanMove）に知らせている
    const bool sentByHuman = !nowMyTurn && isHumanPlayer();
    if (!sentByHuman) m_gameBoard->notifyPiecePlaced(isBlackMove ? Turn::Black : Turn::White, p.moveCount());
}

// ============================================================
// 相手の指し手受信
// ============================================================

bool CsaGameCoordinator::applyConfirmedMove(const QString& move, int consumedTimeMs, bool nowMyTurn)
{
    if (m_phase != Phase::Playing || !m_gameBoard || failIfVerdictPending()) return false;
    // 手数は対局条件の上限（超えるとサーバーが #MAX_MOVES で終える）と、絶対の上限で打ち切る（セキュリティ監査 V3）
    const int maxMoves = m_progress.summary.maxMoves;
    const int limit = maxMoves > 0 ? qMin(maxMoves + 1, CsaGameSummary::kMaxGameMoves) : CsaGameSummary::kMaxGameMoves;
    if (m_progress.moveCount() >= limit) {
        failGame(tr("手数の上限（%1手）を超えたので対局を中断しました").arg(limit));
        return false;
    }
    const std::optional<RecordEntry> played = playCsaMove(move);
    if (!played) {
        // 書式の誤りも局面に合わない手も、CsaMoveConverter::toUsi が1か所で断る
        deferVerdict(tr("指し手の適用に失敗しました: %1").arg(move),
                     tr("サーバーからの指し手を盤面に適用できません: %1").arg(move));
        return false;
    }
    finishMove(*played, consumedTimeMs, nowMyTurn);
    return true;
}

void CsaGameCoordinator::onMoveReceived(const QString& move, int consumedTimeMs)
{
    qCDebug(lcNetwork).noquote() << "opponent move:" << move << "consumed" << consumedTimeMs << "ms";
    if (applyConfirmedMove(move, consumedTimeMs, true) && !isHumanPlayer())
        startEngineThinking();
}

void CsaGameCoordinator::onMoveConfirmed(const QString& move, int consumedTimeMs)
{
    qCDebug(lcNetwork).noquote() << "move confirmed:" << move << "consumed" << consumedTimeMs << "ms";
    applyConfirmedMove(move, consumedTimeMs, false);
}

// ============================================================
// エンジン思考
// ============================================================

void CsaGameCoordinator::startEngineThinking()
{
    if (!m_engineSeat || !m_engineSeat->isReady() || isHumanPlayer()) {
        return;
    }

    const std::optional<UsiPosition> position = m_progress.usiPosition();
    if (!position) {
        emit logMessage(tr("エンジンに送る局面を作れません"), true);
        return;
    }
    qCDebug(lcNetwork) << "engine starts thinking";

    // 持ち時間はこの司令塔（MatchTimeSource）がサーバーの残り時間から返す
    m_engineSeat->engine()->requestMatchMove(*position);
}

void CsaGameCoordinator::onEngineMoveReady(const UsiMatchMove& result)
{
    if (!isMyTurn()) return;

    if (!result.move.isValid()) {
        emit logMessage(tr("エンジンが有効な指し手を返しませんでした"), true);
        return;
    }

    ShogiBoard* board = m_gameBoard ? m_gameBoard->board() : nullptr;
    if (!board) {
        emit logMessage(tr("盤面が取得できませんでした"), true);
        return;
    }

    // CSA形式の指し手は、盤面を更新する前の駒から作る
    const bool black = m_progress.isBlackSide;
    const QString csaMove = CsaMoveConverter::toCsa(result.move, black ? Turn::Black : Turn::White, board->position());
    if (csaMove.isEmpty()) {
        emit logMessage(tr("指し手の駒種変換に失敗しました"), true);
        return;
    }

    qCDebug(lcNetwork).noquote() << "engine move in CSA format:" << csaMove;

    if (m_client->sendMove(csaMove)) {
        emit engineMovePlayed(black ? Turn::Black : Turn::White, m_progress.moveCount() + 1, result.score);
    }
}
