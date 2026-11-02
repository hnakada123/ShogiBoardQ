#ifndef GAMECHANNEL_H
#define GAMECHANNEL_H

/// @file gamechannel.h
/// @brief 対局相手への窓口（ローカル対局と通信対局で同じ操作）

#include <optional>

#include "nyugyokujudgement.h"
#include "shogitypes.h"
#include "usimove.h"

class MatchCoordinator;

/**
 * @brief 対局中の利用者の操作の送り先
 *
 * ローカル対局（MatchCoordinator）と通信対局（CsaGameCoordinator）の違いはこの型の実装に閉じ込め、
 * 盤の手・投了・中断・即指し・入玉宣言を受ける側（BoardMoveController・GameSession・NyugyokuDeclarationHandler）は
 * 対局の種類で分岐しない。
 */
class GameChannel
{
public:
    /// 盤で選んだ手を渡した結果
    enum class MoveResult {
        Rejected,   ///< 指せない（人間の手番でない・合法手でない・終局した）
        Played,     ///< 盤で指した（ローカル対局）
        Sent,       ///< サーバーへ送った（盤はサーバーの確認を受けてから動かす。通信対局）
    };

    virtual ~GameChannel() = default;

    /// 手番の人間が盤で選んだ手を指す（合法手かはここで確かめる）
    virtual MoveResult playMove(const UsiMove& move) = 0;

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
};

/// ローカル対局（人間・エンジン）の窓口
class LocalGameChannel final : public GameChannel
{
public:
    explicit LocalGameChannel(MatchCoordinator* match);

    MoveResult playMove(const UsiMove& move) override;
    void resign() override;
    void breakOff() override;
    void moveNow() override;
    bool isHumanToMove() const override;
    Turn sideToMove() const override;
    bool judgesDeclaration() const override { return true; }
    std::optional<NyugyokuJudgement::Result> declareNyugyoku() override;

private:
    MatchCoordinator* m_match = nullptr;   ///< 非所有
};

#endif // GAMECHANNEL_H
