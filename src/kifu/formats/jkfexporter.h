#ifndef JKFEXPORTER_H
#define JKFEXPORTER_H

/// @file jkfexporter.h
/// @brief JKF形式棋譜エクスポータクラスの定義

#include "gamerecordmodel.h"

#include <QStringList>

/**
 * @brief JKF形式（JSON棋譜フォーマット）の棋譜エクスポートを行うクラス
 *
 * GameRecordModel から JKF 形式の棋譜テキストを生成する。
 * 分岐の指し手にも対応している。
 * https://github.com/na2hiro/Kifu-for-JS/tree/master/packages/json-kifu-format
 */
class JkfExporter
{
public:
    /**
     * @brief JKF形式の行リストを生成
     * @param model 棋譜データモデル
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return JKF形式の行リスト（1要素のJSONテキスト）
     */
    static QStringList exportLines(const GameRecordModel& model,
                                   const GameRecordModel::ExportContext& ctx);
};

#endif // JKFEXPORTER_H
