#ifndef APPLICATIONFONTS_H
#define APPLICATIONFONTS_H

#include <QFont>

class QWidget;

/// 表示言語に応じたGUI共通の書体と、日本語棋譜の字形。
namespace ApplicationFonts {

/// QApplication の生成・スタイル設定後、ウィジェット生成前に呼ぶ。
void initialize(const QString& family = {}, const QString& language = QStringLiteral("ja_JP"));

/// 共通の書体を変更する。空文字列または未導入の書体は標準に戻す。
/// 既存の各画面で設定した文字サイズ・太さは保持する。
void applyFamily(const QString& family);

/// 表示言語に応じた標準の書体名。
QString defaultFamily();

/// 通信ログ用。表示言語に応じた字形と等幅表示を両立する。
QFont monospaceFont();

/// サイズ・装飾と利用者の指定を保ち、日本語棋譜の書体を選ぶ。
QFont japaneseFont(QFont base);

/// 日本語棋譜専用の部品を登録する。親の文字サイズ変更後も字形を維持する。
void useJapaneseFont(QWidget* widget, bool enabled = true);

} // namespace ApplicationFonts

#endif // APPLICATIONFONTS_H
