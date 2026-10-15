/// @file csagamecoordinator_moves.cpp
/// @brief CSA通信対局コーディネータの指し手進行（相手の手・自分の手の確認・エンジン思考・裁定待ち）

#include "csagamecoordinator.h"

#include "csamoveconverter.h"
#include "logcategories.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
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
    m_pendingFailureText = failureText;
    m_verdictTimer->start();
}

bool CsaGameCoordinator::failIfVerdictPending()
{
    if (m_pendingFailureText.isEmpty()) return false;
    const QString text = m_pendingFailureText;
    clearPendingVerdict();
    failGame(text);
    return true;
}

void CsaGameCoordinator::clearPendingVerdict()
{
    m_verdictTimer->stop();
    m_pendingFailureText.clear();
}

void CsaGameCoordinator::onVerdictTimeout()
{
    if (m_pendingFailureText.isEmpty()) return;
    if (m_gameState != GameState::InGame) {
        clearPendingVerdict();
        return;
    }
    const QString text = m_pendingFailureText;
    clearPendingVerdict();
    emit logMessage(tr("サーバーの裁定が届きませんでした"), true);
    failGame(text);
}

QString CsaGameCoordinator::validateMoveText(const QString& move, QPoint* from, QPoint* to) const
{
    if (move.length() < 7) {
        qCWarning(lcNetwork) << "Invalid CSA move length:" << move;
        return tr("不正な形式の指し手を受信しました: %1").arg(move);
    }

    const int fromFile = move[1].digitValue();
    const int fromRank = move[2].digitValue();
    const int toFile = move[3].digitValue();
    const int toRank = move[4].digitValue();

    const bool invalidTo = (toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9);
    const bool invalidFrom = !((fromFile == 0 && fromRank == 0) ||
                               (fromFile >= 1 && fromFile <= 9 &&
                                fromRank >= 1 && fromRank <= 9));
    if (invalidTo || invalidFrom) {
        qCWarning(lcNetwork) << "Invalid CSA move coordinates:"
                             << "fromFile=" << fromFile << "fromRank=" << fromRank
                             << "toFile=" << toFile << "toRank=" << toRank << "move=" << move;
        return tr("不正な座標の指し手を受信しました: %1").arg(move);
    }

    if (from) *from = (fromFile == 0 && fromRank == 0) ? QPoint(-1, -1) : QPoint(fromFile, fromRank);
    if (to) *to = QPoint(toFile, toRank);
    return QString();
}

// ============================================================
// 時計
// ============================================================

void CsaGameCoordinator::startDisplayClock(ShogiClock* clock, const CsaClient::GameSummary& summary,
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


void CsaGameCoordinator::finishMove(const QString& csaMove, const RecordEntry& played, bool isPromotion,
                                    int consumedTimeMs, const QPoint& from, const QPoint& to, bool nowMyTurn)
{
    CsaGameProgress& p = m_progress;
    const bool isBlackMove = (csaMove[0] == QLatin1Char('+'));
    p.recordMoveTime(isBlackMove, consumedTimeMs);
    syncClockAfterMove(nowMyTurn);


    const QString prettyMove = CsaMoveConverter::csaToPretty(csaMove, isPromotion, p.previousTo());
    RecordEntry entry = p.moveRecord(isBlackMove, prettyMove, consumedTimeMs);
    entry.move = played.move;
    entry.sfen = played.sfen;
    p.record.entries.append(entry);
    emit recordEntryReady(entry);
    emit turnChanged(nowMyTurn);
    emit moveHighlightRequested(from, to);
    // 盤面・棋譜・時計を確定し終えた着手を知らせる（駒音・定跡ウィンドウ。通知は盤面の更新の後）
    m_gameController->notifyMoveCommitted(isBlackMove ? Turn::Black : Turn::White, p.moveCount());
}

// ============================================================
// 相手の指し手受信
// ============================================================

bool CsaGameCoordinator::applyConfirmedMove(const QString& move, int consumedTimeMs, bool nowMyTurn)
{
    if (m_gameState != GameState::InGame || !m_gameController || failIfVerdictPending()) return false;
    QPoint from, to;
    const QString problem = validateMoveText(move, &from, &to);
    if (!problem.isEmpty()) {
        deferVerdict(problem, tr("サーバーからの指し手を解釈できません: %1").arg(move));
        return false;
    }
    // 盤面・手番の通知は、棋譜と時計まで確定した後にまとめて送る。
    const QSignalBlocker clockNotifications(m_clock);
    const auto update = m_gameController->positionUpdate();
    const bool isPromotion = CsaMoveConverter::isPromotionMove(move, m_gameController->board());
    const std::optional<RecordEntry> played = CsaMoveConverter::applyMoveToBoard(move, m_gameController);
    if (!played) {
        deferVerdict(tr("指し手の適用に失敗しました: %1").arg(move),
                     tr("サーバーからの指し手を盤面に適用できません: %1").arg(move));
        return false;
    }
    finishMove(move, *played, isPromotion, consumedTimeMs, from, to, nowMyTurn);
    return true;
}

void CsaGameCoordinator::onMoveReceived(const QString& move, int consumedTimeMs)
{
    emit logMessage(tr("相手の指し手: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs), false);
    if (applyConfirmedMove(move, consumedTimeMs, true) && m_playerType == PlayerType::Engine)
        startEngineThinking();
}

void CsaGameCoordinator::onMoveConfirmed(const QString& move, int consumedTimeMs)
{
    emit logMessage(tr("指し手確認: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs), false);
    applyConfirmedMove(move, consumedTimeMs, false);
}

// ============================================================
// エンジン思考
// ============================================================

void CsaGameCoordinator::startEngineThinking()
{
    if (!m_engineController || !m_engineController->isInitialized() || m_playerType != PlayerType::Engine) {
        return;
    }

    CsaEngineController::ThinkingParams params;
    params.positionCmd = m_progress.positionCommand();
    params.timing = m_progress.engineTiming(m_thinkingMarginMs);

    emit logMessage(tr("エンジンが思考中..."), false);

    connect(m_engineController, &CsaEngineController::thinkingFinished,
            this, &CsaGameCoordinator::onEngineThinkingFinished, Qt::UniqueConnection);
    m_engineController->thinkAsync(params);
}

void CsaGameCoordinator::onEngineThinkingFinished(const CsaEngineController::ThinkingResult& result)
{
    if (sender() != m_engineController || !isMyTurn()) return;
    if (m_gameState != GameState::InGame) return;

    if (result.resign) {
        emit logMessage(tr("エンジンが投了しました"), false);
        performResign();
        return;
    }

    if (!result.valid) {
        emit logMessage(tr("エンジンが有効な指し手を返しませんでした"), true);
        return;
    }

    ShogiBoard* board = m_gameController ? m_gameController->board() : nullptr;
    if (!board) {
        emit logMessage(tr("盤面が取得できませんでした"), true);
        return;
    }

    // CSA形式の指し手は、盤面を更新する前の駒から作る
    const bool black = m_progress.isBlackSide;
    const QString csaMove = CsaMoveConverter::usiToCsa(result.move.toString(), black, board);
    if (csaMove.isEmpty()) {
        emit logMessage(tr("指し手の駒種変換に失敗しました"), true);
        return;
    }

    emit logMessage(tr("CSA形式の指し手: %1").arg(csaMove), false);

    if (m_client->sendMove(csaMove)) {
        emit turnChanged(false);
        emit engineScoreUpdated(result.scoreCp, m_progress.moveCount() + 1, result.scoreMate);
    }
}
