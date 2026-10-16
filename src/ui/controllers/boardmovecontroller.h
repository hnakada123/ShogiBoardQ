#ifndef BOARDMOVECONTROLLER_H
#define BOARDMOVECONTROLLER_H

/// @file boardmovecontroller.h
/// @brief 盤上で指した手を、動作状態に合わせて処理する（局面編集・棋譜の変化の記録・通信対局・対局）

#include <QObject>
#include <QPoint>

class AppActivityState;
class BoardInteractionController;
class BranchTreeEditController;
class MatchCoordinator;
class PositionEditController;
class ShogiGameController;
class ShogiView;

/**
 * @brief 盤の操作（BoardInteractionController::moveRequested）で決まった手を処理する
 *
 * - 局面編集中: 駒の配置を変える（PositionEditController）
 * - 待機中: 表示中の手の次の手（変化）として棋譜に記録する（BranchTreeEditController）
 * - 通信対局中: サーバーへ送るよう知らせる（盤は確認を受けてから動かす）
 * - 対局中で人間の手番: 合法手なら指し、対局の司令塔へ渡す
 * どの場合も、手を受け付けたかを盤の操作へ返す（つまんだ駒の表示を戻すため）。
 */
class BoardMoveController : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        ShogiView* view = nullptr;                          ///< 盤（必須）
        ShogiGameController* gameController = nullptr;      ///< 合法判定と着手（必須）
        BoardInteractionController* board = nullptr;        ///< 盤の操作（必須）
        const AppActivityState* activity = nullptr;         ///< 動作状態（必須）
        MatchCoordinator* match = nullptr;                  ///< 対局の司令塔（必須）
        PositionEditController* positionEdit = nullptr;     ///< 局面編集（必須）
        BranchTreeEditController* branchEdit = nullptr;     ///< 変化の記録（必須）
    };

    explicit BoardMoveController(const Deps& deps, QObject* parent = nullptr);

    /**
     * @brief 盤の座標で手を指す（今の動作状態に合った処理をする）
     * @return 手を受け付けた（通信対局ではサーバーへ送った）
     */
    bool playMove(const QPoint& from, const QPoint& to);

public slots:
    /// 盤の操作で手が決まった
    void onMoveRequested(const QPoint& from, const QPoint& to);

signals:
    /// 通信対局で人間が手を選んだ（盤は動かしていない）
    void csaMoveRequested(const QPoint& from, const QPoint& to);

private:
    Deps m_deps;
};

#endif // BOARDMOVECONTROLLER_H
