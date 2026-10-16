#ifndef KI2EXPORTER_H
#define KI2EXPORTER_H

/// @file ki2exporter.h
/// @brief KI2形式棋譜エクスポータクラスの定義

#include "kifuexportmetadata.h"

class KifuBranchTree;

#include <QStringList>

/**
 * @brief KI2形式の棋譜エクスポートを行うクラス
 *
 * 棋譜（分岐ツリー）と書き出しの文脈から KI2 形式の棋譜テキストを生成する。
 * KI2形式は消費時間を含まず、指し手は手番記号（▲△）付きで出力される。
 * 分岐の指し手にも対応している。
 */
class Ki2Exporter
{
public:
    /**
     * @brief KI2形式の行リストを生成
     * @param recordTree 書き出す棋譜（分岐ツリー）
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return KI2形式の行リスト
     */
    static QStringList exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx);
};

#endif // KI2EXPORTER_H
