#ifndef MAINWINDOWCOMPOSER_H
#define MAINWINDOWCOMPOSER_H

/// @file mainwindowcomposer.h
/// @brief MainWindow の部品を依存順に組み立てる合成ルート

#include <QObject>

#include "appstate.h"
#include "mainwindowcomponents.h"
#include "matchcoordinator.h"

class MainWindow;
class QString;
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief 部品の生成・依存の注入・起動時の初期表示を一か所で行う
 *
 * 部品は compose() で全部作る。遅延生成（ensure*）も、作られていない部品を後から探す
 * ゲッターもない。依存は「全部作ってから結ぶ」ので、相互に参照し合う部品も
 * 作成順を気にせず結べる。状態は AppState に1つだけ置き、部品はそこへのポインタを受け取る。
 *
 * 実装は次のファイルに分けている:
 * - mainwindowcomposer.cpp          起動・終了の手順と終了確認
 * - mainwindowcomposer_create.cpp   部品の生成（モデル → 画面 → 盤 → サービス）
 * - mainwindowcomposer_wire.cpp     棋譜・UI 系の依存の注入
 * - mainwindowcomposer_game.cpp     対局系の依存の注入と司令塔のフック
 * - mainwindowcomposer_finalize.cpp シグナル接続と初期表示
 * - mainwindowcomposer_automation.cpp 自動化 API（--automation）
 */
class MainWindowComposer : public QObject
{
    Q_OBJECT

public:
    MainWindowComposer(MainWindow& window, Ui::MainWindow& ui);
    ~MainWindowComposer() override;

    /// 起動手順を一括実行する（MainWindow コンストラクタから1回だけ呼ぶ）
    void compose();

    /// 終了手順（設定保存・エンジン終了）。2回目以降は何もしない
    void shutdown();

    /// 未保存の棋譜・定跡について閉じてよいか確認する
    [[nodiscard]] bool confirmClose();

    /// 自動化 API のサーバーを作って待ち受けを始める（1回だけ）
    void startAutomationServer(const QString& socketPath);

    AppState& state() { return m_state; }
    const AppState& state() const { return m_state; }
    MainWindowComponents& parts() { return m_c; }
    const MainWindowComponents& parts() const { return m_c; }

private:
    // ===== 生成（mainwindowcomposer_create.cpp） =====
    void createModels();
    void setupUiSkeleton();
    void createBoardAndGameController();
    void createPanelsAndDocks();
    void createBranchNavigation();
    void createRecordAndPlayerInfo();
    void createSecondaryDocks();
    void createServices();

    // ===== 依存の注入（mainwindowcomposer_wire.cpp） =====
    void wireFoundation();
    void wireKifu();
    void wireUi();
    void wireDialogs();

    // ===== 対局系（mainwindowcomposer_game.cpp） =====
    void wireGame();
    void wireMatchCoordinator();
    void wireCsaGame();
    MatchCoordinator::Hooks buildMatchHooks();
    MatchCoordinator::UndoHooks buildUndoHooks();

    // ===== 接続と初期表示（mainwindowcomposer_finalize.cpp） =====
    void connectSignals();
    void finalizeUi();

    MainWindow& m_window;
    Ui::MainWindow& m_ui;
    AppState m_state;
    MainWindowComponents m_c;
    bool m_shutdownDone = false;
};

#endif // MAINWINDOWCOMPOSER_H
