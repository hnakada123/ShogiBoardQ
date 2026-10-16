#ifndef BOARDMOVECONTROLLER_H
#define BOARDMOVECONTROLLER_H

/// @file boardmovecontroller.h
/// @brief 盤上で指した手を、動作状態に合わせて処理する（局面編集・棋譜の変化の記録・通信対局・対局）

#include <QObject>
#include <QPoint>

#include "boardinputroute.h"
#include "usimove.h"

class AppActivityState;
class BoardInteractionController;
class BranchTreeEditController;
class HumanTurnPolicy;
class MatchCoordinator;
class PositionEditSession;
class ShogiGameController;
class ShogiView;

/**
 * @brief 主画面の盤で指した手の行き先（BoardInputRoute）。動作状態ごとに受け付けるかと処理を決める
 *
 * つまめるか（acceptsInput）は、局面編集中なら常に、それ以外は HumanTurnPolicy の判定（待機中は変化を
 * 記録できるとき、対局中は人間の手番）で決まる。決まった手は動作状態に合わせて処理する。
 * - 局面編集中: 駒の配置を変える（PositionEditSession）
 * - 待機中: 表示中の手の次の手（変化）として棋譜に記録する（BranchTreeEditController）
 * - 通信対局中: サーバーへ送るよう知らせる（盤は確認を受けてから動かす）
 * - 対局中で人間の手番: 合法手なら指し、対局の司令塔へ渡す
 * どの場合も、手を受け付けたかを盤の操作へ返す（つまんだ駒の表示を戻すため）。
 */
class BoardMoveController : public QObject, public BoardInputRoute
{
    Q_OBJECT

public:
    struct Deps {
        ShogiView* view = nullptr;                          ///< 盤（必須）
        ShogiGameController* gameController = nullptr;      ///< 合法判定と着手（必須）
        BoardInteractionController* board = nullptr;        ///< 盤の操作（必須）
        const AppActivityState* activity = nullptr;         ///< 動作状態（必須）
        MatchCoordinator* match = nullptr;                  ///< 対局の司令塔（必須）
        PositionEditSession* positionEdit = nullptr;        ///< 局面編集（必須）
        BranchTreeEditController* branchEdit = nullptr;     ///< 変化の記録（必須）
        const HumanTurnPolicy* turnPolicy = nullptr;        ///< 人間が指せる手番か（必須）
    };

    /// 盤の操作（deps.board）の行き先になる
    explicit BoardMoveController(const Deps& deps, QObject* parent = nullptr);

    // --- BoardInputRoute ---
    bool acceptsInput() const override;
    bool isEditing() const override;
    void submit(const QPoint& from, const QPoint& to) override;

    /**
     * @brief 盤の座標で手を指す（今の動作状態に合った処理をする）
     * @return 手を受け付けた（通信対局ではサーバーへ送った）
     */
    bool playMove(const QPoint& from, const QPoint& to);
    /**
     * @brief 成り・不成まで決まった手（定跡の手）を、盤で指した手と同じ行き先で指す（局面編集中は指さない）
     * @return 手を受け付けた（通信対局ではサーバーへ送った）
     */
    bool playUsiMove(const UsiMove& move);

signals:
    /// 通信対局で人間が手を選んだ（盤は動かしていない）
    void csaMoveRequested(const UsiMove& move);

private:
    /// 対局で人間の手番か（司令塔が手を受け付けるか）
    bool canPlayInGame() const;
    /// 指した手の結果を盤の操作へ返す（ハイライト・つまんだ駒の表示）
    void reportResult(const UsiMove& move, bool ok);

    Deps m_deps;
};

#endif // BOARDMOVECONTROLLER_H
