#ifndef USENEXPORTER_H
#define USENEXPORTER_H

/// @file usenexporter.h
/// @brief USEN形式棋譜エクスポータクラスの定義

#include "kifuexportmetadata.h"

class KifuBranchTree;

#include <QStringList>

/**
 * @brief USEN形式（Url Safe sfen-Extended Notation）の棋譜エクスポートを行うクラス
 *
 * 棋譜（分岐ツリー）と書き出しの文脈から USEN 形式の棋譜テキストを生成する。
 * 分岐の指し手にも対応している。
 * https://www.slideshare.net/slideshow/scalajs-web/92707205#15
 */
class UsenExporter
{
public:
    /**
     * @brief USEN形式の行リストを生成
     * @param recordTree 書き出す棋譜（分岐ツリー）
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return USEN形式の行リスト（1要素のUSEN文字列）
     */
    static QStringList exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx);
};

#endif // USENEXPORTER_H
