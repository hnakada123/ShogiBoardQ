#ifndef CSAGAMECHANNEL_H
#define CSAGAMECHANNEL_H

/// @file csagamechannel.h
/// @brief 通信対局の窓口（操作はサーバーへ送り、入玉宣言の成否はサーバーが判定する）

#include "gamechannel.h"

class CsaGameCoordinator;

class CsaGameChannel final : public GameChannel
{
public:
    explicit CsaGameChannel(CsaGameCoordinator* coordinator);

    MoveResult playMove(const UsiMove& move) override;
    void resign() override;
    void breakOff() override;
    void moveNow() override;
    bool isInProgress() const override;
    bool isHumanToMove() const override;
    bool canResign() const override;
    Turn sideToMove() const override;
    bool acceptsDeclaration() const override { return true; }
    std::optional<NyugyokuJudgement::Result> declareNyugyoku() override;

private:
    CsaGameCoordinator* m_coordinator = nullptr;   ///< 非所有
};

#endif // CSAGAMECHANNEL_H
