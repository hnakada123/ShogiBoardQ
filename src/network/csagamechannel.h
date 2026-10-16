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

    void resign() override;
    void breakOff() override;
    void moveNow() override;
    bool isHumanToMove() const override;
    Turn sideToMove() const override;
    bool judgesDeclaration() const override { return false; }
    std::optional<NyugyokuJudgement::Result> declareNyugyoku() override;
    /// サーバーが知らせた対局者名（エンジンで指していても対局者名で記録する）
    MatchPlayers players() const override;
    /// サーバーの対局条件の持ち時間
    TimeControl timeControl() const override;

private:
    CsaGameCoordinator* m_coordinator = nullptr;   ///< 非所有
};

#endif // CSAGAMECHANNEL_H
