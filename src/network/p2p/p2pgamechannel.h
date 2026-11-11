#ifndef P2PGAMECHANNEL_H
#define P2PGAMECHANNEL_H

/// @file p2pgamechannel.h
/// @brief P2P対局の窓口（盤の手は相手の承認を受けてから盤に指す。入玉宣言・即指しはない）

#include "gamechannel.h"

class P2pGameCoordinator;

/**
 * @brief P2P対局の窓口
 *
 * 盤で選んだ手は司令塔へ渡し、送った時点では盤を動かさない（MoveResult::Sent。相手の承認を受けてから盤に指す）。
 * 「中断」は合意中断の申し出にする（一方的にやめるのは P2P対局のウィンドウの「対局をやめる」）。
 * v1 は入玉宣言を扱わない（実装仕様 1.1節）。
 */
class P2pGameChannel final : public GameChannel
{
public:
    explicit P2pGameChannel(P2pGameCoordinator* coordinator);

    MoveResult playMove(const UsiMove& move) override;
    void resign() override;
    void breakOff() override;
    void moveNow() override {}
    bool isInProgress() const override;
    bool isHumanToMove() const override;
    bool canResign() const override;
    Turn sideToMove() const override;
    bool acceptsDeclaration() const override { return false; }
    std::optional<NyugyokuJudgement::Result> declareNyugyoku() override { return std::nullopt; }

private:
    P2pGameCoordinator* m_coordinator = nullptr;   ///< 非所有
};

#endif // P2PGAMECHANNEL_H
