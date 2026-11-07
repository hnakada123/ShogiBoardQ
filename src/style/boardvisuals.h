#ifndef BOARDVISUALS_H
#define BOARDVISUALS_H

#include <algorithm>

/// 配色とは独立した、すべての盤面に共通する質感の設定。
struct BoardVisuals {
    bool woodGrain = true;
    bool pieceShadow = true;
    int pieceScale = 108;
    bool standWoodGrain = true;

    /// 駒の大きさの範囲（%。normalized と外観ダイアログの欄で共通）
    static constexpr int kMinPieceScale = 90;
    static constexpr int kMaxPieceScale = 112;

    BoardVisuals normalized() const
    {
        return {woodGrain, pieceShadow, std::clamp(pieceScale, kMinPieceScale, kMaxPieceScale), standWoodGrain};
    }
    bool operator==(const BoardVisuals& other) const
    {
        return woodGrain == other.woodGrain && pieceShadow == other.pieceShadow
            && pieceScale == other.pieceScale && standWoodGrain == other.standWoodGrain;
    }
    bool operator!=(const BoardVisuals& other) const { return !(*this == other); }
};

#endif // BOARDVISUALS_H
