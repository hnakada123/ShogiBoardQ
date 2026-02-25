/// @file fmvattacks.cpp
/// @brief 利き計算の実装

#include "fmvattacks.h"

namespace fmv {

namespace {

/// マスが盤内か判定
bool inBoard(int file, int rank) noexcept
{
    return file >= 0 && file < kBoardSize && rank >= 0 && rank < kBoardSize;
}

/// ステップ駒の利き判定: sqからdelta方向にattackerの該当駒があるかチェック
void checkStep(const EnginePosition& pos, Square sq, Color attacker,
               int df, int dr, PieceType pt, Bitboard81& result) noexcept
{
    int file = squareFile(sq) + df;
    int rank = squareRank(sq) + dr;
    if (!inBoard(file, rank)) {
        return;
    }
    Square from = toSquare(file, rank);
    int ci = static_cast<int>(attacker);
    if (pos.pieceOcc[ci][static_cast<int>(pt)].test(from)) {
        result.set(from);
    }
}

/// 走り駒の利き判定: sqからdf,dr方向にレイスキャンし、最初に見つかった駒がattackerの該当駒か
void checkRay(const EnginePosition& pos, Square sq, Color attacker,
              int df, int dr,
              PieceType slidePt1, PieceType slidePt2,
              Bitboard81& result) noexcept
{
    int file = squareFile(sq) + df;
    int rank = squareRank(sq) + dr;
    int ci = static_cast<int>(attacker);

    while (inBoard(file, rank)) {
        Square from = toSquare(file, rank);
        if (pos.occupied.test(from)) {
            // 最初に見つかった駒をチェック
            if (pos.pieceOcc[ci][static_cast<int>(slidePt1)].test(from)
                || pos.pieceOcc[ci][static_cast<int>(slidePt2)].test(from)) {
                result.set(from);
            }
            return; // 途中に駒があればそこで止まる
        }
        file += df;
        rank += dr;
    }
}

} // namespace

Bitboard81 attackersTo(const EnginePosition& pos, Square sq, Color attacker)
{
    Bitboard81 result;

    // attacker から見た「前方」方向
    // Black(先手)は上方向(-rank方向)、White(後手)は下方向(+rank方向)に攻撃する
    // したがって、sqから見て「逆方向」を探す:
    //   Black の歩は sq の rank+1 にいる → dr = +1
    //   White の歩は sq の rank-1 にいる → dr = -1
    int forward = (attacker == Color::Black) ? 1 : -1;

    // --- 歩 ---
    checkStep(pos, sq, attacker, 0, forward, PieceType::Pawn, result);

    // --- 桂 ---
    checkStep(pos, sq, attacker, -1, 2 * forward, PieceType::Knight, result);
    checkStep(pos, sq, attacker,  1, 2 * forward, PieceType::Knight, result);

    // --- 銀 ---
    checkStep(pos, sq, attacker, -1, forward,  PieceType::Silver, result);
    checkStep(pos, sq, attacker,  0, forward,  PieceType::Silver, result);
    checkStep(pos, sq, attacker,  1, forward,  PieceType::Silver, result);
    checkStep(pos, sq, attacker, -1, -forward, PieceType::Silver, result);
    checkStep(pos, sq, attacker,  1, -forward, PieceType::Silver, result);

    // --- 金/と/成香/成桂/成銀 ---
    // 金の動き: 前3マス + 横2マス + 後ろ1マス
    auto checkGoldStep = [&](PieceType pt) {
        checkStep(pos, sq, attacker, -1, forward,  pt, result);
        checkStep(pos, sq, attacker,  0, forward,  pt, result);
        checkStep(pos, sq, attacker,  1, forward,  pt, result);
        checkStep(pos, sq, attacker, -1, 0,        pt, result);
        checkStep(pos, sq, attacker,  1, 0,        pt, result);
        checkStep(pos, sq, attacker,  0, -forward, pt, result);
    };
    checkGoldStep(PieceType::Gold);
    checkGoldStep(PieceType::ProPawn);
    checkGoldStep(PieceType::ProLance);
    checkGoldStep(PieceType::ProKnight);
    checkGoldStep(PieceType::ProSilver);

    // --- 玉 ---
    for (int dr = -1; dr <= 1; ++dr) {
        for (int df = -1; df <= 1; ++df) {
            if (df == 0 && dr == 0) {
                continue;
            }
            checkStep(pos, sq, attacker, df, dr, PieceType::King, result);
        }
    }

    // --- 香 ---
    // 先手の香は上方向に走る → sqから下方向に探す
    checkRay(pos, sq, attacker, 0, forward, PieceType::Lance, PieceType::Lance, result);

    // --- 角・馬（斜め走り） ---
    checkRay(pos, sq, attacker, -1, -1, PieceType::Bishop, PieceType::Horse, result);
    checkRay(pos, sq, attacker,  1, -1, PieceType::Bishop, PieceType::Horse, result);
    checkRay(pos, sq, attacker, -1,  1, PieceType::Bishop, PieceType::Horse, result);
    checkRay(pos, sq, attacker,  1,  1, PieceType::Bishop, PieceType::Horse, result);

    // --- 飛・龍（縦横走り） ---
    checkRay(pos, sq, attacker, 0, -1, PieceType::Rook, PieceType::Dragon, result);
    checkRay(pos, sq, attacker, 0,  1, PieceType::Rook, PieceType::Dragon, result);
    checkRay(pos, sq, attacker, -1, 0, PieceType::Rook, PieceType::Dragon, result);
    checkRay(pos, sq, attacker,  1, 0, PieceType::Rook, PieceType::Dragon, result);

    // --- 馬のステップ部分（十字隣接） ---
    checkStep(pos, sq, attacker,  0, -1, PieceType::Horse, result);
    checkStep(pos, sq, attacker,  0,  1, PieceType::Horse, result);
    checkStep(pos, sq, attacker, -1,  0, PieceType::Horse, result);
    checkStep(pos, sq, attacker,  1,  0, PieceType::Horse, result);

    // --- 龍のステップ部分（斜め隣接） ---
    checkStep(pos, sq, attacker, -1, -1, PieceType::Dragon, result);
    checkStep(pos, sq, attacker,  1, -1, PieceType::Dragon, result);
    checkStep(pos, sq, attacker, -1,  1, PieceType::Dragon, result);
    checkStep(pos, sq, attacker,  1,  1, PieceType::Dragon, result);

    // 香のレイ判定では、先手の香 vs 後手の香は forward 方向が異なるため
    // 上の checkRay で正しく処理されている。ただし、checkRay は1方向のみ探すため
    // 香については forward 方向のみスキャンすれば十分。

    return result;
}

} // namespace fmv
