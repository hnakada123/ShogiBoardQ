#ifndef KIFULOADPARSER_H
#define KIFULOADPARSER_H

#include "kifufilereader.h"
#include "kiftosfenconverter.h"
#include "threadtypes.h"
#include "shogimove.h"

/// GUIオブジェクトを含まない読み込み結果。ワーカーから値で受け渡す。
struct KifuLoadResult {
    bool success = false;
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
/// ファイルを1回読み（文字コードは形式ごとに判別）、parseContent に渡す
KifuLoadResult parseFile(const QString& path, KifuFileReader::KifuFormat format = KifuFileReader::KifuFormat::Unknown,
                         const CancelFlag& cancel = {});
/// 貼り付けたテキストの形式を判定して読む（SFEN・BOD は局面だけを返す）
KifuLoadResult parseText(const QString& content, const CancelFlag& cancel = {});
/// 形式が決まっているテキストを読む。sourceName は失敗時のメッセージに使う（ファイル名など。空でもよい）
KifuLoadResult parseContent(const QString& content, KifuFileReader::KifuFormat format,
                            const CancelFlag& cancel = {}, const QString& sourceName = QString());
/// 棋譜ファイルをテキストとして読む（CSA は CSA の文字コード宣言、それ以外は自動判別）
bool readFileText(const QString& path, KifuFileReader::KifuFormat format, QString& outText, QString* warn);
}

#endif // KIFULOADPARSER_H
