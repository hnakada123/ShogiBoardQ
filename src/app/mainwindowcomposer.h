#ifndef MAINWINDOWCOMPOSER_H
#define MAINWINDOWCOMPOSER_H

/// @file mainwindowcomposer.h
/// @brief MainWindow の部品を依存順に組み立てる合成ルート

#include <QObject>

#include "appstate.h"
#include "mainwindowcomponents.h"

class MainWindow;
class QString;
QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

/**
 * @brief 部品の生成・依存の注入・起動時の初期表示を一か所で行う
 *
 * 部品は compose() で全部作る。遅延生成も、作られていない部品を後から探すゲッターもない。
 * 部品は依存する部品が先にできている順に作り、依存はコンストラクタの Deps で渡す
 * （作った後に set* で注入しない）。部品どうしのシグナルは全部作ってからつなぐ。
 * 状態は AppState に1つだけ置き、部品はそこへのポインタを受け取る。
 *
 * 実装は次のファイルに分けている:
 * - mainwindowcomposer.cpp            起動・終了の手順、初期表示、終了確認
 * - mainwindowcomposer_create.cpp     部品の生成（棋譜とモデル → 盤 → 画面部品 → 司令塔 → 表示とドック → セッション → 操作）
 * - mainwindowcomposer_wire.cpp       部品どうしのシグナル接続
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
    void createRecordAndModels();
    void createBoard();
    void createPanels();
    void createMatchAndAnalysis();
    void createPresentersAndDocks();
    void createDocks();
    void createSessions();
    void createControllers();

    // ===== シグナル接続（mainwindowcomposer_wire.cpp） =====
    void connectRecord();
    void connectBoard();
    void connectGame();
    void connectAnalysisAndUi();

    /// 盤・棋譜欄・対局者名の初期表示と、保存したウィンドウの大きさ・ドックの配置の復元
    void finalizeUi();

    /// 人間が盤で指せる手番か（盤のクリックと定跡手で使う。終了処理の後は false）
    bool isHumanTurnNow() const;
    /// 検討の候補手の矢印を盤に出すか（検討タブの設定）
    bool showConsiderationArrows() const;

    MainWindow& m_window;
    Ui::MainWindow& m_ui;
    AppState m_state;
    MainWindowComponents m_c;
    bool m_shutdownDone = false;
};

#endif // MAINWINDOWCOMPOSER_H
