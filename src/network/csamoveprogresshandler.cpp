/// @file csamoveprogresshandler.cpp
/// @brief CSA対局の指し手進行ハンドラの実装

#include "csamoveprogresshandler.h"
#include "csamoveconverter.h"
#include "csaenginecontroller.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "shogiclock.h"
#include "shogiboard.h"
#include "logcategories.h"

CsaMoveProgressHandler::CsaMoveProgressHandler()
{
    m_verdictTimer.setSingleShot(true);
    m_verdictTimer.setInterval(kVerdictTimeoutMs);
    connect(&m_verdictTimer, &QTimer::timeout, this, &CsaMoveProgressHandler::onVerdictTimeout);
}

void CsaMoveProgressHandler::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void CsaMoveProgressHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

// ============================================================
// 裁定待ち（解釈できない指し手行）
// ============================================================

void CsaMoveProgressHandler::deferVerdict(const QString& logText, const QString& failureText)
{
    // 仕様 3.4: 不正な着手・不正な書式の指し手も指し手行として届き、続けて #ILLEGAL_MOVE と
    // 勝敗が送られる。ここで終局させず、サーバーの裁定を待つ
    m_hooks.logMessage(logText, true);
    m_hooks.logMessage(tr("サーバーの裁定を待ちます"), false);
    m_pendingFailureText = failureText;
    m_verdictTimer.start();
}

bool CsaMoveProgressHandler::failIfVerdictPending()
{
    if (m_pendingFailureText.isEmpty()) return false;
    const QString text = m_pendingFailureText;
    clearPendingVerdict();
    m_hooks.failGame(text);
    return true;
}

void CsaMoveProgressHandler::clearPendingVerdict()
{
    m_verdictTimer.stop();
    m_pendingFailureText.clear();
}

void CsaMoveProgressHandler::onVerdictTimeout()
{
    if (m_pendingFailureText.isEmpty()) return;
    if (!m_refs.gameState || *m_refs.gameState != GameState::InGame) {
        clearPendingVerdict();
        return;
    }
    const QString text = m_pendingFailureText;
    clearPendingVerdict();
    m_hooks.logMessage(tr("サーバーの裁定が届きませんでした"), true);
    m_hooks.failGame(text);
}

QString CsaMoveProgressHandler::validateMoveText(const QString& move, QPoint* from, QPoint* to) const
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
                             << "fromFile=" << fromFile
                             << "fromRank=" << fromRank
                             << "toFile=" << toFile
                             << "toRank=" << toRank
                             << "move=" << move;
        return tr("不正な座標の指し手を受信しました: %1").arg(move);
    }

    if (from) *from = (fromFile == 0 && fromRank == 0) ? QPoint(-1, -1) : QPoint(fromFile, fromRank);
    if (to) *to = QPoint(toFile, toRank);
    return QString();
}

// ============================================================
// 時間管理ヘルパー
// ============================================================

void CsaMoveProgressHandler::startDisplayClock(ShogiClock* clock, const CsaClient::GameSummary& summary,
                                               qint64 blackRemainingMs, qint64 whiteRemainingMs)
{
    if (!clock) return;
    // 仕様 3.2.2: Time 階層（または全時間要素）の省略は時間無制限。決まりが空なら消費時間だけ数える
    clock->setTimeControl(summary.timeControl());
    if (!summary.isTimeUnlimited()) clock->setRemainingTimes(blackRemainingMs, whiteRemainingMs);
}

void CsaMoveProgressHandler::updateTimeTracking(bool isBlackMove, int consumedTimeMs)
{
    if (isBlackMove) {
        *m_refs.blackTotalTimeMs += consumedTimeMs;
        *m_refs.blackRemainingMs -= consumedTimeMs;
        if (*m_refs.blackRemainingMs < 0) *m_refs.blackRemainingMs = 0;
    } else {
        *m_refs.whiteTotalTimeMs += consumedTimeMs;
        *m_refs.whiteRemainingMs -= consumedTimeMs;
        if (*m_refs.whiteRemainingMs < 0) *m_refs.whiteRemainingMs = 0;
    }
    // CSAの加算は次の手番の開始時に、その手番側の加算時間で行う。
    if (isBlackMove) *m_refs.whiteRemainingMs += m_refs.gameSummary->incrementMs(false);
    else *m_refs.blackRemainingMs += m_refs.gameSummary->incrementMs(true);
}

void CsaMoveProgressHandler::syncClockAfterMove(bool startMyTurnClock)
{
    if (!*m_refs.clock) return;

    // 表示用の時計をサーバーが示した残り時間に合わせる
    if (!m_refs.gameSummary->isTimeUnlimited()) {
        (*m_refs.clock)->setRemainingTimes(*m_refs.blackRemainingMs, *m_refs.whiteRemainingMs);
    }

    const Turn mySide = *m_refs.isBlackSide ? Turn::Black : Turn::White;
    (*m_refs.clock)->setCurrentPlayer(startMyTurnClock ? mySide : (mySide == Turn::Black ? Turn::White : Turn::Black));
    (*m_refs.clock)->startClock();
}

// ============================================================
// 相手の指し手受信
// ============================================================

void CsaMoveProgressHandler::handleMoveReceived(const QString& move, int consumedTimeMs)
{
    m_hooks.logMessage(tr("相手の指し手: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs), false);

    // 裁定待ちのまま次の指し手が来た（サーバーは対局を続けている）なら、盤面が合わないので続行できない
    if (failIfVerdictPending()) return;

    QPoint from, to;
    const QString problem = validateMoveText(move, &from, &to);
    if (!problem.isEmpty()) {
        deferVerdict(problem, tr("サーバーからの指し手を解釈できません: %1").arg(move));
        return;
    }

    const bool isBlackMove = (move[0] == QLatin1Char('+'));
    const int toFile = move[3].digitValue();
    const int toRank = move[4].digitValue();

    // 成り判定（盤面更新前に行う）
    const bool isPromotion = *m_refs.gameController
        && CsaMoveConverter::isPromotionMove(move, (*m_refs.gameController)->board());

    if (!CsaMoveConverter::applyMoveToBoard(move, *m_refs.gameController, *m_refs.usiMoves, *m_refs.sfenHistory, *m_refs.moveCount)) {
        deferVerdict(tr("指し手の適用に失敗しました: %1").arg(move),
                     tr("サーバーからの指し手を盤面に適用できません: %1").arg(move));
        return;
    }

    updateTimeTracking(isBlackMove, consumedTimeMs);
    syncClockAfterMove(true);  // 次は自分の手番

    // 盤・駒台の変化は ShogiBoard の通知で変わった所だけ描き直されるので、盤全体は描き直さない。
    if (m_hooks.moveAppliedToBoard) m_hooks.moveAppliedToBoard();

    (*m_refs.moveCount)++;
    *m_refs.isMyTurn = true;
    m_hooks.turnChanged(true);

    m_hooks.moveHighlightRequested(from, to);

    QString usiMove = CsaMoveConverter::csaToUsi(move);
    if (!isPromotion && usiMove.endsWith(QLatin1Char('+'))) usiMove.chop(1);
    QString prettyMove = CsaMoveConverter::csaToPretty(move, isPromotion, *m_refs.prevToFile, *m_refs.prevToRank, *m_refs.moveCount - 1);

    *m_refs.prevToFile = toFile;
    *m_refs.prevToRank = toRank;

    m_hooks.moveMade(move, usiMove, prettyMove, consumedTimeMs);

    if (*m_refs.playerType == PlayerType::Engine) {
        startEngineThinking();
    }
}

// ============================================================
// 自分の指し手確認
// ============================================================

void CsaMoveProgressHandler::handleMoveConfirmed(const QString& move, int consumedTimeMs)
{
    m_hooks.logMessage(tr("指し手確認: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs), false);

    if (failIfVerdictPending()) return;

    QPoint from, to;
    const QString problem = validateMoveText(move, &from, &to);
    if (!problem.isEmpty()) {
        // 自分の送った手が不正な書式として切り出されて返った。続く #ILLEGAL_MOVE を待つ
        deferVerdict(problem, tr("サーバーからの指し手確認を解釈できません: %1").arg(move));
        return;
    }

    const bool isBlackMove = (move[0] == QLatin1Char('+'));
    const int toFile = move[3].digitValue();
    const int toRank = move[4].digitValue();

    // USI指し手リストとSFEN記録は更新する必要がある
    // CSA の駒種だけでは成った手か成駒を動かした手か分からないので、送った手の成り・不成に合わせる
    QString usiMove = CsaMoveConverter::csaToUsi(move);
    if (!m_sentMove.promote && usiMove.endsWith(QLatin1Char('+'))) usiMove.chop(1);
    if (usiMove.isEmpty()) {
        qCWarning(lcNetwork) << "Invalid confirmed CSA move payload:" << move;
        deferVerdict(tr("指し手確認の変換に失敗しました: %1").arg(move),
                     tr("サーバーからの指し手確認を変換できません: %1").arg(move));
        return;
    }
    m_refs.usiMoves->append(usiMove);

    // エンジンの指し手の場合のみSFEN記録を更新（人間はvalidateAndMoveで追加済み）
    if (*m_refs.playerType == PlayerType::Engine) {
        if (*m_refs.gameController && (*m_refs.gameController)->board() && *m_refs.sfenHistory) {
            QString boardSfen = (*m_refs.gameController)->board()->convertBoardToSfen();
            QString standSfen = (*m_refs.gameController)->board()->convertStandToSfen();
            QString currentPlayerStr = ((*m_refs.gameController)->currentPlayer() == Turn::Black)
                                       ? QStringLiteral("b") : QStringLiteral("w");
            QString fullSfen = QString("%1 %2 %3 %4")
                                   .arg(boardSfen, currentPlayerStr, standSfen)
                                   .arg(*m_refs.moveCount + 2);
            (*m_refs.sfenHistory)->append(fullSfen);
        }
    }

    updateTimeTracking(isBlackMove, consumedTimeMs);
    syncClockAfterMove(false);  // 次は相手の手番

    (*m_refs.moveCount)++;
    *m_refs.isMyTurn = false;
    m_hooks.turnChanged(false);

    m_hooks.moveHighlightRequested(from, to);

    const bool isPromotion = m_sentMove.promote;
    QString prettyMove = CsaMoveConverter::csaToPretty(move, isPromotion, *m_refs.prevToFile, *m_refs.prevToRank, *m_refs.moveCount - 1);

    *m_refs.prevToFile = toFile;
    *m_refs.prevToRank = toRank;

    m_hooks.moveMade(move, usiMove, prettyMove, consumedTimeMs);
}

// ============================================================
// エンジン思考
// ============================================================

void CsaMoveProgressHandler::startEngineThinking()
{
    if (!*m_refs.engineController || !(*m_refs.engineController)->isInitialized()
        || *m_refs.playerType != PlayerType::Engine) {
        return;
    }

    // ポジションコマンドを構築
    QString positionCmd = *m_refs.positionStr;
    if (!m_refs.usiMoves->isEmpty()) {
        positionCmd += QStringLiteral(" moves ") + m_refs.usiMoves->join(QLatin1Char(' '));
    }

    // 時間の条件。CSA の残り時間は手番の始めに加算するので、手番側は今の手番の加算を含み、
    // 相手側は次の手番の加算を含まない。USI の btime/wtime の意味（その手番の加算を含まない）にそろえる。
    // 仕様 3.2.2.1: 1手に使える時間は「遅延時間＋残り時間＋秒読み」。遅延時間はサーバーが消費時間から
    // 差し引くので、今の手の思考時間に足すだけでよい。サーバーは LF の到着時刻で消費時間を測るので、
    // 通信の遅延に備えて余裕を差し引く。go に載せる値は UsiGoTimeCalculator が求める。
    const CsaClient::GameSummary& s = *m_refs.gameSummary;
    const bool black = *m_refs.isBlackSide;
    const TimeControl tc = s.timeControl().normalized();
    const qint64 myIncrementMs = tc.side(black ? Turn::Black : Turn::White).incrementMs;
    qint64 blackRemainMs = qMax(0, *m_refs.blackRemainingMs);
    qint64 whiteRemainMs = qMax(0, *m_refs.whiteRemainingMs);
    (black ? blackRemainMs : whiteRemainMs) = qMax<qint64>(0, (black ? blackRemainMs : whiteRemainMs) - myIncrementMs);

    CsaEngineController::ThinkingParams params;
    params.positionCmd = positionCmd;
    params.timing.timeControl = tc;
    params.timing.btimeMs = blackRemainMs;
    params.timing.wtimeMs = whiteRemainMs;
    params.timing.extraMs = tc.side(black ? Turn::Black : Turn::White).delayMs;
    params.timing.marginMs = m_refs.thinkingMarginMs ? qMax(0, *m_refs.thinkingMarginMs) : 0;

    m_hooks.logMessage(tr("エンジンが思考中..."), false);

    connect(*m_refs.engineController, &CsaEngineController::thinkingFinished,
            this, &CsaMoveProgressHandler::onEngineThinkingFinished, Qt::UniqueConnection);
    (*m_refs.engineController)->thinkAsync(params);
}

void CsaMoveProgressHandler::onEngineThinkingFinished(const CsaEngineController::ThinkingResult& result)
{
    if (sender() != *m_refs.engineController || !*m_refs.isMyTurn) return;
    if (*m_refs.gameState != GameState::InGame) return;

    // 投了チェック
    if (result.resign) {
        m_hooks.logMessage(tr("エンジンが投了しました"), false);
        m_hooks.performResign();
        return;
    }

    // 有効な指し手かチェック
    if (!result.valid) {
        m_hooks.logMessage(tr("エンジンが有効な指し手を返しませんでした"), true);
        return;
    }

    // CSA形式の指し手を生成（盤面更新前に駒情報を取得）
    ShogiBoard* board = *m_refs.gameController ? (*m_refs.gameController)->board() : nullptr;
    if (!board) {
        m_hooks.logMessage(tr("盤面が取得できませんでした"), true);
        return;
    }

    // CSA形式の指し手は、盤面を更新する前の駒から作る
    const Turn mover = *m_refs.isBlackSide ? Turn::Black : Turn::White;
    const QString csaMove = CsaMoveConverter::usiToCsa(result.move.toString(), *m_refs.isBlackSide, board);
    if (csaMove.isEmpty()) {
        m_hooks.logMessage(tr("指し手の駒種変換に失敗しました"), true);
        return;
    }

    m_hooks.logMessage(tr("CSA形式の指し手: %1").arg(csaMove), false);

    // 盤面を更新
    if (!board->applyUsiMove(result.move, mover)) {
        m_hooks.logMessage(tr("指し手の適用に失敗しました: %1").arg(csaMove), true);
        m_hooks.failGame(tr("エンジンの指し手を盤面に適用できません: %1").arg(csaMove));
        return;
    }

    (*m_refs.gameController)->changeCurrentPlayer();
    if (m_hooks.moveAppliedToBoard) m_hooks.moveAppliedToBoard();

    // 評価値更新
    int ply = *m_refs.moveCount + 1;
    m_hooks.engineScoreUpdated(result.scoreCp, ply, result.scoreMate);

    // サーバーに送信
    noteSentMove(result.move);
    m_refs.client->sendMove(csaMove);
}
