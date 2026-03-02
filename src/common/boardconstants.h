#ifndef BOARDCONSTANTS_H
#define BOARDCONSTANTS_H

/// @file boardconstants.h
/// @brief 将棋盤の座標・サイズ定数（共有ヘッダ）
///
/// # 座標系について
///
/// このヘッダは **ShogiBoard / UI 層の 1-indexed 座標系** に対応する定数を定義する。
///
/// | 範囲       | 筋（file）     | 意味                     |
/// |-----------|--------------|--------------------------|
/// | 盤上       | 1 〜 9        | 将棋盤の筋番号              |
/// | 先手駒台   | kBlackStandFile (10) | 先手の駒台                 |
/// | 後手駒台   | kWhiteStandFile (11) | 後手の駒台                 |
///
/// **注意**: ShogiMove / FMV / エンジン内部座標系は 0-indexed（盤上 0〜8、
/// 先手持ち駒 = EngineMoveValidator::BLACK_HAND_FILE = 9、
/// 後手持ち駒 = EngineMoveValidator::WHITE_HAND_FILE = 10）であり、
/// このヘッダの定数とは異なる。境界変換は fmvconverter.cpp で行う。

namespace BoardConstants {

constexpr int kBoardSize = 9;           ///< 盤面の1辺のマス数
constexpr int kNumBoardSquares = 81;    ///< 将棋盤の総マス数（kBoardSize * kBoardSize）
constexpr int kBlackStandFile = 10;     ///< 先手駒台のファイル番号（1-indexed）
constexpr int kWhiteStandFile = 11;     ///< 後手駒台のファイル番号（1-indexed）

} // namespace BoardConstants

#endif // BOARDCONSTANTS_H
