#ifndef MAINWINDOWLIFECYCLEPIPELINE_H
#define MAINWINDOWLIFECYCLEPIPELINE_H

/// @file mainwindowlifecyclepipeline.h
/// @brief MainWindow の起動/終了フローを集約するパイプライン

class MainWindow;

/**
 * @brief MainWindow の起動/終了フローを一本化するパイプライン
 *
 * 責務:
 * - 起動手順（UI構築→配線→設定復元→コーディネータ初期化）を runStartup() に集約
 * - 終了手順（設定保存→エンジン終了→リソース解放）を runShutdown() に集約
 *
 * 依存順序:
 * - runStartup() は MainWindow コンストラクタから1回だけ呼ぶこと
 * - runShutdown() は closeEvent / saveSettingsAndClose / デストラクタから呼ぶ（二重実行防止付き）
 */
class MainWindowLifecyclePipeline
{
public:
    explicit MainWindowLifecyclePipeline(MainWindow& mw);

    /// 起動手順を一括実行する
    void runStartup();

    /// 終了手順を一括実行する（二重実行防止付き）
    void runShutdown();

    /// 終了手順を実行してからアプリケーションを終了する
    void shutdownAndQuit();

private:
    MainWindow& m_mw;
    bool m_shutdownDone = false;

    // --- Startup sub-steps ---

    /// 基盤オブジェクト（CompositionRoot/Registry/SignalRouter/CommLogModel）を生成する
    void createFoundationObjects();

    /// UI骨格（ドックネスティング/セントラル/ツールバー）をセットアップする
    void setupUiSkeleton();

    /// コア部品（GC/View/盤モデル）を初期化する
    void initializeCoreComponents();

    /// 早期サービス（PolicyService/TimePresenter/QueryService）を初期化する
    void initializeEarlyServices();

    /// ゲームパネル群を構築する（ServiceRegistry に委譲）
    void buildGamePanels();

    /// ウィンドウ設定を復元しレイアウトを同期する（ServiceRegistry に委譲）
    void restoreWindowAndSync();

    /// シグナル配線とツールチップを設定する
    void connectSignals();

    /// コーディネータの最終初期化と追加機能設定を行う
    void finalizeAndConfigureUi();
};

#endif // MAINWINDOWLIFECYCLEPIPELINE_H
