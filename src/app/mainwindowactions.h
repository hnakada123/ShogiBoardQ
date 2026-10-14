#ifndef MAINWINDOWACTIONS_H
#define MAINWINDOWACTIONS_H

#include "mainwindowresetservice.h"
#include "playmode.h"
#include "shogimove.h"
#include <QPoint>
#include <QList>
#include <functional>

class BoardSetupController;
class BranchNavigationWiring;
class GameRecordUpdateService;
class GameSessionOrchestrator;
class PositionEditCoordinator;
class CommentCoordinator;

/// 起動後の利用者操作。MainWindow やレジストリの内部状態へのアクセスを持たない。
class MainWindowActions {
public:
    struct Deps {
        BoardSetupController* boardSetup = nullptr;
        BoardInteractionController* boardController = nullptr;
        BranchNavigationWiring* branchWiring = nullptr;
        ShogiGameController* gameController = nullptr;
        ShogiView* view = nullptr;
        UiStatePolicyManager* uiPolicy = nullptr;
        GameRecordUpdateService* recordUpdates = nullptr;
        GameSessionOrchestrator* gameSession = nullptr;
        PositionEditCoordinator* positionEdit = nullptr;
        CommentCoordinator* comments = nullptr;
        PlayMode* playMode = nullptr;
        bool* rowGuard = nullptr;
        QString* startSfen = nullptr;
        QString* currentSfen = nullptr;
        QList<ShogiMove>* gameMoves = nullptr;
        MainWindowResetService::ModelResetDeps editedPositionReset;
        std::function<bool()> confirmDiscardUnsavedKifu;
        std::function<void()> updateJosekiWindow;
    };
    explicit MainWindowActions(Deps deps);
    void handleMoveRequested(const QPoint& from, const QPoint& to);
    bool canRecordVariationMove() const;
    bool recordVariationMove(QPoint& from, QPoint& to);
    void handleMoveCommitted(int mover, int ply);
    void handleBeginPositionEditing();
    void handleFinishPositionEditing();
    void resetRecordForEditedPosition();
private:
    const Deps m_deps;
};
#endif
