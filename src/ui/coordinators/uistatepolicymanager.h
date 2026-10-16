#ifndef UISTATEPOLICYMANAGER_H
#define UISTATEPOLICYMANAGER_H

/// @file uistatepolicymanager.h
/// @brief アプリケーション状態に応じたUI要素の有効/無効制御を一元管理するクラス

#include <QObject>
#include "appactivitystate.h"
#include <functional>

class QAction;
class RecordPane;
class KifuNavigationController;
class BranchTreeManager;
class ConsiderationTabManager;

namespace Ui { class MainWindow; }

/**
 * @brief アプリケーション状態ごとのUI要素ポリシーを一元管理するマネージャ
 *
 * UI要素ごとに「有効にする状態の集合」と「それ以外の状態で無効にするか隠すか」を
 * コンパイル時の表（uistatepolicymanager.cpp の kRules）で宣言し、
 * 状態が変わるたびに全UI要素へ一括適用する。
 */
class UiStatePolicyManager : public QObject
{
    Q_OBJECT
public:
    /// アプリケーション状態
    using AppState = AppActivityState::Activity;

    /// UI要素に適用するポリシー
    enum class Policy {
        Enabled,  ///< 表示+有効
        Disabled, ///< 表示+無効
        Hidden    ///< 非表示
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
        GameResume,
        GameAnalyzeKifu,
        GameCancelAnalysis,
        GameTsumeSearch,
        GameStopTsumeSearch,
        GameTsumeshogiGenerator,  ///< 詰将棋局面生成
        GameJishogiScore,
        GameNyugyokuDeclaration,

        // --- 設定メニュー ---
        SettingsEngine,
        SettingsReset,

        // --- ウィジェット ---
        WidgetNavigation,      ///< ナビゲーションボタン + 棋譜欄クリック
        WidgetBranchTree,      ///< 分岐ツリークリック
        WidgetConsiderationStart, ///< 検討ドック「検討開始」ボタン

        Count  ///< 要素数（番兵）
    };

    /// 依存オブジェクト
    struct Deps {
        Ui::MainWindow* ui = nullptr;
        RecordPane* recordPane = nullptr;                     ///< 棋譜欄のクリック
        KifuNavigationController* navigation = nullptr;       ///< 局面送りの操作（棋譜欄と評価値グラフのボタン）
        BranchTreeManager* branchTree = nullptr;              ///< 分岐ツリーのクリック
        ConsiderationTabManager* consideration = nullptr;     ///< 検討ドックの「検討開始」
        std::function<bool()> localGameHasHuman;  ///< ローカル対局に人間が参加しているか（待った・投了・入玉宣言）
        std::function<bool()> canResumeGame;      ///< 中断した対局を再開できるか（司令塔に問い合わせる）
        AppActivityState* activity = nullptr;
    };

    /// deps.activity が無ければ自分だけの動作状態を使う（テスト用）
    explicit UiStatePolicyManager(const Deps& deps, QObject* parent = nullptr);
    UiStatePolicyManager() : UiStatePolicyManager(Deps{}) {}

    /// 現在の状態を返す
    AppState currentState() const { return m_activity->activity(); }

    /// 指定要素が現在の状態で有効かを返す
    bool isEnabled(UiElement element) const;

    /// 表だけで決まるポリシー（再開の可否・人間の参加などの条件を除く）
    static Policy tablePolicy(AppState state, UiElement element);

public slots:
    /// 再開できるかが変わった（「再開」の有効・無効を問い合わせ直す）
    void refreshResumeAvailability();

    /// ナビゲーションが許可されている場合のみ矢印ボタンを有効化する
    void enableNavigationIfAllowed();

signals:
    /// 状態が変更されたときに発火する
    void stateChanged(AppActivityState::Activity newState);

private:
    /// 再開の可否・人間の参加を反映した現在のポリシーを返す
    Policy effectivePolicy(UiElement element) const;

    /// 個別のUI要素にポリシーを適用する
    void applyPolicy(UiElement element, Policy policy);

    Deps m_deps;
    AppActivityState m_defaultActivity;
    AppActivityState* m_activity = nullptr;
    void refreshActivity();
};

#endif // UISTATEPOLICYMANAGER_H
