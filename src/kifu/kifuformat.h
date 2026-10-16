#ifndef KIFUFORMAT_H
#define KIFUFORMAT_H

/// @file kifuformat.h
/// @brief 棋譜の形式（読み込み・保存・変換で共通）と、その判定

#include <QString>
#include <optional>

/// 棋譜の形式
enum class KifuFormat {
    Kif,   ///< .kif（Shift_JIS）・.kifu（UTF-8）
    Ki2,   ///< .ki2・.ki2u
    Csa,   ///< .csa
    Jkf,   ///< .jkf（JSON）
    Usi,   ///< .usi（position コマンド）
    Usen,  ///< .usen
    Sfen,  ///< .sfen（局面だけ。変換の出力では本譜の局面列）
    Bod,   ///< 局面図（貼り付けたテキストだけ）
};

namespace KifuFormats {

/// 形式の名前（"kif" "ki2" "csa" "jkf" "usi" "usen" "sfen"。"kifu" も読む）。CLI・自動化 API の引数に使う
std::optional<KifuFormat> fromName(const QString& name);
QString name(KifuFormat format);

/// 拡張子の形式（大文字小文字を区別しない）。分からなければ nullopt
std::optional<KifuFormat> fromPath(const QString& path);

/// 棋譜ファイルとして保存できる形式か（Kif・Ki2・Csa・Jkf・Usi・Usen）
bool isSavable(KifuFormat format);

/// テキストの内容から形式を判定する（分からなければ Kif）
KifuFormat detect(const QString& content);

/// ファイルの形式。拡張子で決まらない場合（.json や未知の拡張子）は内容から判定し、
/// それでも棋譜の形式でなければ Kif とみなす（局面だけのファイルは Usi）
KifuFormat detectFile(const QString& filePath);

} // namespace KifuFormats

#endif // KIFUFORMAT_H
