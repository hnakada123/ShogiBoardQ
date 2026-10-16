#ifndef MAINWINDOWACTIONS_H
#define MAINWINDOWACTIONS_H

/// @file mainwindowactions.h
/// @brief 複数の部品にまたがる利用者の操作（局面編集の開始・新規・終了）

#include <QObject>

class GameSession;
class KifuFileController;
class PositionEditCoordinator;
class QWidget;
class SessionResetCoordinator;
class ShogiGameController;

/**
 * @brief 1つの部品では完結しない利用者の操作をまとめる
 *
 * 部品は MainWindowComposer が起動時に全部作ってから渡す（すべて非所有）。
 */
class MainWindowActions : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        QWidget* window = nullptr;                          ///< 閉じる対象のウィンドウ
        ShogiGameController* gameController = nullptr;      ///< 編集した局面
        GameSession* gameSession = nullptr;
        PositionEditCoordinator* positionEdit = nullptr;
        KifuFileController* kifuFileController = nullptr;  ///< 未保存の棋譜の確認
        SessionResetCoordinator* reset = nullptr;          ///< 編集した局面だけの棋譜にする
    };

    explicit MainWindowActions(const Deps& deps, QObject* parent = nullptr);

public slots:
    /// 局面編集を始める（棋譜は開始局面だけになるので、未保存の棋譜を先に確認する）
    void beginPositionEditing();
    /// 盤の局面を開始局面とする棋譜にする（局面編集の開始・終了）
    void resetRecordForEditedPosition();
    /// 「新規」。未保存の棋譜を確認してから初期状態に戻す
    void resetToInitialState();
    /// 「終了」。ウィンドウを閉じられたらアプリを終了する
    void requestQuit();

private:
    const Deps m_deps;
};

#endif // MAINWINDOWACTIONS_H
