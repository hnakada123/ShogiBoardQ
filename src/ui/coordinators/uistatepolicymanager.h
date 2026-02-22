#ifndef UISTATEPOLICYMANAGER_H
#define UISTATEPOLICYMANAGER_H

/// @file uistatepolicymanager.h
/// @brief アプリケーション状態に応じたUI要素の有効/無効制御を一元管理するクラス

#include <QObject>
#include <QHash>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcUiPolicy)

class QAction;
class RecordPane;
class EngineAnalysisTab;
class BoardInteractionController;

namespace Ui { class MainWindow; }

/**
 * @brief アプリケーション状態ごとのUI要素ポリシーを一元管理するマネージャ
 *
 * (AppState, UiElement) → Policy の2次元テーブルで宣言的に管理し、
 * 状態遷移時に全UI要素のポリシーを一括適用する。
 */
class UiStatePolicyManager : public QObject
{
    Q_OBJECT
public:
    /// アプリケーション状態
    enum class AppState {
        Idle,                  ///< 待機状態
        DuringGame,            ///< 対局中
        DuringAnalysis,        ///< 棋譜解析中
        DuringCsaGame,         ///< CSA通信対局中
        DuringTsumeSearch,     ///< 詰み探索中
        DuringConsideration,   ///< 検討中
        DuringPositionEdit     ///< 局面編集中
    };
    Q_ENUM(AppState)

    /// UI要素に適用するポリシー
    enum class Policy {
        Enabled,  ///< 表示+有効
        Disabled, ///< 表示+無効
        Hidden,   ///< 非表示
        Shown     ///< 表示+有効（Hidden の対）
    };

    /// UI要素の識別子
    enum class UiElement {
        // --- ファイルメニュー ---
        FileNew,
        FileOpen,
        FileSave,
        FileSaveAs,
        FileSaveBoardImage,
        FileSaveEvalGraph,
        FileQuit,

        // --- 編集メニュー ---
        EditCopyKifu,          ///< 棋譜コピー（サブメニュー内全項目）
        EditCopyPosition,      ///< 局面コピー（サブメニュー内全項目）
        EditPasteKifu,
        EditSfenViewer,
        EditCopyBoardImage,
        EditCopyEvalGraphImage,
        EditStartPosition,     ///< 局面編集開始
        EditEndPosition,       ///< 局面編集終了
        EditSetHirate,         ///< 平手初期配置
        EditSetTsume,          ///< 詰将棋初期配置
        EditReturnAllToStand,  ///< 全駒を駒台へ
        EditChangeTurn,        ///< 手番変更

        // --- 対局メニュー ---
        GameStart,
        GameCsa,
        GameResign,
        GameUndo,
        GameMakeImmediate,
        GameBreakOff,
        GameAnalyzeKifu,
        GameCancelAnalysis,
        GameTsumeSearch,
        GameStopTsumeSearch,
        GameTsumeshogiGenerator,  ///< 詰将棋局面生成
        GameJishogiScore,
        GameNyugyokuDeclaration,

        // --- 設定メニュー ---
        SettingsEngine,

        // --- ウィジェット ---
        WidgetNavigation,      ///< ナビゲーションボタン + 棋譜欄クリック
        WidgetBranchTree,      ///< 分岐ツリークリック
        WidgetBoardClick,      ///< 盤面クリック（指し手入力）

        Count  ///< 要素数（番兵）
    };

    /// 依存オブジェクト
    struct Deps {
        Ui::MainWindow* ui = nullptr;
        RecordPane* recordPane = nullptr;
        EngineAnalysisTab* analysisTab = nullptr;
        BoardInteractionController* boardController = nullptr;
    };

    explicit UiStatePolicyManager(QObject* parent = nullptr);

    /// 依存オブジェクトを更新する（遅延生成対応）
    void updateDeps(const Deps& deps);

    /// 指定状態のポリシーを全UI要素に一括適用する
    void applyState(AppState state);

    /// 現在の状態を返す
    AppState currentState() const { return m_currentState; }

    /// 指定要素が現在の状態で有効（Enabled/Shown）かを返す
    bool isEnabled(UiElement element) const;

public slots:
    /// 各状態遷移用の便利スロット（シグナル直結用）
    void transitionToIdle();
    void transitionToDuringGame();
    void transitionToDuringAnalysis();
    void transitionToDuringCsaGame();
    void transitionToDuringTsumeSearch();
    void transitionToDuringConsideration();
    void transitionToDuringPositionEdit();

signals:
    /// 状態が変更されたときに発火する
    void stateChanged(AppState newState);

private:
    /// ポリシーテーブルを構築する
    void buildPolicyTable();

    /// 個別のUI要素にポリシーを適用する
    void applyPolicy(UiElement element, Policy policy);

    /// QAction にポリシーを適用するヘルパ
    void applyActionPolicy(QAction* action, Policy policy);

    Deps m_deps;
    AppState m_currentState = AppState::Idle;

    /// ポリシーテーブル: state → (element → policy)
    QHash<AppState, QHash<UiElement, Policy>> m_policyTable;
};

#endif // UISTATEPOLICYMANAGER_H
