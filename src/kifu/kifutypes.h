#ifndef KIFUTYPES_H
#define KIFUTYPES_H

/// @file kifutypes.h
/// @brief 棋譜関連の共通データ型定義

#include <QString>
#include <QList>

#include "kifdisplayitem.h"
#include "shogimove.h"

/**
 * @brief 分岐解決済みの1行分のデータを保持する構造体
 *
 * 棋譜読み込み後、分岐を含む棋譜データを行単位で平坦化した結果。
 *
 */
struct ResolvedRow {
    int startPly = 1;               ///< 開始手数
    int parent   = -1;              ///< 親行のインデックス（本譜は -1）
    QList<KifDisplayItem> disp;     ///< 表示用アイテムリスト
    QStringList sfen;               ///< 各手数のSFENリスト
    QList<ShogiMove> gm;          ///< 指し手リスト
    int varIndex = -1;              ///< 変化インデックス（本譜 = -1）
    QStringList comments;           ///< 各手のコメント
};

/**
 * @brief ライブ対局の状態を保持する構造体
 *
 * 読み込んだ棋譜データとは分離して、進行中の対局データを管理する。
 *
 */
struct LiveGameState {
    int anchorPly = -1;              ///< 分岐起点の手数（-1 = 初期局面から）
    int parentRowIndex = -1;         ///< 親行のインデックス（-1 = 本譜）
    QList<KifDisplayItem> moves;     ///< ライブ対局で追加された手
    QStringList sfens;               ///< SFEN列
    QList<ShogiMove> gameMoves;    ///< 指し手
    bool isActive = false;           ///< ライブ対局中かどうか

    int totalPly() const { return anchorPly + static_cast<int>(moves.size()); }

    void clear() {
        anchorPly = -1;
        parentRowIndex = -1;
        moves.clear();
        sfens.clear();
        gameMoves.clear();
        isActive = false;
    }
};

#endif // KIFUTYPES_H
