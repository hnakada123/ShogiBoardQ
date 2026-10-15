#ifndef KIFUSAVEFILE_H
#define KIFUSAVEFILE_H

/// @file kifusavefile.h
/// @brief 棋譜の保存形式（拡張子）と、ファイルへの書き出し

#include <QString>
#include <QStringList>
#include <functional>

/**
 * @brief 棋譜の保存形式の判定と書き出し（ダイアログを出さない。GUI・自動化 API・CLI で共通）
 */
namespace KifuSaveFile {

/// 保存形式（保存先パスの拡張子から決定する）
enum class SaveFormat {
    Kif,   ///< .kif / .kifu（既定。未知の拡張子もこれ）
    Ki2,   ///< .ki2 / .ki2u
    Csa,   ///< .csa
    Jkf,   ///< .jkf
    Usen,  ///< .usen
    Usi,   ///< .usi
};

/// 保存形式が確定してから、その形式の出力だけを生成する。
using LineGenerator = std::function<QStringList(SaveFormat)>;

/// 保存先パスの拡張子から保存形式を判定する（大文字小文字は区別しない）
SaveFormat saveFormatForPath(const QString& path);

/// 拡張子のない保存パスに、選択されたフィルタの拡張子を付ける。
QString pathForSelectedFilter(const QString& path, const QString& selectedFilter);

/// 保存先パスの拡張子が保存形式として認識できるか（.kif/.kifu/.ki2/.ki2u/.csa/.jkf/.usen/.usi）
bool hasKnownSaveExtension(const QString& path);

/// 保存先パスの拡張子が Shift_JIS 出力を要求するか（.kif / .ki2）
bool usesShiftJisForPath(const QString& path);

/// Shift_JIS で表せない文字（missing）があるとき、UTF-8 で保存してよいか答える関数
using Utf8Fallback = std::function<bool(const QString& missing)>;

/**
 * @brief 拡張子に応じたエンコーディングで行を書き込む
 *
 * lines には saveFormatForPath(path) が返す形式の行を渡す。
 * .kif/.ki2 に Shift_JIS で表せない文字があれば allowUtf8 に聞き、許されれば UTF-8 で書く
 * （先頭行の encoding=UTF-8 宣言を残す）。allowUtf8 が無ければエラーにする。
 * allowUtf8 が断ったときは outError を空のまま false を返す。
 */
bool write(const QString& path, const QStringList& lines, QString* outError = nullptr,
           const Utf8Fallback& allowUtf8 = {});

} // namespace KifuSaveFile

#endif // KIFUSAVEFILE_H
