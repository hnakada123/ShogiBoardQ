#ifndef GAMEMODESTRATEGY_H
#define GAMEMODESTRATEGY_H

/// @file gamemodestrategy.h
/// @brief 対局モード別の Strategy インターフェース

#include <QPoint>
#include <QString>

/**
 * @brief 対局モードごとの開始・着手処理を抽象化するインターフェース
 *
 * MatchCoordinator から対局モード固有のロジックを分離するための
 * Strategy パターンの基底クラス。
 */
class GameModeStrategy {
public:
    virtual ~GameModeStrategy() = default;

    /// 対局を開始する
    virtual void start() = 0;

    /// 人間の着手を処理する
    virtual void onHumanMove(const QPoint& from, const QPoint& to,
                             const QString& prettyMove) = 0;

    /// このモードでエンジンが必要か
    virtual bool needsEngine() const = 0;

    /// ターンタイマーを起動する（必要なモードのみオーバーライド）
    virtual void armTurnTimerIfNeeded() {}

    /// ターンタイマーを停止し考慮時間を設定する（必要なモードのみオーバーライド）
    virtual void finishTurnTimerAndSetConsideration(int /*moverPlayer*/) {}

    /// ターンタイマーを解除する（終局時など、計測を破棄する場合）
    virtual void disarmTurnTimer() {}

    /// 初手がエンジン手番なら go を発行する（HvEのみオーバーライド）
    virtual void startInitialMoveIfNeeded() {}
};

#endif // GAMEMODESTRATEGY_H
