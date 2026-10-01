#ifndef BOARDVISUALS_H
#define BOARDVISUALS_H

#include <algorithm>

/// 配色とは独立した、すべての盤面に共通する質感の設定。
struct BoardVisuals {
    bool woodGrain = true;
    bool pieceShadow = true;
    int pieceScale = 108;
    bool standWoodGrain = true;

    BoardVisuals normalized() const
    {
        return {woodGrain, pieceShadow, std::clamp(pieceScale, 90, 112), standWoodGrain};
    }
    bool operator==(const BoardVisuals& other) const
    {
        return woodGrain == other.woodGrain && pieceShadow == other.pieceShadow
            && pieceScale == other.pieceScale && standWoodGrain == other.standWoodGrain;
    }
    bool operator!=(const BoardVisuals& other) const { return !(*this == other); }
};

#endif // BOARDVISUALS_H
