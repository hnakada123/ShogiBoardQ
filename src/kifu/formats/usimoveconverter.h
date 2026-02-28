#ifndef USIMOVECONVERTER_H
#define USIMOVECONVERTER_H

/// @file usimoveconverter.h
/// @brief USI指し手変換クラスの定義

#include <QStringList>
#include <QVector>

struct ShogiMove;

/**
 * @brief USI形式の指し手文字列を生成する変換クラス
 *
 * ShogiMoveリストやSFENレコードからUSI形式の指し手リストへの変換を行う。
 */
class UsiMoveConverter
{
public:
    /**
     * @brief ShogiMoveリストからUSI指し手リストを生成
     * @param moves ShogiMove構造体のリスト
     * @return USI形式の指し手文字列リスト
     */
    static QStringList fromGameMoves(const QVector<ShogiMove>& moves);

    /**
     * @brief SFENレコード（局面列）からUSI指し手リストを生成
     *
     * 連続する2局面のSFEN差分から指し手を推定する。
     * @param sfenRecord SFEN形式の局面文字列リスト（初期局面含む）
     * @return USI形式の指し手文字列リスト
     */
    static QStringList fromSfenRecord(const QStringList& sfenRecord);
};

#endif // USIMOVECONVERTER_H
