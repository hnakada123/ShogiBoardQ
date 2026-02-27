#ifndef USIEXPORTER_H
#define USIEXPORTER_H

/// @file usiexporter.h
/// @brief USI形式棋譜エクスポータクラスの定義

#include "gamerecordmodel.h"

#include <QStringList>

/**
 * @brief USIプロトコル形式（position コマンド文字列）の棋譜エクスポートを行うクラス
 *
 * GameRecordModel から USI position コマンド形式の棋譜テキストを生成する。
 * 分岐の指し手には対応していない（本譜のみ出力）。
 * https://shogidokoro2.stars.ne.jp/usi.html
 */
class UsiExporter
{
public:
    /**
     * @brief USI形式の行リストを生成
     * @param model 棋譜データモデル
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @param usiMoves USI形式の指し手リスト（本譜用）
     * @return USI形式の行リスト（1要素のposition コマンド文字列）
     */
    static QStringList exportLines(const GameRecordModel& model,
                                   const GameRecordModel::ExportContext& ctx,
                                   const QStringList& usiMoves);
};

#endif // USIEXPORTER_H
