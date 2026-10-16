#ifndef GAMECHANNEL_H
#define GAMECHANNEL_H

/// @file gamechannel.h
/// @brief 対局相手への窓口（ローカル対局と通信対局で同じ操作）

#include <optional>

#include "matchplayers.h"
#include "nyugyokujudgement.h"
#include "shogitypes.h"
#include "timecontrol.h"

class MatchCoordinator;
class ShogiClock;

/**
 * @brief 対局中の利用者の操作の送り先
 *
 * ローカル対局（MatchCoordinator）と通信対局（CsaGameCoordinator）の違いはこの型の実装に閉じ込め、
 * 投了・中断・即指し・入玉宣言を受ける側（GameSession・NyugyokuDeclarationHandler）は対局の種類で分岐しない。
 */
class GameChannel
{
public:
    virtual ~GameChannel() = default;

    virtual void resign() = 0;
    virtual void breakOff() = 0;
    /// 手番のエンジンにすぐ指させる
    virtual void moveNow() = 0;

    /// 人間が指す手番か（入玉宣言・投了を人間が行える）
    virtual bool isHumanToMove() const = 0;
    /// 手番
    virtual Turn sideToMove() const = 0;
    /// 入玉宣言の成否をこちらで盤面から判定するか（通信対局はサーバーが判定する）
    virtual bool judgesDeclaration() const = 0;
    /**
     * @brief 手番の側が入玉宣言する
     * @return こちらで判定したときはその結果（終局済み）。サーバーが判定するときは nullopt
     */
    virtual std::optional<NyugyokuJudgement::Result> declareNyugyoku() = 0;

    /// 対局者
    virtual MatchPlayers players() const = 0;
    /// 持ち時間の決まり
    virtual TimeControl timeControl() const = 0;
};

/// ローカル対局（人間・エンジン）の窓口
class LocalGameChannel final : public GameChannel
{
public:
    LocalGameChannel(MatchCoordinator* match, const ShogiClock* clock);

    void resign() override;
    void breakOff() override;
    void moveNow() override;
    bool isHumanToMove() const override;
    Turn sideToMove() const override;
    bool judgesDeclaration() const override { return true; }
    std::optional<NyugyokuJudgement::Result> declareNyugyoku() override;
    MatchPlayers players() const override;
    TimeControl timeControl() const override;

private:
    MatchCoordinator* m_match = nullptr;   ///< 非所有
    const ShogiClock* m_clock = nullptr;   ///< 非所有
};

#endif // GAMECHANNEL_H
