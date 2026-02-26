#ifndef MATCHRUNTIMEQUERYSERVICE_H
#define MATCHRUNTIMEQUERYSERVICE_H

/// @file matchruntimequeryservice.h
/// @brief 対局実行時の判定・取得クエリを集約するサービス

#include "shogigamecontroller.h"
#include "matchcoordinator.h"

class PlayModePolicyService;
class TimeControlController;

/**
 * @brief 対局実行時の判定・取得クエリを集約する純粋ロジッククラス
 *
 * MainWindow に散在していた対局中判定・手番判定・時間取得・SFEN アクセサを
 * 単一サービスに集約する。QObject 非継承。
 *
 * match はダブルポインタで保持し、対局開始/終了による値変化に追従する。
 */
class MatchRuntimeQueryService
{
public:
    struct Deps {
        PlayModePolicyService* playModePolicy = nullptr;  ///< プレイモード判定（安定ポインタ）
        TimeControlController* timeController = nullptr;  ///< 時間制御（安定ポインタ）
        MatchCoordinator** match = nullptr;               ///< 対局司令塔（ダブルポインタ）
    };

    void updateDeps(const Deps& deps);

    // --- 手番・対局判定 ---

    /// 現在の手番が人間かどうかを判定する
    bool isHumanTurnNow() const;

    /// 対局が進行中（終局前）かどうかを判定する
    bool isGameActivelyInProgress() const;

    /// 人間 vs 人間モードかどうかを判定する
    bool isHvH() const;

    /// 指定プレイヤーが人間側かどうかを判定する
    bool isHumanSide(ShogiGameController::Player p) const;

    // --- 時間取得 ---

    /// 指定プレイヤーの残り時間を取得する（ms）
    qint64 getRemainingMsFor(MatchCoordinator::Player p) const;

    /// 指定プレイヤーの加算時間を取得する（ms）
    qint64 getIncrementMsFor(MatchCoordinator::Player p) const;

    /// 秒読み時間を取得する（ms）
    qint64 getByoyomiMs() const;

    // --- SFEN アクセサ ---

    /// MatchCoordinatorが保持するSFEN履歴への参照を取得する（未初期化時はnullptr）
    QStringList* sfenRecord();

    /// MatchCoordinatorが保持するSFEN履歴への参照を取得する（const版）
    const QStringList* sfenRecord() const;

private:
    Deps m_deps;
};

#endif // MATCHRUNTIMEQUERYSERVICE_H
