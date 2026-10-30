#ifndef KIFUSAVEFILE_H
#define KIFUSAVEFILE_H

/// @file kifusavefile.h
/// @brief 棋譜の保存形式と文字コード（拡張子から決める）、ファイルへの書き出し、既定のファイル名

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <functional>

#include "kifuformat.h"

/**
 * @brief 棋譜の保存（ダイアログを出さない。GUI・自動化 API・CLI で共通）
 *
 * 保存形式と文字コードは保存先の拡張子から形式の表（KifuFormats::saveTargets）で決める。
 * 行は書き出し（KifuExport::lines）にその文字コードを渡して作り、先頭行の文字コードの宣言もそれに合わせる。
 */
namespace KifuSaveFile {

/// 保存形式と文字コードを決めてから、その形式・文字コードの行だけを作る
using LineGenerator = std::function<QStringList(KifuFormat, KifuEncoding)>;

/// 保存先パスの拡張子から保存形式を判定する（大文字小文字は区別しない。保存できない拡張子は Kif）
KifuFormat saveFormatForPath(const QString& path);

/// 保存先パスの拡張子の文字コード（.kif・.ki2 は Shift_JIS。ほかと保存できない拡張子は UTF-8）
KifuEncoding encodingForPath(const QString& path);

/// 保存先パスの拡張子が保存形式として認識できるか（.kif/.kifu/.ki2/.ki2u/.csa/.jkf/.usen/.usi）
bool hasKnownSaveExtension(const QString& path);

/// 保存できる拡張子を "." を付けて並べた文（".kifu, .kif, …"。知らせの文に使う）
QString knownSaveExtensionsText();

/// Shift_JIS で表せない文字（missing）があるとき、UTF-8 で保存してよいか答える関数
using Utf8Fallback = std::function<bool(const QString& missing)>;

/**
 * @brief 保存先の拡張子の形式と文字コードで行を作って書く
 *
 * .kif・.ki2 で Shift_JIS で表せない文字があれば allowUtf8 に聞き、許されれば UTF-8 で行を作り直して書く
 * （先頭行は encoding=UTF-8 の宣言になる）。allowUtf8 が無ければエラーにする。
 * allowUtf8 が断ったときは outError を空のまま false を返す。
 */
bool write(const QString& path, const LineGenerator& generate, QString* outError = nullptr,
           const Utf8Fallback& allowUtf8 = {});

/**
 * @brief 文字コード encoding で作った行を書く（CLI の変換の出力。形式は呼び出し側が決める）
 *
 * Shift_JIS で表せない文字があれば、その文字を outError に書いて false を返す。
 */
bool writeLines(const QString& path, const QStringList& lines, KifuEncoding encoding, QString* outError = nullptr);

/// 保存ファイル名の既定値（日時_先手_vs_後手.拡張子。対局者の名前が無ければ 日時_Session.拡張子）。
/// 名前は対局情報の対局者名（駒落ちは下手・上手）を渡す
QString makeDefaultSaveFileName(const QString& blackName, const QString& whiteName, const QDateTime& now,
                                const QString& extension = QStringLiteral("kifu"));

/// Shift_JIS で表せない文字を、出現順・重複なしで最大 maxCount 個連結して返す。すべて表せる場合は空文字。
QString charactersNotInShiftJis(const QString& text, int maxCount = 10);

} // namespace KifuSaveFile

#endif // KIFUSAVEFILE_H
