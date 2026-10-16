#ifndef JKFEXPORTER_H
#define JKFEXPORTER_H

/// @file jkfexporter.h
/// @brief JKF形式棋譜エクスポータクラスの定義

#include "kifuexportmetadata.h"

class KifuBranchTree;

#include <QStringList>

/**
 * @brief JKF形式（JSON棋譜フォーマット）の棋譜エクスポートを行うクラス
 *
 * 棋譜（分岐ツリー）と書き出しの文脈から JKF 形式の棋譜テキストを生成する。
 * 分岐の指し手にも対応している。
 * https://github.com/na2hiro/Kifu-for-JS/tree/master/packages/json-kifu-format
 */
class JkfExporter
{
public:
    /**
     * @brief JKF形式の行リストを生成
     * @param recordTree 書き出す棋譜（分岐ツリー）
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return JKF形式の行リスト（1要素のJSONテキスト）
     */
    static QStringList exportLines(const KifuBranchTree& recordTree, const KifuExportMetadata& ctx);
};

#endif // JKFEXPORTER_H
