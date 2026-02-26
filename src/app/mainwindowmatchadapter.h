#ifndef MAINWINDOWMATCHADAPTER_H
#define MAINWINDOWMATCHADAPTER_H

/// @file mainwindowmatchadapter.h
/// @brief MatchCoordinator から MainWindow への callback 群を集約するアダプタ

#include <QPoint>
#include <QString>
#include <functional>

class ShogiView;
class ShogiGameController;
class BoardInteractionController;
class DialogCoordinator;
class TurnStateSyncService;
class PlayerInfoWiring;
enum class PlayMode;

/**
 * @brief MatchCoordinator の callback/hook 群を MainWindow から分離するアダプタ
 *
 * 責務:
 * - 新規対局 UI 初期化（initializeNewGameHook）
 * - GC 盤面描画（renderBoardFromGc）
 * - 着手ハイライト表示（showMoveHighlights）
 * - 終局メッセージ表示（showGameOverMessageBox）
 * - 手番/持ち時間表示更新（updateTurnAndTimekeepingDisplay）
 *
 * QObject 非継承の純粋ロジッククラス。
 */
class MainWindowMatchAdapter
{
public:
    struct Deps {
        // --- renderBoardFromGc / initializeNewGameHook 用 ---
        ShogiView* shogiView = nullptr;
        ShogiGameController* gameController = nullptr;

        // --- showMoveHighlights 用 ---
        BoardInteractionController* boardController = nullptr;

        // --- initializeNewGameHook 用 ---
        PlayMode* playMode = nullptr;
        std::function<void(QString&)> initializeNewGame;
        std::function<PlayerInfoWiring*()> ensureAndGetPlayerInfoWiring;

        // --- showGameOverMessageBox 用 ---
        std::function<DialogCoordinator*()> ensureAndGetDialogCoordinator;

        // --- updateTurnAndTimekeepingDisplay 用 ---
        std::function<TurnStateSyncService*()> ensureAndGetTurnStateSync;
    };

    void updateDeps(const Deps& deps);

    /// 新規対局初期化のフックポイント（GameStartCoordinator 用コールバック）
    void initializeNewGameHook(const QString& s);

    /// GC の盤面状態を ShogiView へ反映する（MatchCoordinator フック用）
    void renderBoardFromGc();

    /// 指し手のハイライトを盤面に表示する
    void showMoveHighlights(const QPoint& from, const QPoint& to);

    /// 対局終了メッセージボックスを表示する
    void showGameOverMessageBox(const QString& title, const QString& message);

    /// 手番と持ち時間の表示を更新する
    void updateTurnAndTimekeepingDisplay();

private:
    Deps m_deps;
};

#endif // MAINWINDOWMATCHADAPTER_H
