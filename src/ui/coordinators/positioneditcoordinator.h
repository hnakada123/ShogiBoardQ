#ifndef POSITIONEDITCOORDINATOR_H
#define POSITIONEDITCOORDINATOR_H

/// @file positioneditcoordinator.h
/// @brief 局面編集の開始・終了（盤の編集モード・動作状態・「編集終了」ボタン・編集メニュー）

#include <QObject>

class AppActivityState;
class BoardInteractionController;
class KifuNavigationState;
class PositionEditController;
class QAction;
class ShogiGameController;
class ShogiView;

/**
 * @brief 局面編集を始めて終える流れを受け持つ
 *
 * 盤の編集そのものは PositionEditController、棋譜を編集した局面だけにするのは受け手
 * （positionEditingStarted / positionEditingFinished を受けた MainWindowActions）が行う。
 */
class PositionEditCoordinator : public QObject
{
    Q_OBJECT

public:
    /// 編集メニューの項目（編集中だけ使う）
    struct EditActions {
        QAction* returnAllPiecesToStand = nullptr;
        QAction* setHiratePosition = nullptr;
        QAction* setTsumePosition = nullptr;
        QAction* changeTurn = nullptr;
    };

    struct Deps {
        ShogiView* view = nullptr;                          ///< 盤（必須）
        ShogiGameController* gameController = nullptr;      ///< 盤の局面と手番（必須）
        BoardInteractionController* boardController = nullptr; ///< 盤の操作モード
        PositionEditController* posEdit = nullptr;          ///< 盤の編集（必須）
        const KifuNavigationState* navState = nullptr;      ///< 表示中の手（編集する局面。必須）
        AppActivityState* activity = nullptr;               ///< 動作状態（必須）
        EditActions actions;
    };

    explicit PositionEditCoordinator(const Deps& deps, QObject* parent = nullptr);

public slots:
    /// 表示中の局面で局面編集を始める
    void beginPositionEditing();
    /// 局面編集を終える
    void finishPositionEditing();

signals:
    void positionEditingStarted();
    void positionEditingFinished();

private:
    Deps m_deps;
};

#endif // POSITIONEDITCOORDINATOR_H
