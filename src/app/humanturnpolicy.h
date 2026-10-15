#ifndef HUMANTURNPOLICY_H
#define HUMANTURNPOLICY_H

/// @file humanturnpolicy.h
/// @brief 盤を人間が操作できるか・対局が進行中かの判定

#include <functional>

class AppActivityState;
class CsaGameWiring;
class MatchCoordinator;
class ShogiGameController;

/**
 * @brief 盤を人間が操作できるか・対局が進行中かを、アプリの動作状態と対局の司令塔から判定する
 *
 * ローカル対局は MatchCoordinator（どちらの手番が人間か・終局したか）、通信対局は
 * CsaGameCoordinator に聞く。QObject 非継承。
 */
class HumanTurnPolicy
{
public:
    struct Deps {
        const AppActivityState* activity = nullptr;
        const ShogiGameController* gameController = nullptr;
        const MatchCoordinator* match = nullptr;
        /// 通信対局の配線（司令塔は最初の CSA 対局で作られるので、呼ばれた時点のものを使う）
        const CsaGameWiring* csaGameWiring = nullptr;
        /// 対局していないとき、盤上の着手を棋譜の変化として受け付けるか
        std::function<bool()> canRecordVariationMove;
    };

    void updateDeps(const Deps& deps);

    /// 盤を人間が操作できる手番か（対局していなければ、変化の入力を受け付けるか）
    bool isHumanTurnNow() const;

    /// 対局が進行中（終局前）か
    bool isGameActivelyInProgress() const;

private:
    Deps m_deps;
};

#endif // HUMANTURNPOLICY_H
