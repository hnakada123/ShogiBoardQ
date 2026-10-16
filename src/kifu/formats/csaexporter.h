#ifndef CSAEXPORTER_H
#define CSAEXPORTER_H

/// @file csaexporter.h
/// @brief CSA形式棋譜エクスポータクラスの定義

#include "kifuexportmetadata.h"

class KifuBranchTree;

#include <QStringList>

/**
 * @brief CSA形式の棋譜エクスポートを行うクラス
 *
 * 棋譜（分岐ツリー）と書き出しの文脈から CSA V3.0 形式の棋譜テキストを生成する。
 * 分岐の指し手には対応していない（本譜のみ出力）。
 */
class CsaExporter
{
public:
    /**
     * @brief CSA形式の行リストを生成
     * @param recordTree 書き出す棋譜（分岐ツリー）
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return CSA形式の行リスト
     */
    static QStringList exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx);
};

#endif // CSAEXPORTER_H
