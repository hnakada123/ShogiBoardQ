#ifndef HUMANTURNPOLICY_H
#define HUMANTURNPOLICY_H

/// @file humanturnpolicy.h
/// @brief 盤を人間が操作できるか・対局が進行中かの判定

class AppActivityState;
class BranchTreeEditController;
class CsaGameCoordinator;
class MatchCoordinator;
class ShogiGameController;

/**
 * @brief 盤を人間が操作できるか・対局が進行中かを、アプリの動作状態と対局の司令塔から判定する
 *
 * 動作状態ごとの盤の手の行き先（boardInput）はここだけで決める。ローカル対局は MatchCoordinator
 * （どちらの手番が人間か・手を受け付けるか）、通信対局は CsaGameCoordinator に聞く。QObject 非継承。
 */
class HumanTurnPolicy
{
public:
    struct Deps {
        const AppActivityState* activity = nullptr;
        const ShogiGameController* gameController = nullptr;
        const MatchCoordinator* match = nullptr;
        const CsaGameCoordinator* csaGame = nullptr;     ///< 通信対局の司令塔
        /// 対局していないとき、盤上の着手を棋譜の変化として記録できるか
        const BranchTreeEditController* branchEdit = nullptr;
    };

    /// 盤で指した手の行き先
    enum class BoardInput {
        None,        ///< 指せない（相手の手番・終局後・検討中・終了処理の後など）
        Edit,        ///< 局面編集: 駒の配置を変える
        Variation,   ///< 待機中: 表示中の手の次の手（変化）として棋譜に記録する
        Game,        ///< ローカル対局で人間の手番: 指して対局の司令塔へ渡す
        CsaServer,   ///< 通信対局で自分の手番: サーバーへ送る（盤は確認を受けてから動かす）
    };

    /// 依存が無いものは「操作できない」「対局していない」と答える
    explicit HumanTurnPolicy(const Deps& deps) : m_deps(deps) {}

    /// 今の動作状態で、盤で指した手をどう扱うか
    BoardInput boardInput() const;

    /// 盤を人間が指せる手番か（対局していなければ、変化の入力を受け付けるか。局面編集は含まない）
    bool isHumanTurnNow() const;

    /// 対局が進行中（終局前）か
    bool isGameActivelyInProgress() const;

    /// 終了処理の後は、盤のクリックが届いても何も受け付けない
    void disable() { m_disabled = true; }

private:
    const Deps m_deps;
    bool m_disabled = false;
};

#endif // HUMANTURNPOLICY_H
