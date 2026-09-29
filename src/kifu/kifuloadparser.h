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
    QStringList sfens;
    QStringList positionCommands;
    QList<ShogiMove> moves;
};

namespace KifuLoadParser {
KifuLoadResult parseFile(const QString& path, KifuFileReader::KifuFormat format = KifuFileReader::KifuFormat::Unknown,
                         const CancelFlag& cancel = {});
KifuLoadResult parseText(const QString& content, const CancelFlag& cancel = {});
}

#endif // KIFULOADPARSER_H
