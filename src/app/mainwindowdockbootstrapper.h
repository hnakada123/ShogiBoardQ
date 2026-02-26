#ifndef MAINWINDOWDOCKBOOTSTRAPPER_H
#define MAINWINDOWDOCKBOOTSTRAPPER_H

/// @file mainwindowdockbootstrapper.h
/// @brief ドック生成・パネル構築・タブ設定の責務を集約するブートストラッパー

class MainWindow;

/**
 * @brief ドック生成・パネル構築の責務を MainWindow から分離するブートストラッパー
 *
 * MainWindowUiBootstrapper から呼び出され、以下を担当する:
 * - RecordPane のセットアップ
 * - EngineAnalysisTab の構築・シグナル接続・依存設定
 * - 各種 QDockWidget の生成（EvalChart, RecordPane, Analysis, Menu, Joseki, AnalysisResults）
 * - 分岐ナビゲーションクラスの初期化
 * - 定跡ウィンドウの更新
 */
class MainWindowDockBootstrapper
{
public:
    explicit MainWindowDockBootstrapper(MainWindow& mw);

    /// 棋譜欄ウィジェットをセットアップする
    void setupRecordPane();

    /// エンジン解析タブをセットアップする
    void setupEngineAnalysisTab();

    /// 評価値グラフのQDockWidgetを作成する
    void createEvalChartDock();

    /// 棋譜欄のQDockWidgetを作成する
    void createRecordPaneDock();

    /// 解析関連のQDockWidget群を作成する
    void createAnalysisDocks();

    /// メニューウィンドウのQDockWidgetを作成する
    void createMenuWindowDock();

    /// 定跡ウィンドウのQDockWidgetを作成する
    void createJosekiWindowDock();

    /// 棋譜解析結果のQDockWidgetを作成する
    void createAnalysisResultsDock();

    /// 分岐ナビゲーション関連クラスを初期化する
    void initializeBranchNavigationClasses();

private:
    /// エンジン解析タブの依存コンポーネントを設定する
    void configureAnalysisTabDependencies();

    MainWindow& m_mw;
};

#endif // MAINWINDOWDOCKBOOTSTRAPPER_H
