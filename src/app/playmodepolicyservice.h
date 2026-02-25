#ifndef PLAYMODEPOLICYSERVICE_H
#define PLAYMODEPOLICYSERVICE_H

/// @file playmodepolicyservice.h
/// @brief プレイモード依存の判定ロジックを集約するサービス

#include "shogigamecontroller.h"

enum class PlayMode;
class MatchCoordinator;
class CsaGameCoordinator;

/**
 * @brief プレイモード依存の判定ロジックを集約する純粋ロジッククラス
 *
 * MainWindow に散在していたプレイモード判定の switch 文を集約し、
 * 単一責務で管理する。QObject 非継承。
 */
class PlayModePolicyService
{
public:
    struct Deps {
        PlayMode* playMode = nullptr;
        ShogiGameController* gameController = nullptr;
        MatchCoordinator* match = nullptr;
        CsaGameCoordinator* csaGameCoordinator = nullptr;
    };

    void updateDeps(const Deps& deps);

    /// 現在の手番が人間かどうかを判定する
    bool isHumanTurnNow() const;

    /// 指定プレイヤーが人間側かどうかを判定する
    bool isHumanSide(ShogiGameController::Player p) const;

    /// 人間 vs 人間モードかどうかを判定する
    bool isHvH() const;

    /// 対局が進行中（終局前）かどうかを判定する
    bool isGameActivelyInProgress() const;

private:
    Deps m_deps;
};

#endif // PLAYMODEPOLICYSERVICE_H
