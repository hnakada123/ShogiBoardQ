#ifndef NOTATIONSTYLE_H
#define NOTATIONSTYLE_H

/// @file notationstyle.h
/// @brief 表示言語と棋譜の表記の設定（起動時に1度決め、KifuPresentation が読む）

#include <QString>

/**
 * @brief 画面に出す指し手の表記（日本語・英語）と移動元の付け方の設定
 *
 * 表示用の設定なので、棋譜のファイル・プロトコルには使わない。起動時に ApplicationTranslations が
 * configure() で決め、表記を作る側（kifu の KifuPresentation）が options() を読む。
 */
namespace NotationStyle {

enum class Notation { Japanese, Western };

struct Options {
    Notation notation = Notation::Japanese;
    bool alwaysOrigin = false; ///< 英語表記で、紛れがなくても移動元を書く
};

/// 言語の設定（"system" ならシステムの言語）を、翻訳ファイルの言語（ja_JP・zh_CN・zh_TW・en）にする
QString resolveLanguage(const QString& setting, const QString& systemLocale);
/// 表記の設定（notation: "japanese"・"western"・それ以外は言語で決める）
void configure(const QString& language, const QString& notation, bool alwaysOrigin);
Options options();
/// configure() の回数（表示用の文字列を覚えた側が作り直すかの判定に使う）
int revision();

} // namespace NotationStyle

#endif // NOTATIONSTYLE_H
