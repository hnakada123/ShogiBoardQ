#ifndef MAINWINDOWLIFECYCLEPIPELINE_H
#define MAINWINDOWLIFECYCLEPIPELINE_H

/// @file mainwindowlifecyclepipeline.h
/// @brief MainWindow の起動/終了フローを集約するパイプライン

#include "mainwindowlifecyclesequence.h"

/**
 * @brief MainWindow の起動/終了フローを一本化するパイプライン
 *
 * 責務:
 * - 起動手順（UI構築→配線→設定復元→コーディネータ初期化）を runStartup() に集約
 * - 終了手順（設定保存→エンジン終了→リソース解放）を runShutdown() に集約
 * - 終了確認と終了要求の判断を担い、画面やサービスの詳細には依存しない
 *
 * 依存順序:
 * - runStartup() は MainWindow コンストラクタから1回だけ呼ぶこと
 * - runShutdown() は closeEvent / requestClose / デストラクタから呼ぶ（二重実行防止付き）
 */
class MainWindowLifecyclePipeline
{
public:
    struct Deps {
        MainWindowStartupSequence::Steps startup;
        MainWindowShutdownSequence::Steps shutdown;
        std::function<bool()> isShuttingDown; ///< 自動化 API 等からの終了要求も参照する
        std::function<bool()> confirmDiscardUnsavedKifu;
        std::function<bool()> confirmCloseJoseki;
        std::function<bool()> closeWindow;
        std::function<void()> quitApplication;
    };

    explicit MainWindowLifecyclePipeline(Deps deps);

    /// 起動手順を一括実行する
    void runStartup();

    /// 終了手順を一括実行する（二重実行防止付き）
    void runShutdown();

    /// 未保存の棋譜・定跡について終了可能か確認する
    [[nodiscard]] bool confirmClose() const;

    /// ウィンドウが閉じられた場合にのみ終了手順とアプリ終了を実行する
    void requestClose();

private:
    Deps m_deps;
    bool m_shutdownDone = false;
};

#endif // MAINWINDOWLIFECYCLEPIPELINE_H
