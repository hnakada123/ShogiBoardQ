#ifndef MAINWINDOWUIBOOTSTRAPPER_H
#define MAINWINDOWUIBOOTSTRAPPER_H

/// @file mainwindowuibootstrapper.h
/// @brief MainWindow の UI ブート手順（パネル構築・依存順序制御）をカプセル化する

class MainWindow;

/**
 * @brief MainWindow の UI 初期化手順を依存順に実行するブートストラッパー
 *
 * コンストラクタで呼び出される以下の初期化フェーズを管理する:
 *
 * 1. buildGamePanels() — ゲームパネル群の構築（依存順序あり）
 * 2. restoreWindowAndSync() — ウィンドウ設定復元とレイアウト同期
 * 3. finalizeCoordinators() — コーディネーター・プレゼンターの最終初期化
 *
 * 依存順序:
 * - buildGamePanels は initializeComponents() 完了後に呼ぶこと
 * - restoreWindowAndSync は buildGamePanels 完了後に呼ぶこと
 * - finalizeCoordinators は connectCoreSignals 完了後に呼ぶこと
 */
class MainWindowUiBootstrapper
{
public:
    explicit MainWindowUiBootstrapper(MainWindow& mw);

    /// ゲーム表示パネル群を依存順に構築する
    void buildGamePanels();

    /// ウィンドウ設定を復元しレイアウトを同期する
    void restoreWindowAndSync();

    /// コーディネーターの最終初期化を行う
    void finalizeCoordinators();

private:
    MainWindow& m_mw;
};

#endif // MAINWINDOWUIBOOTSTRAPPER_H
