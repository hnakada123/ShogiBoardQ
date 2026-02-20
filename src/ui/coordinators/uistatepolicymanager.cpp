/// @file uistatepolicymanager.cpp
/// @brief アプリケーション状態に応じたUI要素の有効/無効制御の実装

#include "uistatepolicymanager.h"

#include "ui_mainwindow.h"
#include "recordpane.h"
#include "engineanalysistab.h"
#include "boardinteractioncontroller.h"

#include <QAction>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcUiPolicy, "shogi.ui.policy")

// ======================================================================
// 初期化
// ======================================================================

UiStatePolicyManager::UiStatePolicyManager(QObject* parent)
    : QObject(parent)
{
    buildPolicyTable();
}

void UiStatePolicyManager::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

// ======================================================================
// ポリシーテーブル構築
// ======================================================================

void UiStatePolicyManager::buildPolicyTable()
{
    using S = AppState;
    using E = UiElement;
    using P = Policy;

    // ヘルパ: 全状態の特定要素にデフォルトポリシーを設定
    auto setAll = [this](E elem, P policy) {
        const auto states = {S::Idle, S::DuringGame, S::DuringAnalysis,
                             S::DuringCsaGame, S::DuringTsumeSearch,
                             S::DuringConsideration, S::DuringPositionEdit};
        for (auto s : states) {
            m_policyTable[s][elem] = policy;
        }
    };

    // ヘルパ: 特定状態の特定要素にポリシーを設定
    auto set = [this](S state, E elem, P policy) {
        m_policyTable[state][elem] = policy;
    };

    // =====================================================================
    // ファイルメニュー
    // =====================================================================
    // 新規/開く: Idle のみ有効（検討中・詰み探索中はゲーム状態変更不可）
    for (E elem : {E::FileNew, E::FileOpen}) {
        setAll(elem, P::Disabled);
        set(S::Idle, elem, P::Enabled);
    }
    // 上書き保存/名前を付けて保存: Idle, 詰み探索中, 検討中で有効（状態を変更しない）
    for (E elem : {E::FileSave, E::FileSaveAs}) {
        setAll(elem, P::Disabled);
        set(S::Idle,                elem, P::Enabled);
        set(S::DuringTsumeSearch,   elem, P::Enabled);
        set(S::DuringConsideration, elem, P::Enabled);
    }
    // 盤面画像保存/評価値グラフ画像保存/終了: 常に有効
    setAll(E::FileSaveBoardImage, P::Enabled);
    setAll(E::FileSaveEvalGraph,  P::Enabled);
    setAll(E::FileQuit,           P::Enabled);

    // =====================================================================
    // 編集メニュー
    // =====================================================================
    // 棋譜コピー: Idle, 詰み探索中, 検討中のみ有効
    setAll(E::EditCopyKifu, P::Disabled);
    set(S::Idle,              E::EditCopyKifu, P::Enabled);
    set(S::DuringTsumeSearch, E::EditCopyKifu, P::Enabled);
    set(S::DuringConsideration, E::EditCopyKifu, P::Enabled);

    // 局面コピー: 常に有効
    setAll(E::EditCopyPosition, P::Enabled);

    // 棋譜貼り付け: Idle のみ有効（検討中・詰み探索中はゲーム状態変更不可）
    setAll(E::EditPasteKifu, P::Disabled);
    set(S::Idle, E::EditPasteKifu, P::Enabled);

    // SFEN集ビューア: Idle のみ有効（検討中・詰み探索中は局面読み込み不可）
    setAll(E::EditSfenViewer, P::Disabled);
    set(S::Idle, E::EditSfenViewer, P::Enabled);

    // 盤面画像コピー: 常に有効
    setAll(E::EditCopyBoardImage, P::Enabled);

    // 評価値グラフ画像コピー: 常に有効
    setAll(E::EditCopyEvalGraphImage, P::Enabled);

    // 局面編集開始: Idle のみ表示、それ以外は非表示
    setAll(E::EditStartPosition, P::Hidden);
    set(S::Idle, E::EditStartPosition, P::Shown);

    // 局面編集終了/平手/詰将棋/全駒駒台/手番変更: 編集中のみ表示
    for (E elem : {E::EditEndPosition, E::EditSetHirate, E::EditSetTsume,
                   E::EditReturnAllToStand, E::EditChangeTurn}) {
        setAll(elem, P::Hidden);
        set(S::DuringPositionEdit, elem, P::Shown);
    }

    // =====================================================================
    // 対局メニュー
    // =====================================================================
    // 対局/CSA通信対局: Idle のみ有効
    for (E elem : {E::GameStart, E::GameCsa}) {
        setAll(elem, P::Disabled);
        set(S::Idle, elem, P::Enabled);
    }

    // 投了: 対局中/CSA中のみ有効
    setAll(E::GameResign, P::Disabled);
    set(S::DuringGame,    E::GameResign, P::Enabled);
    set(S::DuringCsaGame, E::GameResign, P::Enabled);

    // 待った: 対局中のみ有効
    setAll(E::GameUndo, P::Disabled);
    set(S::DuringGame, E::GameUndo, P::Enabled);

    // すぐ指させる: 対局中/CSA中のみ有効
    setAll(E::GameMakeImmediate, P::Disabled);
    set(S::DuringGame,    E::GameMakeImmediate, P::Enabled);
    set(S::DuringCsaGame, E::GameMakeImmediate, P::Enabled);

    // 中断: 対局中/CSA中のみ有効
    setAll(E::GameBreakOff, P::Disabled);
    set(S::DuringGame,    E::GameBreakOff, P::Enabled);
    set(S::DuringCsaGame, E::GameBreakOff, P::Enabled);

    // 棋譜解析: Idle のみ有効
    setAll(E::GameAnalyzeKifu, P::Disabled);
    set(S::Idle, E::GameAnalyzeKifu, P::Enabled);

    // 棋譜解析中止: 解析中のみ有効
    setAll(E::GameCancelAnalysis, P::Disabled);
    set(S::DuringAnalysis, E::GameCancelAnalysis, P::Enabled);

    // 詰み探索: Idle のみ有効
    setAll(E::GameTsumeSearch, P::Disabled);
    set(S::Idle, E::GameTsumeSearch, P::Enabled);

    // 詰み探索中止: 詰み探索中のみ有効
    setAll(E::GameStopTsumeSearch, P::Disabled);
    set(S::DuringTsumeSearch, E::GameStopTsumeSearch, P::Enabled);

    // 持将棋点数: 常に有効
    setAll(E::GameJishogiScore, P::Enabled);

    // 入玉宣言: 対局中/CSA中のみ有効
    setAll(E::GameNyugyokuDeclaration, P::Disabled);
    set(S::DuringGame,    E::GameNyugyokuDeclaration, P::Enabled);
    set(S::DuringCsaGame, E::GameNyugyokuDeclaration, P::Enabled);

    // =====================================================================
    // 設定メニュー
    // =====================================================================
    // エンジン設定: Idle, 検討中のみ有効
    setAll(E::SettingsEngine, P::Disabled);
    set(S::Idle,                E::SettingsEngine, P::Enabled);
    set(S::DuringConsideration, E::SettingsEngine, P::Enabled);

    // =====================================================================
    // ウィジェット
    // =====================================================================
    // ナビゲーション: Idle, 詰み探索中, 検討中のみ有効
    setAll(E::WidgetNavigation, P::Disabled);
    set(S::Idle,              E::WidgetNavigation, P::Enabled);
    set(S::DuringTsumeSearch, E::WidgetNavigation, P::Enabled);
    set(S::DuringConsideration, E::WidgetNavigation, P::Enabled);

    // 分岐ツリー: Idle, 詰み探索中, 検討中のみ有効
    setAll(E::WidgetBranchTree, P::Disabled);
    set(S::Idle,              E::WidgetBranchTree, P::Enabled);
    set(S::DuringTsumeSearch, E::WidgetBranchTree, P::Enabled);
    set(S::DuringConsideration, E::WidgetBranchTree, P::Enabled);

    // 盤面クリック（指し手入力）: 対局中/CSA中はisHumanTurnCbで手番制御するため有効にする
    setAll(E::WidgetBoardClick, P::Disabled);
    set(S::Idle,          E::WidgetBoardClick, P::Enabled);
    set(S::DuringGame,    E::WidgetBoardClick, P::Enabled);
    set(S::DuringCsaGame, E::WidgetBoardClick, P::Enabled);
}

// ======================================================================
// 状態適用
// ======================================================================

bool UiStatePolicyManager::isEnabled(UiElement element) const
{
    const auto& policies = m_policyTable.value(m_currentState);
    const auto it = policies.find(element);
    if (it != policies.end()) {
        return it.value() == Policy::Enabled || it.value() == Policy::Shown;
    }
    return true;
}

void UiStatePolicyManager::applyState(AppState state)
{
    qCDebug(lcUiPolicy).noquote()
        << "applyState:" << static_cast<int>(state)
        << "(from" << static_cast<int>(m_currentState) << ")";

    m_currentState = state;

    const auto& policies = m_policyTable.value(state);
    for (auto it = policies.constBegin(); it != policies.constEnd(); ++it) {
        applyPolicy(it.key(), it.value());
    }

    Q_EMIT stateChanged(state);
}

// ======================================================================
// 遷移スロット
// ======================================================================

void UiStatePolicyManager::transitionToIdle()
{
    applyState(AppState::Idle);
}

void UiStatePolicyManager::transitionToDuringGame()
{
    applyState(AppState::DuringGame);
}

void UiStatePolicyManager::transitionToDuringAnalysis()
{
    applyState(AppState::DuringAnalysis);
}

void UiStatePolicyManager::transitionToDuringCsaGame()
{
    applyState(AppState::DuringCsaGame);
}

void UiStatePolicyManager::transitionToDuringTsumeSearch()
{
    applyState(AppState::DuringTsumeSearch);
}

void UiStatePolicyManager::transitionToDuringConsideration()
{
    applyState(AppState::DuringConsideration);
}

void UiStatePolicyManager::transitionToDuringPositionEdit()
{
    applyState(AppState::DuringPositionEdit);
}

// ======================================================================
// ポリシー適用（個別要素）
// ======================================================================

void UiStatePolicyManager::applyActionPolicy(QAction* action, Policy policy)
{
    if (!action) return;

    switch (policy) {
    case Policy::Enabled:
        action->setVisible(true);
        action->setEnabled(true);
        break;
    case Policy::Disabled:
        action->setVisible(true);
        action->setEnabled(false);
        break;
    case Policy::Hidden:
        action->setVisible(false);
        break;
    case Policy::Shown:
        action->setVisible(true);
        action->setEnabled(true);
        break;
    }
}

void UiStatePolicyManager::applyPolicy(UiElement element, Policy policy)
{
    auto* ui = m_deps.ui;

    switch (element) {
    // --- ファイルメニュー ---
    case UiElement::FileNew:
        applyActionPolicy(ui ? ui->actionNewGame : nullptr, policy);
        break;
    case UiElement::FileOpen:
        applyActionPolicy(ui ? ui->actionOpenKifuFile : nullptr, policy);
        break;
    case UiElement::FileSave:
        applyActionPolicy(ui ? ui->actionSave : nullptr, policy);
        break;
    case UiElement::FileSaveAs:
        applyActionPolicy(ui ? ui->actionSaveAs : nullptr, policy);
        break;
    case UiElement::FileSaveBoardImage:
        applyActionPolicy(ui ? ui->actionSaveBoardImage : nullptr, policy);
        break;
    case UiElement::FileSaveEvalGraph:
        applyActionPolicy(ui ? ui->actionSaveEvaluationGraph : nullptr, policy);
        break;
    case UiElement::FileQuit:
        applyActionPolicy(ui ? ui->actionQuit : nullptr, policy);
        break;

    // --- 編集メニュー（棋譜コピー系） ---
    case UiElement::EditCopyKifu:
        if (ui) {
            applyActionPolicy(ui->actionCopyKIF, policy);
            applyActionPolicy(ui->actionCopyKI2, policy);
            applyActionPolicy(ui->actionCopyCSA, policy);
            applyActionPolicy(ui->actionCopyJKF, policy);
        }
        break;
    case UiElement::EditCopyPosition:
        if (ui) {
            applyActionPolicy(ui->actionCopyUSICurrent, policy);
            applyActionPolicy(ui->actionCopyUSIAll, policy);
            applyActionPolicy(ui->actionCopyUSEN, policy);
            applyActionPolicy(ui->actionCopySFEN, policy);
            applyActionPolicy(ui->actionCopyBOD, policy);
        }
        break;
    case UiElement::EditPasteKifu:
        applyActionPolicy(ui ? ui->actionPasteKifu : nullptr, policy);
        break;
    case UiElement::EditSfenViewer:
        applyActionPolicy(ui ? ui->actionSfenCollectionViewer : nullptr, policy);
        break;
    case UiElement::EditCopyBoardImage:
        applyActionPolicy(ui ? ui->actionCopyBoardToClipboard : nullptr, policy);
        break;
    case UiElement::EditCopyEvalGraphImage:
        applyActionPolicy(ui ? ui->actionCopyEvalGraphToClipboard : nullptr, policy);
        break;

    // --- 編集メニュー（局面編集系） ---
    case UiElement::EditStartPosition:
        applyActionPolicy(ui ? ui->actionStartEditPosition : nullptr, policy);
        break;
    case UiElement::EditEndPosition:
        applyActionPolicy(ui ? ui->actionEndEditPosition : nullptr, policy);
        break;
    case UiElement::EditSetHirate:
        applyActionPolicy(ui ? ui->actionSetHiratePosition : nullptr, policy);
        break;
    case UiElement::EditSetTsume:
        applyActionPolicy(ui ? ui->actionSetTsumePosition : nullptr, policy);
        break;
    case UiElement::EditReturnAllToStand:
        applyActionPolicy(ui ? ui->actionReturnAllPiecesToStand : nullptr, policy);
        break;
    case UiElement::EditChangeTurn:
        applyActionPolicy(ui ? ui->actionChangeTurn : nullptr, policy);
        break;

    // --- 対局メニュー ---
    case UiElement::GameStart:
        applyActionPolicy(ui ? ui->actionStartGame : nullptr, policy);
        break;
    case UiElement::GameCsa:
        applyActionPolicy(ui ? ui->actionCSA : nullptr, policy);
        break;
    case UiElement::GameResign:
        applyActionPolicy(ui ? ui->actionResign : nullptr, policy);
        break;
    case UiElement::GameUndo:
        applyActionPolicy(ui ? ui->actionUndoMove : nullptr, policy);
        break;
    case UiElement::GameMakeImmediate:
        applyActionPolicy(ui ? ui->actionMakeImmediateMove : nullptr, policy);
        break;
    case UiElement::GameBreakOff:
        applyActionPolicy(ui ? ui->actionBreakOffGame : nullptr, policy);
        break;
    case UiElement::GameAnalyzeKifu:
        applyActionPolicy(ui ? ui->actionAnalyzeKifu : nullptr, policy);
        break;
    case UiElement::GameCancelAnalysis:
        applyActionPolicy(ui ? ui->actionCancelAnalyzeKifu : nullptr, policy);
        break;
    case UiElement::GameTsumeSearch:
        applyActionPolicy(ui ? ui->actionTsumeShogiSearch : nullptr, policy);
        break;
    case UiElement::GameStopTsumeSearch:
        applyActionPolicy(ui ? ui->actionStopTsumeSearch : nullptr, policy);
        break;
    case UiElement::GameJishogiScore:
        applyActionPolicy(ui ? ui->actionJishogiScore : nullptr, policy);
        break;
    case UiElement::GameNyugyokuDeclaration:
        applyActionPolicy(ui ? ui->actionNyugyokuDeclaration : nullptr, policy);
        break;

    // --- 設定メニュー ---
    case UiElement::SettingsEngine:
        applyActionPolicy(ui ? ui->actionEngineSettings : nullptr, policy);
        break;

    // --- ウィジェット ---
    case UiElement::WidgetNavigation:
        if (m_deps.recordPane) {
            m_deps.recordPane->setNavigationEnabled(policy == Policy::Enabled);
        }
        break;
    case UiElement::WidgetBranchTree:
        if (m_deps.analysisTab) {
            m_deps.analysisTab->setBranchTreeClickEnabled(policy == Policy::Enabled);
        }
        break;
    case UiElement::WidgetBoardClick:
        if (m_deps.boardController) {
            m_deps.boardController->setMoveInputEnabled(policy == Policy::Enabled);
        }
        break;

    case UiElement::Count:
        break;
    }
}
