#ifndef FASTMOVEVALIDATOR_H
#define FASTMOVEVALIDATOR_H

/// @file fastmovevalidator.h
/// @brief 高速な合法手判定クラスの定義

#include <QChar>
#include <QMap>
#include <QVector>

#include "legalmovestatus.h"
#include "shogimove.h"

/**
 * @brief 将棋の合法手判定・合法手数生成・王手判定を行う軽量バリデータ
 *
 * 盤面状態（81マス）と駒台情報を入力として、単一手の合法性判定と
 * 局面全体の合法手数生成を提供する。
 * 本クラスは QObject 非依存で、ゲームロジック層から直接利用する。
 */
class FastMoveValidator
{
public:
    /**
     * @brief 手番種別
     */
    enum Turn {
        BLACK,      ///< 先手
        WHITE,      ///< 後手
        TURN_SIZE   ///< 手番種別数
    };

    static constexpr int BOARD_SIZE = 9;                            ///< 盤の1辺のマス数
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE; ///< 盤上マス総数（81）
    static constexpr int BLACK_HAND_FILE = BOARD_SIZE;              ///< 先手駒台の疑似ファイル座標
    static constexpr int WHITE_HAND_FILE = BOARD_SIZE + 1;          ///< 後手駒台の疑似ファイル座標

    /**
     * @brief 単一の指し手が合法か判定する
     * @param turn 現在手番
     * @param boardData 盤面（81マス）
     * @param pieceStand 駒台
     * @param currentMove 判定対象手（成り/不成情報を含む）
     * @return 成り手/不成手の合法可否を保持するステータス
     */
    LegalMoveStatus isLegalMove(const Turn& turn,
                                const QVector<QChar>& boardData,
                                const QMap<QChar, int>& pieceStand,
                                ShogiMove& currentMove) const;

    /**
     * @brief 局面の合法手数を生成して返す
     * @param turn 現在手番
     * @param boardData 盤面（81マス）
     * @param pieceStand 駒台
     * @return 合法手数
     */
    int generateLegalMoves(const Turn& turn,
                           const QVector<QChar>& boardData,
                           const QMap<QChar, int>& pieceStand) const;

    /**
     * @brief 指定手番の玉が王手されている数を返す
     * @param turn 王手有無を調べる側の手番
     * @param boardData 盤面（81マス）
     * @return 王手している相手駒の数（0以上）
     */
    int checkIfKingInCheck(const Turn& turn,
                           const QVector<QChar>& boardData) const;
};

#endif // FASTMOVEVALIDATOR_H
