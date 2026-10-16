#ifndef CSAEXPORTER_H
#define CSAEXPORTER_H

/// @file csaexporter.h
/// @brief CSA形式棋譜エクスポータクラスの定義

#include "gamerecordmodel.h"

#include <QStringList>

/**
 * @brief CSA形式の棋譜エクスポートを行うクラス
 *
 * GameRecordModel から CSA V3.0 形式の棋譜テキストを生成する。
 * 分岐の指し手には対応していない（本譜のみ出力）。
 */
class CsaExporter
{
public:
    /**
     * @brief CSA形式の行リストを生成
     * @param model 棋譜データモデル
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return CSA形式の行リスト
     */
    static QStringList exportLines(const GameRecordModel& model, const GameRecordModel::ExportContext& ctx);
};

#endif // CSAEXPORTER_H
