#ifndef KIFULOADPARSER_H
#define KIFULOADPARSER_H

/// @file kifuloadparser.h
/// @brief 棋譜の読み込み口（ファイルの文字コードの判別と、形式ごとの変換器への振り分け）

#include "kifuformat.h"
#include "kifparsetypes.h"
#include "threadtypes.h"
#include "shogimove.h"

/// GUIオブジェクトを含まない読み込み結果。ワーカーから値で受け渡す。
struct KifuLoadResult {
    bool success = false;
    KifuFormat format = KifuFormat::Kif;  ///< 読んだ形式
    QString filePath;
    QString initialSfen;
    QString teaiLabel;
    QString positionOnly;
    QString error;
    QString warning;
    QList<KifGameInfoItem> gameInfo;
    KifParseResult record;
};

namespace KifuLoadParser {
/// ファイルを1回読み（文字コードは形式ごとに判別）、parseContent に渡す。形式がなければファイルから判定する
KifuLoadResult parseFile(const QString& path, std::optional<KifuFormat> format = std::nullopt,
                         const CancelFlag& cancel = {});
/// 貼り付けたテキストの形式を判定して読む
KifuLoadResult parseText(const QString& content, const CancelFlag& cancel = {});
/// 形式が決まっているテキストを読む（指し手のない USI・SFEN と BOD は局面だけを positionOnly に返す）。
/// sourceName は失敗時のメッセージに使う（ファイル名など。空でもよい）
KifuLoadResult parseContent(const QString& content, KifuFormat format,
                            const CancelFlag& cancel = {}, const QString& sourceName = QString());
/// 棋譜ファイルをテキストとして読む（CSA は CSA の文字コード宣言、それ以外は自動判別）
bool readFileText(const QString& path, KifuFormat format, QString& outText, QString* warn);
}

#endif // KIFULOADPARSER_H
