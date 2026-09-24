#ifndef APPLICATIONFONTS_H
#define APPLICATIONFONTS_H

#include <QFont>

/// アプリ内の日本語を日本語字形で表示するためのフォント選択。
namespace ApplicationFonts {

/// QApplication の生成・スタイル設定後、ウィジェット生成前に呼ぶ。
void initialize();

/// 棋譜貼り付け・通信ログ用。日本語の字形と等幅表示を両立する。
QFont monospaceFont();

} // namespace ApplicationFonts

#endif // APPLICATIONFONTS_H
