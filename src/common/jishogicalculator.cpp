/// @file jishogicalculator.cpp
/// @brief 持将棋（入玉宣言法）の点数計算・判定ロジックの実装

#include "jishogicalculator.h"
#include <QObject>

// ============================================================================
// 点数計算
// ============================================================================

JishogiCalculator::JishogiResult JishogiCalculator::calculate(
    const QVector<QChar>& boardData,
    const QMap<QChar, int>& pieceStand)
{
    // 処理フロー:
    // 1. 盤面を走査して各駒の点数を集計
    // 2. 敵陣判定（先手: 1-3段目、後手: 7-9段目）
    // 3. 駒台の持ち駒を加算

    JishogiResult result;
    result.sente = {0, 0, 0, false};
    result.gote = {0, 0, 0, false};

    // 盤面のサイズは9x9=81
    // 段（rank）: 1-9, 筋（file）: 1-9
    // ShogiBoardでのインデックス = (rank - 1) * 9 + (file - 1)
    // 先手の敵陣: 1-3段目（rank 1-3）
    // 後手の敵陣: 7-9段目（rank 7-9）

    for (int file = 1; file <= 9; ++file) {
        for (int rank = 1; rank <= 9; ++rank) {
            int index = (rank - 1) * 9 + (file - 1);
            if (index < 0 || index >= boardData.size()) continue;

            QChar piece = boardData[index];
            if (piece == ' ' || piece == QChar()) continue;

            int points = getPiecePoints(piece);

            if (isSentePiece(piece)) {
                result.sente.totalPoints += points;

                if (piece == 'K') {
                    // 先手の玉が敵陣（1-3段目）にいるか
                    if (rank <= 3) {
                        result.sente.kingInEnemyTerritory = true;
                    }
                } else {
                    // 玉以外の駒が敵陣にいる場合
                    if (rank <= 3) {
                        result.sente.declarationPoints += points;
                        result.sente.piecesInEnemyTerritory++;
                    }
                }
            } else if (isGotePiece(piece)) {
                result.gote.totalPoints += points;

                if (piece == 'k') {
                    // 後手の玉が敵陣（7-9段目）にいるか
                    if (rank >= 7) {
                        result.gote.kingInEnemyTerritory = true;
                    }
                } else {
                    // 玉以外の駒が敵陣にいる場合
                    if (rank >= 7) {
                        result.gote.declarationPoints += points;
                        result.gote.piecesInEnemyTerritory++;
                    }
                }
            }
        }
    }

    // 駒台の駒を加算
    // 先手の持ち駒: 大文字（P, L, N, S, G, B, R）
    // 後手の持ち駒: 小文字（p, l, n, s, g, b, r）
    for (auto it = pieceStand.constBegin(); it != pieceStand.constEnd(); ++it) {
        QChar piece = it.key();
        int count = it.value();
        int points = getPiecePoints(piece) * count;

        if (isSentePiece(piece)) {
            result.sente.totalPoints += points;
            result.sente.declarationPoints += points;
        } else if (isGotePiece(piece)) {
            result.gote.totalPoints += points;
            result.gote.declarationPoints += points;
        }
    }

    return result;
}

// ============================================================================
// 宣言条件判定
// ============================================================================

bool JishogiCalculator::meetsDeclarationConditions(const PlayerScore& score, bool kingInCheck)
{
    // 王手がかかっている場合は宣言できない
    if (kingInCheck) {
        return false;
    }
    return score.kingInEnemyTerritory && score.piecesInEnemyTerritory >= 10;
}

// ============================================================================
// 判定結果文字列
// ============================================================================

/// 24点法: 31点以上→勝ち、24〜30点→引き分け、24点未満→負け
QString JishogiCalculator::getResult24(const PlayerScore& score, bool kingInCheck)
{
    if (!meetsDeclarationConditions(score, kingInCheck)) {
        return QObject::tr("負け");
    }

    if (score.declarationPoints >= 31) {
        return QObject::tr("勝ち");
    } else if (score.declarationPoints >= 24) {
        return QObject::tr("引き分け");
    } else {
        return QObject::tr("負け");
    }
}

/// 27点法: 先手28点以上、後手27点以上で勝ち
QString JishogiCalculator::getResult27(const PlayerScore& score, bool isSente, bool kingInCheck)
{
    if (!meetsDeclarationConditions(score, kingInCheck)) {
        return QObject::tr("負け");
    }

    int requiredPoints = isSente ? 28 : 27;

    if (score.declarationPoints >= requiredPoints) {
        return QObject::tr("勝ち");
    } else {
        return QObject::tr("負け");
    }
}

// ============================================================================
// 内部ヘルパー
// ============================================================================

/// 成駒は成る前の駒と同じ点数
int JishogiCalculator::getPiecePoints(QChar piece)
{
    // 大駒（飛車・角・龍・馬）: 5点
    // 小駒（金・銀・桂・香・歩・と金・成香・成桂・成銀）: 1点
    // 玉: 0点
    QChar upper = piece.toUpper();

    if (upper == 'K') {
        return 0;
    }

    // 大駒: R（飛車）, B（角）, U（龍）, C（馬）
    if (isMajorPiece(piece)) {
        return 5;
    }

    // それ以外の駒（小駒）: 1点
    // P（歩）, L（香車）, N（桂馬）, S（銀）, G（金）
    // Q（と金）, M（成香）, O（成桂）, T（成銀）
    return 1;
}

bool JishogiCalculator::isMajorPiece(QChar piece)
{
    QChar upper = piece.toUpper();
    // R: 飛車, U: 龍（成り飛車）, B: 角, C: 馬（成り角）
    return upper == 'R' || upper == 'B' || upper == 'U' || upper == 'C';
}

bool JishogiCalculator::isSentePiece(QChar piece)
{
    return piece.isUpper();
}

bool JishogiCalculator::isGotePiece(QChar piece)
{
    return piece.isLower();
}
