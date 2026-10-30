#ifndef BOARDMOVECONTROLLER_H
#define BOARDMOVECONTROLLER_H

/// @file boardmovecontroller.h
/// @brief 盤上で指した手を、動作状態に合わせて処理する（局面編集・棋譜の変化の記録・通信対局・対局）

#include <QObject>
#include <QPoint>

#include "boardinputroute.h"
#include "humanturnpolicy.h"
#include "usimove.h"

class AppActivityState;
class BoardInteractionController;
class BranchTreeEditController;
class MatchCoordinator;
class PositionEditSession;
class GameBoard;
class ShogiView;

/**
 * @brief 主画面の盤で指した手の行き先（BoardInputRoute）
 *
 * 受け付けるかと行き先は HumanTurnPolicy::boardInput() の1か所で決まる。このクラスは行き先ごとに処理する。
 * - 局面編集: 駒の配置を変える（PositionEditSession）
 * - 変化: 表示中の手の次の手として棋譜に記録する（BranchTreeEditController）
 * - 通信対局: サーバーへ送るよう知らせる（盤は確認を受けてから動かす）
 * - 対局: 合法手なら指し、対局の司令塔へ渡す
 * 局面編集のほかは、クリックの移動元・移動先を UsiMove にしてから同じ処理に渡す。
 * どの場合も、手を受け付けたかを盤の操作へ返す（つまんだ駒の表示を戻すため）。
 */
class BoardMoveController : public QObject, public BoardInputRoute
{
    Q_OBJECT

public:
    struct Deps {
        ShogiView* view = nullptr;                          ///< 盤（必須）
        GameBoard* gameBoard = nullptr;                     ///< 合法判定と着手（必須）
        BoardInteractionController* board = nullptr;        ///< 盤の操作（必須）
        const AppActivityState* activity = nullptr;         ///< 動作状態（必須）
        MatchCoordinator* match = nullptr;                  ///< 対局の司令塔（必須）
        PositionEditSession* positionEdit = nullptr;        ///< 局面編集（必須）
        BranchTreeEditController* branchEdit = nullptr;     ///< 変化の記録（必須）
        const HumanTurnPolicy* turnPolicy = nullptr;        ///< 盤の手の行き先（必須）
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
    using Input = HumanTurnPolicy::BoardInput;
    /// 指した手の結果を盤の操作へ返す（ハイライト・つまんだ駒の表示）
    void reportResult(const UsiMove& move, bool ok);

    Deps m_deps;
};

#endif // BOARDMOVECONTROLLER_H
