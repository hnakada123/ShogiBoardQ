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

void CsaMoveProgressHandler::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void CsaMoveProgressHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

// ============================================================
// 時間管理ヘルパー
// ============================================================

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
}

void CsaMoveProgressHandler::syncClockAfterMove(bool startMyTurnClock)
{
    if (!*m_refs.clock) return;

    int blackRemainSec = *m_refs.blackRemainingMs / 1000;
    int whiteRemainSec = *m_refs.whiteRemainingMs / 1000;
    int byoyomiSec = m_refs.gameSummary->byoyomi * m_refs.gameSummary->timeUnitMs() / 1000;
    int incrementSec = m_refs.gameSummary->increment * m_refs.gameSummary->timeUnitMs() / 1000;

    (*m_refs.clock)->setPlayerTimes(blackRemainSec, whiteRemainSec,
                                    byoyomiSec, byoyomiSec,
                                    incrementSec, incrementSec,
                                    true);

    if (startMyTurnClock) {
        (*m_refs.clock)->setCurrentPlayer(*m_refs.isBlackSide ? 1 : 2);
    } else {
        (*m_refs.clock)->setCurrentPlayer(*m_refs.isBlackSide ? 2 : 1);
    }
    (*m_refs.clock)->startClock();
}

// ============================================================
// 相手の指し手受信
// ============================================================

void CsaMoveProgressHandler::handleMoveReceived(const QString& move, int consumedTimeMs)
{
    m_hooks.logMessage(tr("相手の指し手: %1 (消費時間: %2ms)").arg(move).arg(consumedTimeMs), false);

    bool isBlackMove = (move.length() > 0 && move[0] == QLatin1Char('+'));

    updateTimeTracking(isBlackMove, consumedTimeMs);
    syncClockAfterMove(true);  // 次は自分の手番

    // CSA形式から座標を抽出（ハイライト用）
    QPoint from, to;
    int fromFile = 0, fromRank = 0;
    int toFile = 0, toRank = 0;
    if (move.length() >= 5) {
        fromFile = move[1].digitValue();
        fromRank = move[2].digitValue();
        toFile = move[3].digitValue();
        toRank = move[4].digitValue();

        if (toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9) {
            qCWarning(lcNetwork) << "Invalid CSA move coordinates: toFile=" << toFile << "toRank=" << toRank << "move=" << move;
            m_hooks.logMessage(tr("不正な座標の指し手を受信しました: %1").arg(move), true);
            m_hooks.errorOccurred(tr("サーバーからの指し手の座標が不正です: %1").arg(move));
            m_hooks.setGameState(GameState::Error);
            return;
        }

        if (fromFile == 0 && fromRank == 0) {
            from = QPoint(-1, -1);
        } else {
            from = QPoint(fromFile, fromRank);
        }
        to = QPoint(toFile, toRank);
    }

    // 成り判定（盤面更新前に行う）
    bool isPromotion = false;
    if (move.length() >= 7 && fromFile != 0 && fromRank != 0) {
        QString destPiece = move.mid(5, 2);
        static const QStringList promotedPieces = {
            QStringLiteral("TO"), QStringLiteral("NY"), QStringLiteral("NK"),
            QStringLiteral("NG"), QStringLiteral("UM"), QStringLiteral("RY")
        };
        if (promotedPieces.contains(destPiece)) {
            if (*m_refs.gameController && (*m_refs.gameController)->board()) {
                Piece srcPiece = (*m_refs.gameController)->board()->getPieceCharacter(fromFile, fromRank);
                static const QString unpromoted = QStringLiteral("PLNSBRplnsbr");
                if (unpromoted.contains(pieceToChar(srcPiece))) {
                    isPromotion = true;
                }
            }
        }
    }

    if (!CsaMoveConverter::applyMoveToBoard(move, *m_refs.gameController, *m_refs.usiMoves, *m_refs.sfenHistory, *m_refs.moveCount)) {
        m_hooks.logMessage(tr("指し手の適用に失敗しました: %1").arg(move), true);
        m_hooks.errorOccurred(tr("サーバーからの指し手を盤面に適用できません: %1").arg(move));
        m_hooks.setGameState(GameState::Error);
        return;
    }

    if (*m_refs.view) (*m_refs.view)->update();

    (*m_refs.moveCount)++;
    *m_refs.isMyTurn = true;
    m_hooks.turnChanged(true);

    m_hooks.moveHighlightRequested(from, to);

    QString usiMove = CsaMoveConverter::csaToUsi(move);
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

    bool isBlackMove = (move.length() > 0 && move[0] == QLatin1Char('+'));

    updateTimeTracking(isBlackMove, consumedTimeMs);
    syncClockAfterMove(false);  // 次は相手の手番

    // USI指し手リストとSFEN記録は更新する必要がある
    QString usiMove = CsaMoveConverter::csaToUsi(move);
    if (!usiMove.isEmpty()) {
        m_refs.usiMoves->append(usiMove);
    }

    // エンジンの指し手の場合のみSFEN記録を更新（人間はvalidateAndMoveで追加済み）
    if (*m_refs.playerType == PlayerType::Engine) {
        if (*m_refs.gameController && (*m_refs.gameController)->board() && *m_refs.sfenHistory) {
            QString boardSfen = (*m_refs.gameController)->board()->convertBoardToSfen();
            QString standSfen = (*m_refs.gameController)->board()->convertStandToSfen();
            QString currentPlayerStr = ((*m_refs.gameController)->currentPlayer() == ShogiGameController::Player1)
                                       ? QStringLiteral("b") : QStringLiteral("w");
            QString fullSfen = QString("%1 %2 %3 %4")
                                   .arg(boardSfen, currentPlayerStr, standSfen)
                                   .arg(*m_refs.moveCount + 1);
            (*m_refs.sfenHistory)->append(fullSfen);
        }
    }

    // CSA形式から座標を抽出（ハイライト用）
    QPoint from, to;
    int toFile = 0, toRank = 0;
    if (move.length() >= 5) {
        int fromFile = move[1].digitValue();
        int fromRank = move[2].digitValue();
        toFile = move[3].digitValue();
        toRank = move[4].digitValue();

        const bool invalidTo = (toFile < 1 || toFile > 9 || toRank < 1 || toRank > 9);
        const bool invalidFrom = !((fromFile == 0 && fromRank == 0) ||
                                   (fromFile >= 1 && fromFile <= 9 &&
                                    fromRank >= 1 && fromRank <= 9));
        if (invalidTo || invalidFrom) {
            qCWarning(lcNetwork) << "Invalid confirmed CSA move coordinates:"
                                 << "fromFile=" << fromFile << "fromRank=" << fromRank
                                 << "toFile=" << toFile << "toRank=" << toRank
                                 << "move=" << move;
            m_hooks.logMessage(tr("不正な座標の指し手確認を受信しました: %1").arg(move), true);
            m_hooks.errorOccurred(tr("サーバーからの指し手確認の座標が不正です: %1").arg(move));
            m_hooks.setGameState(GameState::Error);
            return;
        }

        if (fromFile == 0 && fromRank == 0) {
            from = QPoint(-1, -1);
        } else {
            from = QPoint(fromFile, fromRank);
        }
        to = QPoint(toFile, toRank);
    }

    (*m_refs.moveCount)++;
    *m_refs.isMyTurn = false;
    m_hooks.turnChanged(false);

    m_hooks.moveHighlightRequested(from, to);

    bool isPromotion = *m_refs.gameController ? (*m_refs.gameController)->promote() : false;
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

    // 時間パラメータを計算
    int byoyomiMs = m_refs.gameSummary->byoyomi * m_refs.gameSummary->timeUnitMs();
    int totalTimeMs = m_refs.gameSummary->totalTime * m_refs.gameSummary->timeUnitMs();
    int blackRemainMs = totalTimeMs - *m_refs.blackTotalTimeMs;
    int whiteRemainMs = totalTimeMs - *m_refs.whiteTotalTimeMs;
    if (blackRemainMs < 0) blackRemainMs = 0;
    if (whiteRemainMs < 0) whiteRemainMs = 0;
    int incMs = m_refs.gameSummary->increment * m_refs.gameSummary->timeUnitMs();

    CsaEngineController::ThinkingParams params;
    params.positionCmd = positionCmd;
    params.byoyomiMs = byoyomiMs;
    params.btimeStr = QString::number(blackRemainMs);
    params.wtimeStr = QString::number(whiteRemainMs);
    params.bincMs = incMs;
    params.wincMs = incMs;
    params.useByoyomi = (byoyomiMs > 0);

    m_hooks.logMessage(tr("エンジンが思考中..."), false);

    auto result = (*m_refs.engineController)->think(params);

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

    int fromFile = result.from.x();
    int fromRank = result.from.y();
    int toFile = result.to.x();
    int toRank = result.to.y();

    QChar turnSign = *m_refs.isBlackSide ? QLatin1Char('+') : QLatin1Char('-');
    QString csaPiece;

    bool isDrop = (fromFile >= 10);
    if (isDrop) {
        Piece piece = board->getPieceCharacter(fromFile, fromRank);
        csaPiece = CsaMoveConverter::pieceCharToCsa(piece, false);
        fromFile = 0;
        fromRank = 0;
    } else {
        Piece piece = board->getPieceCharacter(fromFile, fromRank);
        csaPiece = CsaMoveConverter::pieceCharToCsa(piece, result.promote);
    }

    QString csaMove = QString("%1%2%3%4%5%6")
        .arg(turnSign).arg(fromFile).arg(fromRank)
        .arg(toFile).arg(toRank).arg(csaPiece);

    m_hooks.logMessage(tr("CSA形式の指し手: %1").arg(csaMove), false);

    // 盤面を更新
    if (!isDrop) {
        Piece movingPiece = board->getPieceCharacter(result.from.x(), result.from.y());
        Piece capturedPiece = board->getPieceCharacter(toFile, toRank);
        if (capturedPiece != Piece::None) {
            board->addPieceToStand(capturedPiece);
        }
        board->movePieceToSquare(movingPiece, result.from.x(), result.from.y(), toFile, toRank, result.promote);
    } else {
        Piece dropPiece = board->getPieceCharacter(result.from.x(), result.from.y());
        board->decrementPieceOnStand(dropPiece);
        board->movePieceToSquare(dropPiece, 0, 0, toFile, toRank, false);
    }

    (*m_refs.gameController)->changeCurrentPlayer();
    if (*m_refs.view) (*m_refs.view)->update();

    // 評価値更新
    int ply = *m_refs.moveCount + 1;
    m_hooks.engineScoreUpdated(result.scoreCp, ply);

    // サーバーに送信
    m_refs.client->sendMove(csaMove);
}
