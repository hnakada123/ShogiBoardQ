#ifndef USENEXPORTER_H
#define USENEXPORTER_H

/// @file usenexporter.h
/// @brief USEN形式棋譜エクスポータクラスの定義

#include "gamerecordmodel.h"

#include <QStringList>

/**
 * @brief USEN形式（Url Safe sfen-Extended Notation）の棋譜エクスポートを行うクラス
 *
 * GameRecordModel から USEN 形式の棋譜テキストを生成する。
 * 分岐の指し手にも対応している。
 * https://www.slideshare.net/slideshow/scalajs-web/92707205#15
 */
class UsenExporter
{
public:
    /**
     * @brief USEN形式の行リストを生成
     * @param model 棋譜データモデル
     * @param ctx 出力コンテキスト（ヘッダ情報等）
     * @param usiMoves USI形式の指し手リスト（本譜用）
     * @return USEN形式の行リスト（1要素のUSEN文字列）
     */
    static QStringList exportLines(const GameRecordModel& model,
                                   const GameRecordModel::ExportContext& ctx,
                                   const QStringList& usiMoves);
};

#endif // USENEXPORTER_H
