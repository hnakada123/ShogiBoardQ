#ifndef TSUMESHOGIEXPORTHEADERBUILDER_H
#define TSUMESHOGIEXPORTHEADERBUILDER_H

/// @file tsumeshogiexportheaderbuilder.h
/// @brief 詰将棋局面生成のファイル保存に付けるコメントヘッダの生成

#include "tsumeshogigenerator.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QString>
#include <QStringList>

/**
 * @brief 詰将棋局面生成の保存ファイル先頭に挿入するコメントヘッダを組み立てる
 *
 * ShogiBoardQ のバージョン、生成日時、生成設定を '#' 始まりの行にする。
 * コメント行は TsumeCollection::parse と SfenCollectionDialog の読み込みで読み飛ばされる。
 */
class TsumeshogiExportHeaderBuilder
{
    Q_DECLARE_TR_FUNCTIONS(TsumeshogiExportHeaderBuilder)

public:
    /**
     * @brief ヘッダ行（各行 "# " 始まり、改行なし）を返す
     * @param appVersion ShogiBoardQ のバージョン文字列（APP_VERSION）
     * @param generatedAt 生成を開始した日時
     * @param settings 生成に使った設定
     */
    static QStringList build(const QString& appVersion,
                             const QDateTime& generatedAt,
                             const TsumeshogiGenerator::Settings& settings);
};

#endif // TSUMESHOGIEXPORTHEADERBUILDER_H
