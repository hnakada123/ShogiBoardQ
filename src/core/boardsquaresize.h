#ifndef BOARDSQUARESIZE_H
#define BOARDSQUARESIZE_H

/// @file boardsquaresize.h
/// @brief 盤のマスの幅（px）の範囲

/**
 * @brief 盤のマスの幅（px）の範囲と既定値
 *
 * 盤の描画（views の BoardGeometry・ShogiView）・設定の宣言（AppSettings::squareSize など）・
 * CLI の盤面画像（render-board の square_size）で共通。範囲はここだけで決める。
 */
namespace BoardSquareSize {

inline constexpr int kMin = 20;
inline constexpr int kMax = 150;
inline constexpr int kDefault = 50;

} // namespace BoardSquareSize

#endif // BOARDSQUARESIZE_H
