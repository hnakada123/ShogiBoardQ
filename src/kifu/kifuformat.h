#ifndef KIFUFORMAT_H
#define KIFUFORMAT_H

/// @file kifuformat.h
/// @brief 棋譜の形式（読み込み・保存・変換で共通）と、その判定

#include <QString>
#include <QStringList>
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

/// 棋譜ファイルの文字コード（KIF・KI2・CSA は先頭行に宣言を書く）
enum class KifuEncoding {
    Utf8,      ///< UTF-8
    ShiftJis,  ///< Shift_JIS（.kif・.ki2）
};

namespace KifuFormats {

/// 棋譜を保存できる拡張子1つ（保存ダイアログの絞り込みの並び）
struct SaveTarget {
    QString suffix;         ///< 拡張子（"kifu" など）
    KifuFormat format;      ///< 書く形式
    KifuEncoding encoding;  ///< 書く文字コード
    const char* label;      ///< 保存ダイアログの絞り込みの見出し（翻訳前。文脈は "KifuFileFilter"）
};

/// 形式の名前（"kif" "ki2" "csa" "jkf" "usi" "usen" "sfen"。"kifu" も読む）。CLI・自動化 API の引数に使う
std::optional<KifuFormat> fromName(const QString& name);
QString name(KifuFormat format);

/// 拡張子の形式（大文字小文字を区別しない）。分からなければ nullopt
std::optional<KifuFormat> fromPath(const QString& path);

/// 棋譜ファイルとして保存できる形式か（Kif・Ki2・Csa・Jkf・Usi・Usen）
bool isSavable(KifuFormat format);
/// 保存できる形式の名前（"kif" "ki2" "csa" "jkf" "usi" "usen"。CLI・自動化 API の引数の選択肢）
QStringList savableNames();
/// 保存できる拡張子の一覧（保存ダイアログの絞り込みの並び。UTF-8 の .kifu が先頭）
const QList<SaveTarget>& saveTargets();
/// 保存先 path の拡張子の形式と文字コード（大文字小文字を区別しない。保存できない拡張子は nullopt）
std::optional<SaveTarget> saveTargetForPath(const QString& path);

/// ファイルとして開ける形式の拡張子（"kifu" "kif" … と、JKF の "json"）。形式の指定がなければ全部
QStringList readableSuffixes(std::optional<KifuFormat> format = std::nullopt);
/// 開ける棋譜ファイルか（拡張子が readableSuffixes にある。.json は中身で JKF か判定する）
bool isReadableFile(const QString& path);
/// 変換・保存で書く棋譜ファイルの拡張子（"kifu" "kif" … "sfen"。読むだけの "json" は含まない）
QStringList writableSuffixes();

/// テキストの内容から形式を判定する（分からなければ Kif）
KifuFormat detect(const QString& content);

/// ファイルの形式。拡張子で決まらない場合（.json や未知の拡張子）は内容から判定し、
/// それでも棋譜の形式でなければ Kif とみなす（局面だけのファイルは Usi）
KifuFormat detectFile(const QString& filePath);

} // namespace KifuFormats

#endif // KIFUFORMAT_H
