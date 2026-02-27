#ifndef KIFEXPORTER_H
#define KIFEXPORTER_H

/// @file kifexporter.h
/// @brief KIF形式棋譜エクスポータクラスの定義

#include "gamerecordmodel.h"

#include <QStringList>

/**
 * @brief KIF形式の棋譜エクスポートを行うクラス
 *
 * GameRecordModel から KIF 形式の棋譜テキストを生成する。
 * 分岐の指し手にも対応している。
 */
class KifExporter
{
public:
    /**
     * @brief KIF形式の行リストを生成
     * @param model 棋譜データモデル
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @return KIF形式の行リスト
     */
    static QStringList exportLines(const GameRecordModel& model,
                                   const GameRecordModel::ExportContext& ctx);

    /**
     * @brief SFENから開始局面のBOD行リストを生成
     * @param sfen SFEN形式の局面文字列
     * @return BOD形式の行リスト（空SFENまたは平手の場合は空リスト）
     *
     * KIF / KI2 の両方で非標準局面の表示に使用される。
     */
    static QStringList sfenToBodLines(const QString& sfen);
};

#endif // KIFEXPORTER_H
