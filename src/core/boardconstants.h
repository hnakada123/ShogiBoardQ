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
/// | 駒箱       | kPieceBoxFile (12) | 局面編集用の未使用駒         |
///
/// **注意**: 指し手（ShogiMove・UsiMove の BoardSquare）は盤上を 1〜9 で表し、駒台・駒箱の筋は持たない
/// （駒打ちは UsiMove::drop で表す）。0 始まりの座標を使うのはビットボード（fmv）の内部だけで、
/// 境界の変換は fmvconverter.cpp で行う。

namespace BoardConstants {

constexpr int kBoardSize = 9;           ///< 盤面の1辺のマス数
constexpr int kNumBoardSquares = 81;    ///< 将棋盤の総マス数（kBoardSize * kBoardSize）
constexpr int kBlackStandFile = 10;     ///< 先手駒台のファイル番号（1-indexed）
constexpr int kWhiteStandFile = 11;     ///< 後手駒台のファイル番号（1-indexed）
constexpr int kPieceBoxFile = 12;       ///< 駒箱（段は先手駒台と同じ1〜8）

} // namespace BoardConstants

#endif // BOARDCONSTANTS_H
