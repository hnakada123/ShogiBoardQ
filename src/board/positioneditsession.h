#ifndef POSITIONEDITSESSION_H
#define POSITIONEDITSESSION_H

/// @file positioneditsession.h
/// @brief 局面編集（開始・終了・編集メニューの操作・編集中の着手）

#include <QObject>
#include <QPoint>
#include <QString>

#include "appactivitystate.h"
#include "shogitypes.h"

class BoardInteractionController;
class KifuNavigationState;
class ShogiView;

/**
 * @brief 局面編集を1か所で受け持つ
 *
 * 「編集中か」は動作状態（AppActivityState::DuringPositionEdit）のリースを持っているかだけで決まり、盤の描き方
 * （ShogiView::setPositionEditMode）と「編集終了」ボタンはこのクラスがそれに合わせて切り替える。
 * 編集は待機中だけ始める。ほかの部品が別の動作を始めて動作状態を取り上げたら、編集中の局面で編集を終える。
 * 編集を始めると表示中の局面を、終えると編集した局面を知らせる（受け手が棋譜をその局面だけにする）。
 */
class PositionEditSession : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        ShogiView* view = nullptr;                          ///< 盤（必須）
        BoardInteractionController* board = nullptr;        ///< 盤の操作（必須）
        const KifuNavigationState* navState = nullptr;      ///< 表示中の手（編集する局面。必須）
        AppActivityState* activity = nullptr;               ///< 動作状態（必須）
    };

    explicit PositionEditSession(const Deps& deps, QObject* parent = nullptr);

    bool isEditing() const;
    /// 表示中の手から移ってよいか（局面編集中は棋譜の手を移らない。KifuNavigationController の移動前の確認）
    bool allowsLeavingMove() const { return !isEditing(); }
    /// 編集中の着手（盤の操作で決まった駒の移動）を盤に反映する。駒箱の駒は pieceBoxSide() の駒として置く。
    /// 動かせたら true
    bool applyEditMove(const QPoint& from, const QPoint& to);
    /// 駒箱の駒をどちらの駒として置くか（正本。盤は表示するだけ）
    Turn pieceBoxSide() const { return m_pieceBoxSide; }

public slots:
    /// 表示中の局面で局面編集を始める
    void begin();
    /// 局面編集を終える
    void finish();

    // --- 編集メニュー ---
    void returnAllPiecesToBox();
    void setHiratePosition();
    void setTsumePosition();
    void toggleSideToMove();
    /// 駒箱の駒をどちらの駒として置くか（盤の駒箱の先後の切り替え）
    void setPieceBoxSide(Turn side);

private slots:
    /// ほかの部品が別の動作を始めた
    void onActivityRevoked(QObject* owner);

signals:
    /// 編集を始めた（sfen は編集する局面。手数は1）
    void editingStarted(const QString& sfen);
    /// 編集を終えた（sfen は編集した局面。新しい開始局面になる）
    void editingFinished(const QString& sfen);

private:
    /// つまんでいる駒とハイライトを消す
    void resetInteraction();
    /// 編集中の局面を新しい開始局面にして、盤を編集の表示から戻す
    void endEditing();

    Deps m_deps;
    ActivityLease m_activityLease;   ///< 局面編集中の動作状態
    Turn m_pieceBoxSide = Turn::Black;   ///< 駒箱の駒を置く側（編集をやめても次の編集に引き継ぐ）
};

#endif // POSITIONEDITSESSION_H
