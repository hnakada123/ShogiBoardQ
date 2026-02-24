/// @file test_stubs.cpp
/// @brief テスト用スタブ - 本体モジュールが定義するシンボルの代替定義
///
/// テスト実行ファイルが依存する最小限のシンボルを提供する。
/// 本体の巨大な依存ツリーを回避するために使用。

// === ShogiView スタブ（settingsservice.cpp が参照する最小シンボル） ===
#include "shogiview.h"
int ShogiView::squareSize() const { return 50; }
void ShogiView::setSquareSize(int) {}
