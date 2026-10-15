#ifndef MAINWINDOWACTIONS_H
#define MAINWINDOWACTIONS_H

/// @file mainwindowactions.h
/// @brief 起動後の利用者操作（メニュー・盤・棋譜欄から呼ばれる）

#include <QObject>
#include <QPoint>

#include "mainwindowresetservice.h"
#include "shogitypes.h"

struct AppState;
class BoardInteractionController;
class BoardSetupController;
class BranchNavigationWiring;
class CommentCoordinator;
class GameRecordUpdateService;
class GameSessionOrchestrator;
class JosekiWindowWiring;
class KifuDisplayCoordinator;
class KifuFileController;
class PositionEditCoordinator;
class QWidget;
class RecordNavigationHandler;
class SessionLifecycleCoordinator;
class ShogiGameController;
class ShogiView;
class UiStatePolicyManager;
class UndoFlowService;

/**
 * @brief メニュー・盤・棋譜欄の操作を、担当の部品へ振り分ける
 *
 * 部品は MainWindowComposer が起動時に全部作ってから渡す（すべて非所有）。
 */
class MainWindowActions : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* window = nullptr;                          ///< 閉じる対象のウィンドウ
        BoardSetupController* boardSetup = nullptr;
        BoardInteractionController* boardController = nullptr;
        BranchNavigationWiring* branchWiring = nullptr;
        ShogiGameController* gameController = nullptr;
        ShogiView* view = nullptr;
        UiStatePolicyManager* uiPolicy = nullptr;
        GameRecordUpdateService* recordUpdates = nullptr;
        GameSessionOrchestrator* gameSession = nullptr;
        SessionLifecycleCoordinator* sessionLifecycle = nullptr;
        PositionEditCoordinator* positionEdit = nullptr;
        CommentCoordinator* comments = nullptr;
        KifuDisplayCoordinator* displayCoordinator = nullptr;
        RecordNavigationHandler* recordNavigation = nullptr;
        UndoFlowService* undoFlow = nullptr;
        KifuFileController* kifuFileController = nullptr;  ///< 未保存の棋譜の確認
        JosekiWindowWiring* josekiWiring = nullptr;
        AppState* state = nullptr;
        MainWindowResetService::ModelResetDeps editedPositionReset;
    };

    explicit MainWindowActions(Deps deps, QObject* parent = nullptr);

    /// 対局・解析などをしていない待機中で、盤上の着手を棋譜の変化として記録できるか
    bool canRecordVariationMove() const;
    /// 盤上の着手を、表示中の手の次の手（変化）として記録する
    bool recordVariationMove(QPoint& from, QPoint& to);

public slots:
    void handleMoveRequested(const QPoint& from, const QPoint& to);
    void handleMoveCommitted(Turn mover, int ply);
    void onRecordPaneMainRowChanged(int row);
    void beginPositionEditing();
    void finishPositionEditing();
    void resetRecordForEditedPosition();
    void undoLastTwoMoves();
    /// 「新規」。未保存の棋譜を確認してから初期状態に戻す
    void resetToInitialState();
    /// 「終了」。ウィンドウを閉じられたらアプリを終了する
    void requestQuit();

private:
    const Deps m_deps;
};

#endif // MAINWINDOWACTIONS_H
