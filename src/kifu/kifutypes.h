#ifndef KIFUTYPES_H
#define KIFUTYPES_H

#include <QString>
#include <QVector>

#include "kifdisplayitem.h"
#include "shogimove.h"

struct ResolvedRow {
    int startPly = 1;
    int parent   = -1;                 // ★追加：親行。Main は -1
    QList<KifDisplayItem> disp;
    QStringList sfen;
    QVector<ShogiMove> gm;
    int varIndex = -1;                 // 本譜 = -1
    QStringList comments;              // ★追加：各手のコメント
};

// ライブ対局の状態を保持する構造体（読み込んだ棋譜データとは分離して管理）
struct LiveGameState {
    int anchorPly = -1;              // 分岐起点の手数（-1 = 初期局面から）
    int parentRowIndex = -1;         // 親行のインデックス（-1 = 本譜）
    QList<KifDisplayItem> moves;     // ライブ対局で追加された手
    QStringList sfens;               // SFEN列
    QVector<ShogiMove> gameMoves;    // 指し手
    bool isActive = false;           // ライブ対局中かどうか

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
