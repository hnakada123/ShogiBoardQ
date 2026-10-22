/// @file uistatepolicymanager.cpp
/// @brief アプリケーション状態に応じたUI要素の有効/無効制御の実装

#include "uistatepolicymanager.h"

#include "ui_mainwindow.h"
#include "recordpane.h"
#include "kifunavigationcontroller.h"
#include "evaluationchartwidget.h"
#include "branchtreeview.h"
#include "considerationtabmanager.h"

#include <QAction>
#include "logcategories.h"

// ======================================================================
// 初期化
// ======================================================================

UiStatePolicyManager::UiStatePolicyManager(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
    , m_activity(deps.activity ? deps.activity : &m_defaultActivity)
{
    // 状態は動作状態（AppActivityState）が決める。変わるたびに表のとおりに有効・無効を切り替える
    connect(m_activity, &AppActivityState::changed, this, &UiStatePolicyManager::refreshActivity);
    refreshActivity();
}

// ======================================================================
// ポリシーの表
// ======================================================================

namespace {

using S = UiStatePolicyManager::AppState;
using E = UiStatePolicyManager::UiElement;

/// 状態の集合（AppState ごとに1ビット）
using StateSet = unsigned;
constexpr StateSet in(S state) { return 1u << static_cast<unsigned>(state); }
constexpr StateSet kAllStates = in(S::Idle) | in(S::DuringGame) | in(S::DuringAnalysis) | in(S::DuringCsaGame)
                              | in(S::DuringTsumeSearch) | in(S::DuringConsideration) | in(S::DuringPositionEdit);
/// 盤と棋譜を見るだけの状態（局面を変える操作はしないが、棋譜の閲覧・保存はできる）
constexpr StateSet kBrowsing = in(S::Idle) | in(S::DuringTsumeSearch) | in(S::DuringConsideration);
constexpr StateSet kPlaying = in(S::DuringGame) | in(S::DuringCsaGame);

enum class Off { Disable, Hide };

/// UI要素ごとの規則: enabledIn の状態では有効、それ以外は off（無効または非表示）
struct Rule {
    E element;
    StateSet enabledIn;
    Off off;
};

// 並びは UiElement の宣言順（下の static_assert で確かめる）
constexpr Rule kRules[] = {
    // --- ファイルメニュー ---
    {E::FileNew,                in(S::Idle),  Off::Disable},  // 新規・開く: 局面を変えるので待機中だけ
    {E::FileOpen,               in(S::Idle),  Off::Disable},
    {E::FileSave,               kBrowsing,    Off::Disable},
    {E::FileSaveAs,             kBrowsing,    Off::Disable},
    {E::FileSaveBoardImage,     kAllStates,   Off::Disable},
    {E::FileSaveEvalGraph,      kAllStates,   Off::Disable},
    {E::FileQuit,               kAllStates,   Off::Disable},
    // --- 編集メニュー ---
    {E::EditCopyKifu,           kBrowsing,    Off::Disable},
    {E::EditCopyPosition,       kAllStates,   Off::Disable},
    {E::EditPasteKifu,          in(S::Idle),  Off::Disable},
    {E::EditSfenViewer,         in(S::Idle),  Off::Disable},
    {E::EditCopyBoardImage,     kAllStates,   Off::Disable},
    {E::EditCopyEvalGraphImage, kAllStates,   Off::Disable},
    {E::EditStartPosition,      in(S::Idle),  Off::Hide},     // 局面編集の開始は待機中だけ出す
    {E::EditEndPosition,        in(S::DuringPositionEdit), Off::Hide},  // 編集中だけ出す項目
    {E::EditSetHirate,          in(S::DuringPositionEdit), Off::Hide},
    {E::EditSetTsume,           in(S::DuringPositionEdit), Off::Hide},
    {E::EditReturnAllToStand,   in(S::DuringPositionEdit), Off::Hide},
    {E::EditChangeTurn,         in(S::DuringPositionEdit), Off::Hide},
    // --- 対局メニュー ---
    {E::GameStart,              in(S::Idle),  Off::Disable},
    {E::GameCsa,                in(S::Idle),  Off::Disable},
    {E::GameResign,             kPlaying,     Off::Disable},  // ローカル対局は人間が参加するときだけ（effectivePolicy）
    {E::GameUndo,               in(S::DuringGame), Off::Disable},  // 同上
    {E::GameMakeImmediate,      kPlaying,     Off::Disable},
    {E::GameBreakOff,           kPlaying,     Off::Disable},
    {E::GameResume,             in(S::Idle),  Off::Disable},  // 中断した対局があるときだけ（effectivePolicy）
    {E::GameAnalyzeKifu,        in(S::Idle),  Off::Disable},
    {E::GameCancelAnalysis,     in(S::DuringAnalysis), Off::Disable},
    {E::GameTsumeSearch,        in(S::Idle),  Off::Disable},
    {E::GameStopTsumeSearch,    in(S::DuringTsumeSearch), Off::Disable},
    {E::GameTsumeshogiGenerator, in(S::Idle), Off::Disable},
    {E::GameJishogiScore,       kAllStates,   Off::Disable},
    {E::GameNyugyokuDeclaration, kPlaying,    Off::Disable},  // ローカル対局は人間が参加するときだけ（effectivePolicy）
    // --- 設定メニュー ---
    {E::SettingsEngine,         in(S::Idle) | in(S::DuringConsideration), Off::Disable},
    {E::SettingsReset,          in(S::Idle),  Off::Disable},  // アプリを終了するので待機中だけ
    // --- ウィジェット ---
    {E::WidgetNavigation,       kBrowsing,    Off::Disable},
    {E::WidgetBranchTree,       kBrowsing,    Off::Disable},
    // 局面編集中は盤が編集中の局面なので検討しない（編集を終えてから始める）
    {E::WidgetConsiderationStart, kAllStates & ~kPlaying & ~in(S::DuringPositionEdit), Off::Disable},
};

constexpr bool rulesFollowDeclarationOrder()
{
    constexpr int count = static_cast<int>(E::Count);
    if (static_cast<int>(sizeof(kRules) / sizeof(kRules[0])) != count) return false;
    for (int i = 0; i < count; ++i) {
        if (static_cast<int>(kRules[i].element) != i) return false;
    }
    return true;
}
static_assert(rulesFollowDeclarationOrder(), "kRules must list every UiElement in declaration order");

/// メニュー項目と UI要素の対応（1つの要素が複数の項目をまとめることがある）
struct ActionBinding {
    E element;
    QAction* Ui::MainWindow::* action;
};

constexpr ActionBinding kActions[] = {
    {E::FileNew,                 &Ui::MainWindow::actionNewGame},
    {E::FileOpen,                &Ui::MainWindow::actionOpenKifuFile},
    {E::FileSave,                &Ui::MainWindow::actionSave},
    {E::FileSaveAs,              &Ui::MainWindow::actionSaveAs},
    {E::FileSaveBoardImage,      &Ui::MainWindow::actionSaveBoardImage},
    {E::FileSaveEvalGraph,       &Ui::MainWindow::actionSaveEvaluationGraph},
    {E::FileQuit,                &Ui::MainWindow::actionQuit},
    {E::EditCopyKifu,            &Ui::MainWindow::actionCopyKIF},
    {E::EditCopyKifu,            &Ui::MainWindow::actionCopyKI2},
    {E::EditCopyKifu,            &Ui::MainWindow::actionCopyCSA},
    {E::EditCopyKifu,            &Ui::MainWindow::actionCopyJKF},
    {E::EditCopyKifu,            &Ui::MainWindow::actionCopyUSIAll},
    {E::EditCopyKifu,            &Ui::MainWindow::actionCopyUSEN},
    {E::EditCopyPosition,        &Ui::MainWindow::actionCopyUSICurrent},
    {E::EditCopyPosition,        &Ui::MainWindow::actionCopySFEN},
    {E::EditCopyPosition,        &Ui::MainWindow::actionCopyBOD},
    {E::EditPasteKifu,           &Ui::MainWindow::actionPasteKifu},
    {E::EditSfenViewer,          &Ui::MainWindow::actionSfenCollectionViewer},
    {E::EditCopyBoardImage,      &Ui::MainWindow::actionCopyBoardToClipboard},
    {E::EditCopyEvalGraphImage,  &Ui::MainWindow::actionCopyEvalGraphToClipboard},
    {E::EditStartPosition,       &Ui::MainWindow::actionStartEditPosition},
    {E::EditEndPosition,         &Ui::MainWindow::actionEndEditPosition},
    {E::EditSetHirate,           &Ui::MainWindow::actionSetHiratePosition},
    {E::EditSetTsume,            &Ui::MainWindow::actionSetTsumePosition},
    {E::EditReturnAllToStand,    &Ui::MainWindow::actionReturnAllPiecesToStand},
    {E::EditChangeTurn,          &Ui::MainWindow::actionChangeTurn},
    {E::GameStart,               &Ui::MainWindow::actionStartGame},
    {E::GameCsa,                 &Ui::MainWindow::actionCSA},
    {E::GameResign,              &Ui::MainWindow::actionResign},
    {E::GameUndo,                &Ui::MainWindow::actionUndoMove},
    {E::GameMakeImmediate,       &Ui::MainWindow::actionMakeImmediateMove},
    {E::GameBreakOff,            &Ui::MainWindow::actionBreakOffGame},
    {E::GameResume,              &Ui::MainWindow::actionResumeGame},
    {E::GameAnalyzeKifu,         &Ui::MainWindow::actionAnalyzeKifu},
    {E::GameCancelAnalysis,      &Ui::MainWindow::actionCancelAnalyzeKifu},
    {E::GameTsumeSearch,         &Ui::MainWindow::actionTsumeShogiSearch},
    {E::GameStopTsumeSearch,     &Ui::MainWindow::actionStopTsumeSearch},
    {E::GameTsumeshogiGenerator, &Ui::MainWindow::actionTsumeshogiGenerator},
    {E::GameJishogiScore,        &Ui::MainWindow::actionJishogiScore},
    {E::GameNyugyokuDeclaration, &Ui::MainWindow::actionNyugyokuDeclaration},
    {E::SettingsEngine,          &Ui::MainWindow::actionEngineSettings},
    {E::SettingsReset,           &Ui::MainWindow::actionResetSettings},
};

void applyActionPolicy(QAction* action, UiStatePolicyManager::Policy policy)
{
    if (!action) return;
    action->setVisible(policy != UiStatePolicyManager::Policy::Hidden);
    if (policy != UiStatePolicyManager::Policy::Hidden)
        action->setEnabled(policy == UiStatePolicyManager::Policy::Enabled);
}

} // namespace

UiStatePolicyManager::Policy UiStatePolicyManager::tablePolicy(AppState state, UiElement element)
{
    if (element == UiElement::Count) return Policy::Disabled;
    const Rule& rule = kRules[static_cast<int>(element)];
    if (rule.enabledIn & in(state)) return Policy::Enabled;
    return rule.off == Off::Hide ? Policy::Hidden : Policy::Disabled;
}

// ======================================================================
// 状態適用
// ======================================================================

UiStatePolicyManager::Policy UiStatePolicyManager::effectivePolicy(UiElement element) const
{
    if (element == UiElement::GameResume && !(m_deps.canResumeGame && m_deps.canResumeGame()))
        return Policy::Disabled;
    // 待った・投了・入玉宣言はローカル対局では人間が参加する場合だけ。エンジン同士では行わない
    if (element == UiElement::GameUndo
        || ((element == UiElement::GameResign || element == UiElement::GameNyugyokuDeclaration)
            && currentState() == AppState::DuringGame)) {
        if (!m_deps.localGameHasHuman || !m_deps.localGameHasHuman()) return Policy::Disabled;
    }
    return tablePolicy(currentState(), element);
}

bool UiStatePolicyManager::isEnabled(UiElement element) const
{
    return effectivePolicy(element) == Policy::Enabled;
}

void UiStatePolicyManager::refreshActivity()
{
    for (int i = 0; i < static_cast<int>(UiElement::Count); ++i) {
        const auto element = static_cast<UiElement>(i);
        applyPolicy(element, effectivePolicy(element));
    }
    emit stateChanged(currentState());
}

// ======================================================================
// 状態に左右されない条件
// ======================================================================

void UiStatePolicyManager::refreshResumeAvailability()
{
    applyPolicy(UiElement::GameResume, effectivePolicy(UiElement::GameResume));
}

void UiStatePolicyManager::enableNavigationIfAllowed()
{
    if (!isEnabled(UiElement::WidgetNavigation)) return;
    if (m_deps.navigation) m_deps.navigation->setStepActionsEnabled(true);
}

// ======================================================================
// ポリシー適用（個別要素）
// ======================================================================

void UiStatePolicyManager::applyPolicy(UiElement element, Policy policy)
{
    if (m_deps.ui) {
        for (const ActionBinding& binding : kActions) {
            if (binding.element == element) applyActionPolicy(m_deps.ui->*binding.action, policy);
        }
    }

    const bool enabled = policy == Policy::Enabled;
    switch (element) {
    case UiElement::WidgetNavigation:
        if (m_deps.recordPane) m_deps.recordPane->setKifuViewEnabled(enabled);
        if (m_deps.navigation) m_deps.navigation->setStepActionsEnabled(enabled);
        if (m_deps.evalChart) m_deps.evalChart->setNavigationEnabled(enabled);
        break;
    case UiElement::WidgetBranchTree:
        if (m_deps.branchTree) m_deps.branchTree->setClickEnabled(enabled);
        break;
    case UiElement::WidgetConsiderationStart:
        if (m_deps.consideration) m_deps.consideration->setStartConsiderationEnabled(enabled);
        break;
    default:
        break;
    }
}
