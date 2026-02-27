/// @file test_stubs_settings.cpp
/// @brief 設定ラウンドトリップテスト用のリンカースタブ
///
/// appsettings.cpp が参照する ShogiView メソッドのスタブ。
/// テストでは loadWindowSize() / saveWindowAndBoard() を呼ばないため、
/// 実行時にはヒットしない。

#include "shogiview.h"

int ShogiView::squareSize() const { return 50; }
void ShogiView::setSquareSize(int) {}
