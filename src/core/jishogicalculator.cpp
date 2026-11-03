/// @file jishogicalculator.cpp
/// @brief 持将棋（入玉宣言法）の点数計算・判定ロジックの実装

#include "jishogicalculator.h"

#include "piecerules.h"

// ============================================================================
// 点数計算
// ============================================================================

JishogiCalculator::JishogiResult JishogiCalculator::calculate(const Position& position)
{
    // 処理フロー:
    // 1. 盤面を走査して各駒の点数を集計
    // 2. 敵陣判定（成れる段）
    // 3. 駒台の持ち駒を加算

    JishogiResult result;
    result.sente = {0, 0, 0, false};
    result.gote = {0, 0, 0, false};

    // 敵陣は成れる段（PieceRules::inPromotionZone。先手は1〜3段目、後手は7〜9段目）
    for (int file = 1; file <= 9; ++file) {
        for (int rank = 1; rank <= 9; ++rank) {
            const Piece piece = position.pieceAt(file, rank);
            if (piece == Piece::None) continue;

            const int points = piecePoints(piece);
            PlayerScore& score = isSentePiece(piece) ? result.sente : result.gote;
            const bool inCamp = PieceRules::inPromotionZone(pieceSide(piece), rank);
            score.totalPoints += points;
            if (pieceKind(piece) == PieceKind::King) {
                if (inCamp) score.kingInEnemyTerritory = true;
            } else if (inCamp) {
                // 玉以外の駒が敵陣にいる場合
                score.declarationPoints += points;
                score.piecesInEnemyTerritory++;
            }
        }
    }

    // 駒台の駒を加算
    for (int slot = 0; slot < 2 * kHandKindCount; ++slot) {
        const Piece piece = makePiece(slot < kHandKindCount ? Turn::Black : Turn::White,
                                      static_cast<PieceKind>(slot % kHandKindCount));
        const int points = piecePoints(piece) * position.handCount(piece);

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

bool JishogiCalculator::hasEnoughPiecesInCamp(const PlayerScore& score)
{
    return score.piecesInEnemyTerritory >= kRequiredPiecesInCamp;
}

bool JishogiCalculator::meetsDeclarationConditions(const PlayerScore& score, bool kingInCheck)
{
    // 王手がかかっている場合は宣言できない
    if (kingInCheck) {
        return false;
    }
    return score.kingInEnemyTerritory && hasEnoughPiecesInCamp(score);
}

// ============================================================================
// 宣言の判定
// ============================================================================

int JishogiCalculator::winningPoints(Rule rule, bool isSente)
{
    if (rule == Rule::Points24) return 31;
    return isSente ? 28 : 27;
}

int JishogiCalculator::requiredPoints(Rule rule, bool isSente)
{
    return rule == Rule::Points24 ? 24 : winningPoints(rule, isSente);
}

JishogiCalculator::Verdict JishogiCalculator::judge(const PlayerScore& score, Rule rule, bool isSente,
                                                    bool kingInCheck)
{
    if (!meetsDeclarationConditions(score, kingInCheck)) return Verdict::NotDeclarable;
    if (score.declarationPoints >= winningPoints(rule, isSente)) return Verdict::Win;
    if (score.declarationPoints >= requiredPoints(rule, isSente)) return Verdict::Draw;
    return Verdict::ShortOfPoints;
}

// ============================================================================
// 内部ヘルパー
// ============================================================================

/// 成駒は成る前の駒と同じ点数
int JishogiCalculator::piecePoints(Piece piece)
{
    // 大駒（飛車・角・龍・馬）: kMajorPiecePoints
    // 小駒（金・銀・桂・香・歩・と金・成香・成桂・成銀）: kMinorPiecePoints
    // 玉: 0点
    if (piece == Piece::BlackKing || piece == Piece::WhiteKing) {
        return 0;
    }

    if (isMajorPiece(piece)) {
        return kMajorPiecePoints;
    }

    return kMinorPiecePoints;
}

bool JishogiCalculator::isMajorPiece(Piece piece)
{
    switch (piece) {
    case Piece::BlackRook: case Piece::BlackBishop: case Piece::BlackDragon: case Piece::BlackHorse:
    case Piece::WhiteRook: case Piece::WhiteBishop: case Piece::WhiteDragon: case Piece::WhiteHorse:
        return true;
    default:
        return false;
    }
}

bool JishogiCalculator::isSentePiece(Piece piece)
{
    return isBlackPiece(piece);
}

bool JishogiCalculator::isGotePiece(Piece piece)
{
    return isWhitePiece(piece);
}
