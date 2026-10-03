#ifndef APPLICATIONFONTS_H
#define APPLICATIONFONTS_H

#include <QFont>

/// GUI共通の書体選択と、日本語字形のフォールバック。
namespace ApplicationFonts {

/// QApplication の生成・スタイル設定後、ウィジェット生成前に呼ぶ。
void initialize(const QString& family = {});

/// 共通の書体を変更する。空文字列または未導入の書体は標準に戻す。
/// 既存の各画面で設定した文字サイズ・太さは保持する。
void applyFamily(const QString& family);

/// 日本語表示を考慮した標準の書体名。
QString defaultFamily();

/// 棋譜貼り付け・通信ログ用。日本語の字形と等幅表示を両立する。
QFont monospaceFont();

} // namespace ApplicationFonts

#endif // APPLICATIONFONTS_H
