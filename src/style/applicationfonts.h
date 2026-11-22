#ifndef APPLICATIONFONTS_H
#define APPLICATIONFONTS_H

#include <QFont>

class QWidget;

/// 表示言語に応じたGUI共通の書体と、日本語棋譜の字形。
namespace ApplicationFonts {

/// 画面を出さずに描く（offscreen。CLI の盤面画像・自動化 API・試験）ときにも、OS の書体で描けるようにする。
/// QApplication を作る前に呼ぶ。Windows の offscreen は書体を QT_QPA_FONTDIR（無ければ Qt の lib/fonts）からしか
/// 読まず、書体が1つも無いまま文字を描く（すべて四角になる）ので、Windows の書体のフォルダを渡す。
/// Windows の offscreen でなければ何もしない。
void prepareOffscreenFonts();

/// Windows の offscreen の既定の書体を、Windows の UI の書体（Segoe UI 9 ポイント。日本語・中国語は各言語の
/// UI の書体で補う）にする。offscreen は書体の名前の置き換えを持たず、既定の書体（"Sans Serif"）が名前の順で
/// 最初の書体（Agency FB など）になるため。QApplication を作った後、書体を決める（initialize）前に呼ぶ。
/// Windows の offscreen でなければ何もしない。
void useOffscreenSystemFont();

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
