/// @file offscreen_test_font.cpp
/// @brief offscreen で試験するときの既定の書体（すべての試験の実行ファイルに入れる）
///
/// Windows の offscreen は、書体の名前の置き換えを持たず、既定の書体が名前の順で最初の書体になる。
/// アプリ・CLI と同じく、起動したらすぐに Windows の UI の書体にする（ApplicationFonts::useOffscreenSystemFont。
/// 書体のフォルダは tests/CMakeLists.txt が QT_QPA_FONTDIR で渡す）。Windows の offscreen でなければ何もしない。

#include <QCoreApplication>

#include "applicationfonts.h"

namespace {

void useOffscreenSystemFont()
{
    ApplicationFonts::useOffscreenSystemFont();
}

} // namespace

Q_COREAPP_STARTUP_FUNCTION(useOffscreenSystemFont)
