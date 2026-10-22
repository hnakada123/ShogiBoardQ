/// @file tst_app_ui_state_policy.cpp
/// @brief UiStatePolicyManager の状態遷移・ポリシーテーブル検証テスト
///
/// UI の状態は動作状態（AppActivityState）が決め、UiStatePolicyManager はそれに従って
/// メニュー・ボタンの有効・無効を切り替える。部品が無くても動くので、表の正しさと追従を確かめる。

#include <QtTest>
#include <QSignalSpy>
#include <QMainWindow>
#include "ui_mainwindow.h"

#include "appactivitystate.h"
#include "uistatepolicymanager.h"

class TestAppUiStatePolicy : public QObject
{
    Q_OBJECT

private:
    using S = UiStatePolicyManager::Activity;
    using E = UiStatePolicyManager::UiElement;

    /// 動作状態と、それに従う UI 状態ポリシー
    struct Policy {
        AppActivityState activity;
        UiStatePolicyManager mgr;
        ActivityLease lease;   ///< 表を確かめるための状態（持ち主なし）
        explicit Policy(UiStatePolicyManager::Deps deps = {}) : mgr(withActivity(deps, &activity)) {}
        void set(S state) { lease = activity.begin(state, nullptr); }
        static UiStatePolicyManager::Deps withActivity(UiStatePolicyManager::Deps deps, AppActivityState* activity)
        {
            deps.activity = activity;
            return deps;
        }
    };

    /// 指定状態で指定要素が有効であることを検証するヘルパ
    void verifyEnabled(Policy& p, S state, E element, const char* elementName)
    {
        p.set(state);
        QVERIFY2(p.mgr.isEnabled(element),
                 qPrintable(QStringLiteral("%1 should be enabled in state %2")
                                .arg(QLatin1String(elementName))
                                .arg(static_cast<int>(state))));
    }

    /// 指定状態で指定要素が無効であることを検証するヘルパ
    void verifyDisabled(Policy& p, S state, E element, const char* elementName)
    {
        p.set(state);
        QVERIFY2(!p.mgr.isEnabled(element),
                 qPrintable(QStringLiteral("%1 should be disabled in state %2")
                                .arg(QLatin1String(elementName))
                                .arg(static_cast<int>(state))));
    }

private slots:
    void sharedActivityDrivesPolicy()
    {
        AppActivityState activity;
        UiStatePolicyManager::Deps deps;
        deps.activity = &activity;
        deps.localGameHasHuman = [] { return true; };
        UiStatePolicyManager manager(deps);
        QSignalSpy changed(&activity, &AppActivityState::changed);
        ActivityLease game = activity.begin(AppActivityState::Activity::DuringGame, this);
        QCOMPARE(manager.currentState(), S::DuringGame);
        QVERIFY(manager.isEnabled(E::GameUndo));
        QCOMPARE(changed.count(), 1);
        ActivityLease consideration = activity.begin(S::DuringConsideration, &manager);
        QCOMPARE(activity.activity(), AppActivityState::Activity::DuringConsideration);
        QVERIFY(!manager.isEnabled(E::GameUndo));
        QCOMPARE(changed.count(), 2);
    }

    void settingsResetOnlyEnabledWhileIdle()
    {
        QMainWindow window;
        Ui::MainWindow ui;
        ui.setupUi(&window);
        QVERIFY(ui.Setting->actions().contains(ui.actionResetSettings));
        UiStatePolicyManager::Deps deps;
        deps.ui = &ui;
        Policy p(deps);
        for (const auto state : {S::DuringGame, S::DuringCsaGame, S::DuringAnalysis,
                                S::DuringTsumeSearch, S::DuringConsideration,
                                S::DuringPositionEdit, S::Idle}) {
            p.set(state);
            QCOMPARE(ui.actionResetSettings->isEnabled(), state == S::Idle);
        }
    }

    /// macOS の Qt は文字列から役割を推測し、「設定…」で始まる項目を
    /// アプリケーションメニューの「設定」（⌘,）に、「…について」で終わる項目を
    /// 「ShogiBoardQ について」に移す。どちらも元のメニューに残す。
    void menuItemsAreNotMovedToApplicationMenu()
    {
        QMainWindow window;
        Ui::MainWindow ui;
        ui.setupUi(&window);
        QCOMPARE(ui.actionResetSettings->menuRole(), QAction::NoRole);
        QCOMPARE(ui.actionAboutQt->menuRole(), QAction::NoRole);
        QVERIFY(ui.Help->actions().contains(ui.actionAboutQt));
    }

    void allRecordFormatsShareMenuPolicy()
    {
        QMainWindow window;
        Ui::MainWindow ui;
        ui.setupUi(&window);
        UiStatePolicyManager::Deps deps;
        deps.ui = &ui;
        Policy p(deps);
        UiStatePolicyManager& manager = p.mgr;
        for (const auto state : {S::Idle, S::DuringGame, S::DuringCsaGame, S::DuringAnalysis}) {
            p.set(state);
            const bool canCopyRecord = manager.isEnabled(E::EditCopyKifu);
            for (auto* action : {ui.actionCopyKIF, ui.actionCopyKI2, ui.actionCopyCSA,
                                  ui.actionCopyJKF, ui.actionCopyUSIAll, ui.actionCopyUSEN})
                QCOMPARE(action->isEnabled(), canCopyRecord);
            for (auto* action : {ui.actionCopySFEN, ui.actionCopyBOD, ui.actionCopyUSICurrent})
                QCOMPARE(action->isEnabled(), manager.isEnabled(E::EditCopyPosition));
        }
    }

    // ================================================================
    // 初期状態テスト
    // ================================================================

    /// 初期状態が Idle であること
    void initialStateIsIdle()
    {
        Policy p;
        UiStatePolicyManager& mgr = p.mgr;
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    // ================================================================
    // 動作状態への追従
    // ================================================================

    /// どの動作状態に変えても、ポリシーはその状態になる
    void policyFollowsEveryActivity()
    {
        Policy p;
        for (const auto state : {S::DuringGame, S::DuringAnalysis, S::DuringCsaGame, S::DuringTsumeSearch,
                                S::DuringConsideration, S::DuringPositionEdit, S::Idle}) {
            p.set(state);
            QCOMPARE(p.mgr.currentState(), state);
        }
    }

    // ================================================================
    // 動作状態の持ち主（ActivityLease）
    // ================================================================

    /// 終えられるのは今の持ち主だけ。取り上げられた後に手放しても、新しい動作は変わらない
    void onlyCurrentOwnerReturnsToIdle()
    {
        AppActivityState activity;
        QObject analysis;
        QObject game;
        ActivityLease analysisLease = activity.begin(S::DuringAnalysis, &analysis);
        QVERIFY(analysisLease.isCurrent());
        ActivityLease gameLease = activity.begin(S::DuringGame, &game);
        QVERIFY(!analysisLease.isCurrent());
        analysisLease.reset();
        QCOMPARE(activity.activity(), S::DuringGame);
        gameLease.reset();
        QCOMPARE(activity.activity(), S::Idle);
        // 手放したリースは何もしない
        gameLease.reset();
        QCOMPARE(activity.activity(), S::Idle);
    }

    /// 取り上げられた持ち主には、始めた部品の準備が済んでから（次のイベントループで）知らせる
    void revokedReachesPreviousOwnerLater()
    {
        AppActivityState activity;
        QObject edit;
        QObject consideration;
        QSignalSpy revoked(&activity, &AppActivityState::revoked);
        ActivityLease editLease = activity.begin(S::DuringPositionEdit, &edit);
        ActivityLease considerationLease = activity.begin(S::DuringConsideration, &consideration);
        QCOMPARE(revoked.count(), 0);
        QTRY_COMPARE(revoked.count(), 1);
        QCOMPARE(revoked.first().at(0).value<QObject*>(), &edit);
        // 同じ持ち主が始め直しても知らせない
        considerationLease = activity.begin(S::DuringConsideration, &consideration);
        QTest::qWait(10);
        QCOMPARE(revoked.count(), 1);
        QCOMPARE(activity.activity(), S::DuringConsideration);
    }

    /// リースを破棄すると待機中に戻る（移したリースは元の変数では何もしない）
    void leaseMovesAndReleasesOnDestruction()
    {
        AppActivityState activity;
        {
            ActivityLease first = activity.begin(S::DuringTsumeSearch, nullptr);
            ActivityLease moved = std::move(first);
            first.reset();
            QCOMPARE(activity.activity(), S::DuringTsumeSearch);
            QVERIFY(moved.isCurrent());
        }
        QCOMPARE(activity.activity(), S::Idle);
    }

    // ================================================================
    // 遷移サイクルテスト
    // ================================================================

    /// Idle → DuringGame → Idle の遷移サイクルが正しく動作すること
    void idleGameIdleCycle()
    {
        Policy p;
        UiStatePolicyManager& mgr = p.mgr;
        QCOMPARE(mgr.currentState(), S::Idle);

        p.set(S::DuringGame);
        QCOMPARE(mgr.currentState(), S::DuringGame);

        p.set(S::Idle);
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    /// Idle → DuringAnalysis → Idle の遷移サイクルが正しく動作すること
    void idleAnalysisIdleCycle()
    {
        Policy p;
        UiStatePolicyManager& mgr = p.mgr;
        QCOMPARE(mgr.currentState(), S::Idle);

        p.set(S::DuringAnalysis);
        QCOMPARE(mgr.currentState(), S::DuringAnalysis);

        p.set(S::Idle);
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    /// 全7状態を順に遷移して戻れること
    void fullStateCycle()
    {
        Policy p;
        UiStatePolicyManager& mgr = p.mgr;
        QSignalSpy spy(&p.activity, &AppActivityState::changed);

        const QList<S> allStates = {
            S::Idle, S::DuringGame, S::DuringAnalysis,
            S::DuringCsaGame, S::DuringTsumeSearch,
            S::DuringConsideration, S::DuringPositionEdit
        };

        for (S state : std::as_const(allStates)) {
            p.set(state);
            QCOMPARE(mgr.currentState(), state);
        }

        // 最初の Idle は今の状態と同じなので知らせない
        QCOMPARE(spy.count(), allStates.size() - 1);

        p.set(S::Idle);
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    // ================================================================
    // ファイルメニューのポリシーテスト
    // ================================================================

    /// FileNew: Idle のみ有効
    void fileNew_enabledOnlyInIdle()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::FileNew, "FileNew");
        verifyDisabled(p, S::DuringGame, E::FileNew, "FileNew");
        verifyDisabled(p, S::DuringAnalysis, E::FileNew, "FileNew");
        verifyDisabled(p, S::DuringCsaGame, E::FileNew, "FileNew");
        verifyDisabled(p, S::DuringTsumeSearch, E::FileNew, "FileNew");
        verifyDisabled(p, S::DuringConsideration, E::FileNew, "FileNew");
        verifyDisabled(p, S::DuringPositionEdit, E::FileNew, "FileNew");
    }

    /// FileOpen: Idle のみ有効
    void fileOpen_enabledOnlyInIdle()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::FileOpen, "FileOpen");
        verifyDisabled(p, S::DuringGame, E::FileOpen, "FileOpen");
        verifyDisabled(p, S::DuringAnalysis, E::FileOpen, "FileOpen");
    }

    /// FileSave: Idle, TsumeSearch, Consideration で有効
    void fileSave_enabledInIdleAndReadonlyStates()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::FileSave, "FileSave");
        verifyEnabled(p, S::DuringTsumeSearch, E::FileSave, "FileSave");
        verifyEnabled(p, S::DuringConsideration, E::FileSave, "FileSave");
        verifyDisabled(p, S::DuringGame, E::FileSave, "FileSave");
        verifyDisabled(p, S::DuringAnalysis, E::FileSave, "FileSave");
        verifyDisabled(p, S::DuringCsaGame, E::FileSave, "FileSave");
        verifyDisabled(p, S::DuringPositionEdit, E::FileSave, "FileSave");
    }

    /// FileSaveBoardImage: 常に有効
    void fileSaveBoardImage_alwaysEnabled()
    {
        Policy p;
        const QList<S> allStates = {
            S::Idle, S::DuringGame, S::DuringAnalysis,
            S::DuringCsaGame, S::DuringTsumeSearch,
            S::DuringConsideration, S::DuringPositionEdit
        };
        for (S state : std::as_const(allStates)) {
            verifyEnabled(p, state, E::FileSaveBoardImage, "FileSaveBoardImage");
        }
    }

    /// FileQuit: 常に有効
    void fileQuit_alwaysEnabled()
    {
        Policy p;
        const QList<S> allStates = {
            S::Idle, S::DuringGame, S::DuringAnalysis,
            S::DuringCsaGame, S::DuringTsumeSearch,
            S::DuringConsideration, S::DuringPositionEdit
        };
        for (S state : std::as_const(allStates)) {
            verifyEnabled(p, state, E::FileQuit, "FileQuit");
        }
    }

    // ================================================================
    // 編集メニューのポリシーテスト
    // ================================================================

    /// EditCopyKifu: Idle, TsumeSearch, Consideration で有効
    void editCopyKifu_enabledInReadonlyStates()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::EditCopyKifu, "EditCopyKifu");
        verifyEnabled(p, S::DuringTsumeSearch, E::EditCopyKifu, "EditCopyKifu");
        verifyEnabled(p, S::DuringConsideration, E::EditCopyKifu, "EditCopyKifu");
        verifyDisabled(p, S::DuringGame, E::EditCopyKifu, "EditCopyKifu");
        verifyDisabled(p, S::DuringAnalysis, E::EditCopyKifu, "EditCopyKifu");
    }

    /// EditCopyPosition: 常に有効
    void editCopyPosition_alwaysEnabled()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::EditCopyPosition, "EditCopyPosition");
        verifyEnabled(p, S::DuringGame, E::EditCopyPosition, "EditCopyPosition");
        verifyEnabled(p, S::DuringAnalysis, E::EditCopyPosition, "EditCopyPosition");
    }

    /// EditPasteKifu: Idle のみ有効
    void editPasteKifu_enabledOnlyInIdle()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::EditPasteKifu, "EditPasteKifu");
        verifyDisabled(p, S::DuringGame, E::EditPasteKifu, "EditPasteKifu");
        verifyDisabled(p, S::DuringConsideration, E::EditPasteKifu, "EditPasteKifu");
    }

    /// EditStartPosition: Idle のみ Shown、それ以外 Hidden
    void editStartPosition_shownOnlyInIdle()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::EditStartPosition, "EditStartPosition");
        verifyDisabled(p, S::DuringGame, E::EditStartPosition, "EditStartPosition");
        verifyDisabled(p, S::DuringPositionEdit, E::EditStartPosition, "EditStartPosition");
    }

    /// 局面編集系要素: DuringPositionEdit のみ Shown
    void positionEditElements_shownOnlyDuringEdit()
    {
        Policy p;
        const QList<E> editElements = {
            E::EditEndPosition, E::EditSetHirate, E::EditSetTsume,
            E::EditReturnAllToStand, E::EditChangeTurn
        };
        for (E elem : std::as_const(editElements)) {
            verifyEnabled(p, S::DuringPositionEdit, elem, "PositionEditElement");
            verifyDisabled(p, S::Idle, elem, "PositionEditElement");
            verifyDisabled(p, S::DuringGame, elem, "PositionEditElement");
        }
    }

    // ================================================================
    // 対局メニューのポリシーテスト
    // ================================================================

    /// GameStart: Idle のみ有効
    void gameStart_enabledOnlyInIdle()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::GameStart, "GameStart");
        verifyDisabled(p, S::DuringGame, E::GameStart, "GameStart");
        verifyDisabled(p, S::DuringCsaGame, E::GameStart, "GameStart");
    }

    /// GameResign: DuringGame, DuringCsaGame のみ有効
    void gameResign_enabledDuringGameOrCsa()
    {
        UiStatePolicyManager::Deps deps;
        deps.localGameHasHuman = []() { return true; };
        Policy p(deps);
        verifyEnabled(p, S::DuringGame, E::GameResign, "GameResign");
        verifyEnabled(p, S::DuringCsaGame, E::GameResign, "GameResign");
        verifyDisabled(p, S::Idle, E::GameResign, "GameResign");
        verifyDisabled(p, S::DuringAnalysis, E::GameResign, "GameResign");
        verifyDisabled(p, S::DuringTsumeSearch, E::GameResign, "GameResign");
    }

    void localHumanActions_followPlayers_data()
    {
        QTest::addColumn<bool>("hasHuman");
        QTest::newRow("with-human") << true;
        QTest::newRow("engines-only") << false;
    }

    /// 待った・ローカル対局の投了は人間が参加する場合のみ有効。中断は常に可能。
    void localHumanActions_followPlayers()
    {
        QFETCH(bool, hasHuman);
        QMainWindow window;
        Ui::MainWindow ui;
        ui.setupUi(&window);
        UiStatePolicyManager::Deps deps;
        deps.ui = &ui;
        {
            Policy noPlayers(deps);
            noPlayers.set(S::DuringGame);
            QVERIFY(!noPlayers.mgr.isEnabled(E::GameUndo));
            QVERIFY(!ui.actionUndoMove->isEnabled());
            QVERIFY(!noPlayers.mgr.isEnabled(E::GameResign));
            QVERIFY(!ui.actionResign->isEnabled());
        }

        deps.localGameHasHuman = [&hasHuman]() { return hasHuman; };
        Policy p(deps);
        UiStatePolicyManager& mgr = p.mgr;
        for (const auto state : {S::Idle, S::DuringGame, S::DuringCsaGame, S::DuringAnalysis,
                                S::DuringTsumeSearch, S::DuringConsideration,
                                S::DuringPositionEdit, S::Idle}) {
            p.set(state);
            const bool expected = state == S::DuringGame && hasHuman;
            QCOMPARE(mgr.isEnabled(E::GameUndo), expected);
            QCOMPARE(ui.actionUndoMove->isEnabled(), expected);
            const bool canResign = expected || state == S::DuringCsaGame;
            QCOMPARE(mgr.isEnabled(E::GameResign), canResign);
            QCOMPARE(ui.actionResign->isEnabled(), canResign);
            // 入玉宣言もエンジン同士の対局では行わない（エンジンが自分で宣言する）
            QCOMPARE(mgr.isEnabled(E::GameNyugyokuDeclaration), canResign);
            QCOMPARE(ui.actionNyugyokuDeclaration->isEnabled(), canResign);
            const bool canBreakOff = state == S::DuringGame || state == S::DuringCsaGame;
            QCOMPARE(mgr.isEnabled(E::GameBreakOff), canBreakOff);
            QCOMPARE(ui.actionBreakOffGame->isEnabled(), canBreakOff);
        }

        // 次の対局では最新の対局者で有効・無効を再判定する。
        hasHuman = true;
        p.set(S::DuringGame);
        QVERIFY(ui.actionUndoMove->isEnabled());
        QVERIFY(ui.actionResign->isEnabled());
        p.set(S::Idle);
        hasHuman = false;
        p.set(S::DuringGame);
        QVERIFY(!ui.actionUndoMove->isEnabled());
        QVERIFY(!ui.actionResign->isEnabled());
    }

    /// GameMakeImmediate: DuringGame, DuringCsaGame で有効
    void gameMakeImmediate_enabledDuringGameOrCsa()
    {
        Policy p;
        verifyEnabled(p, S::DuringGame, E::GameMakeImmediate, "GameMakeImmediate");
        verifyEnabled(p, S::DuringCsaGame, E::GameMakeImmediate, "GameMakeImmediate");
        verifyDisabled(p, S::Idle, E::GameMakeImmediate, "GameMakeImmediate");
    }

    /// GameBreakOff: DuringGame, DuringCsaGame で有効
    void gameBreakOff_enabledDuringGameOrCsa()
    {
        Policy p;
        verifyEnabled(p, S::DuringGame, E::GameBreakOff, "GameBreakOff");
        verifyEnabled(p, S::DuringCsaGame, E::GameBreakOff, "GameBreakOff");
        verifyDisabled(p, S::Idle, E::GameBreakOff, "GameBreakOff");
    }

    /// GameAnalyzeKifu: Idle のみ有効
    void gameAnalyzeKifu_enabledOnlyInIdle()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::GameAnalyzeKifu, "GameAnalyzeKifu");
        verifyDisabled(p, S::DuringGame, E::GameAnalyzeKifu, "GameAnalyzeKifu");
        verifyDisabled(p, S::DuringAnalysis, E::GameAnalyzeKifu, "GameAnalyzeKifu");
    }

    /// GameCancelAnalysis: DuringAnalysis のみ有効
    void gameCancelAnalysis_enabledOnlyDuringAnalysis()
    {
        Policy p;
        verifyEnabled(p, S::DuringAnalysis, E::GameCancelAnalysis, "GameCancelAnalysis");
        verifyDisabled(p, S::Idle, E::GameCancelAnalysis, "GameCancelAnalysis");
        verifyDisabled(p, S::DuringGame, E::GameCancelAnalysis, "GameCancelAnalysis");
    }

    /// GameTsumeSearch: Idle のみ有効
    void gameTsumeSearch_enabledOnlyInIdle()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::GameTsumeSearch, "GameTsumeSearch");
        verifyDisabled(p, S::DuringTsumeSearch, E::GameTsumeSearch, "GameTsumeSearch");
    }

    /// GameStopTsumeSearch: DuringTsumeSearch のみ有効
    void gameStopTsumeSearch_enabledOnlyDuringTsumeSearch()
    {
        Policy p;
        verifyEnabled(p, S::DuringTsumeSearch, E::GameStopTsumeSearch, "GameStopTsumeSearch");
        verifyDisabled(p, S::Idle, E::GameStopTsumeSearch, "GameStopTsumeSearch");
    }

    /// GameNyugyokuDeclaration: DuringGame（人間が参加する対局）, DuringCsaGame のみ有効
    void gameNyugyokuDeclaration_enabledDuringGameOrCsa()
    {
        UiStatePolicyManager::Deps deps;
        deps.localGameHasHuman = []() { return true; };
        Policy p(deps);
        verifyEnabled(p, S::DuringGame, E::GameNyugyokuDeclaration, "GameNyugyokuDeclaration");
        verifyEnabled(p, S::DuringCsaGame, E::GameNyugyokuDeclaration, "GameNyugyokuDeclaration");
        verifyDisabled(p, S::Idle, E::GameNyugyokuDeclaration, "GameNyugyokuDeclaration");
    }

    /// GameJishogiScore: 常に有効
    void gameJishogiScore_alwaysEnabled()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::GameJishogiScore, "GameJishogiScore");
        verifyEnabled(p, S::DuringGame, E::GameJishogiScore, "GameJishogiScore");
        verifyEnabled(p, S::DuringCsaGame, E::GameJishogiScore, "GameJishogiScore");
    }

    // ================================================================
    // 設定メニューのポリシーテスト
    // ================================================================

    /// SettingsEngine: Idle, DuringConsideration のみ有効
    void settingsEngine_enabledInIdleOrConsideration()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::SettingsEngine, "SettingsEngine");
        verifyEnabled(p, S::DuringConsideration, E::SettingsEngine, "SettingsEngine");
        verifyDisabled(p, S::DuringGame, E::SettingsEngine, "SettingsEngine");
        verifyDisabled(p, S::DuringAnalysis, E::SettingsEngine, "SettingsEngine");
        verifyDisabled(p, S::DuringCsaGame, E::SettingsEngine, "SettingsEngine");
    }

    // ================================================================
    // ウィジェットのポリシーテスト
    // ================================================================

    /// WidgetNavigation: Idle, TsumeSearch, Consideration で有効
    void widgetNavigation_enabledInReadonlyStates()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::WidgetNavigation, "WidgetNavigation");
        verifyEnabled(p, S::DuringTsumeSearch, E::WidgetNavigation, "WidgetNavigation");
        verifyEnabled(p, S::DuringConsideration, E::WidgetNavigation, "WidgetNavigation");
        verifyDisabled(p, S::DuringGame, E::WidgetNavigation, "WidgetNavigation");
        verifyDisabled(p, S::DuringAnalysis, E::WidgetNavigation, "WidgetNavigation");
        verifyDisabled(p, S::DuringCsaGame, E::WidgetNavigation, "WidgetNavigation");
        verifyDisabled(p, S::DuringPositionEdit, E::WidgetNavigation, "WidgetNavigation");
    }

    /// WidgetBranchTree: Idle, TsumeSearch, Consideration で有効
    void widgetBranchTree_enabledInReadonlyStates()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::WidgetBranchTree, "WidgetBranchTree");
        verifyEnabled(p, S::DuringTsumeSearch, E::WidgetBranchTree, "WidgetBranchTree");
        verifyEnabled(p, S::DuringConsideration, E::WidgetBranchTree, "WidgetBranchTree");
        verifyDisabled(p, S::DuringGame, E::WidgetBranchTree, "WidgetBranchTree");
        verifyDisabled(p, S::DuringAnalysis, E::WidgetBranchTree, "WidgetBranchTree");
    }

    /// WidgetConsiderationStart: 対局中/CSA中と局面編集中は無効
    void widgetConsiderationStart_disabledDuringGames()
    {
        Policy p;
        verifyEnabled(p, S::Idle, E::WidgetConsiderationStart, "WidgetConsiderationStart");
        verifyDisabled(p, S::DuringGame, E::WidgetConsiderationStart, "WidgetConsiderationStart");
        verifyDisabled(p, S::DuringCsaGame, E::WidgetConsiderationStart, "WidgetConsiderationStart");
        verifyEnabled(p, S::DuringAnalysis, E::WidgetConsiderationStart, "WidgetConsiderationStart");
        verifyEnabled(p, S::DuringTsumeSearch, E::WidgetConsiderationStart, "WidgetConsiderationStart");
        verifyEnabled(p, S::DuringConsideration, E::WidgetConsiderationStart, "WidgetConsiderationStart");
        verifyDisabled(p, S::DuringPositionEdit, E::WidgetConsiderationStart, "WidgetConsiderationStart");
    }

    // ================================================================
    // ポリシーテーブル網羅性テスト
    // ================================================================

    /// 全7状態で全UI要素にポリシーが定義されていること（isEnabled がクラッシュしない）
    void allStatesHavePoliciesForAllElements()
    {
        Policy p;
        UiStatePolicyManager& mgr = p.mgr;
        const QList<S> allStates = {
            S::Idle, S::DuringGame, S::DuringAnalysis,
            S::DuringCsaGame, S::DuringTsumeSearch,
            S::DuringConsideration, S::DuringPositionEdit
        };

        for (S state : std::as_const(allStates)) {
            p.set(state);
            // Count sentinel を除く全要素について isEnabled がクラッシュしないこと
            for (int e = 0; e < static_cast<int>(E::Count); ++e) {
                const auto element = static_cast<E>(e);
                // bool を返すこと（例外なし）
                [[maybe_unused]] bool enabled = mgr.isEnabled(element);
            }
        }
    }

    // ================================================================
    // 対称性テスト（開始→停止ペアの整合性）
    // ================================================================

    /// 解析開始/中止が排他的であること
    void analyzeAndCancelAreMutuallyExclusive()
    {
        Policy p;
        UiStatePolicyManager& mgr = p.mgr;

        p.set(S::Idle);
        QVERIFY(mgr.isEnabled(E::GameAnalyzeKifu));
        QVERIFY(!mgr.isEnabled(E::GameCancelAnalysis));

        p.set(S::DuringAnalysis);
        QVERIFY(!mgr.isEnabled(E::GameAnalyzeKifu));
        QVERIFY(mgr.isEnabled(E::GameCancelAnalysis));
    }

    /// 詰み探索開始/停止が排他的であること
    void tsumeSearchAndStopAreMutuallyExclusive()
    {
        Policy p;
        UiStatePolicyManager& mgr = p.mgr;

        p.set(S::Idle);
        QVERIFY(mgr.isEnabled(E::GameTsumeSearch));
        QVERIFY(!mgr.isEnabled(E::GameStopTsumeSearch));

        p.set(S::DuringTsumeSearch);
        QVERIFY(!mgr.isEnabled(E::GameTsumeSearch));
        QVERIFY(mgr.isEnabled(E::GameStopTsumeSearch));
    }

    /// 局面編集開始/終了が排他的であること
    void positionEditStartAndEndAreMutuallyExclusive()
    {
        Policy p;
        UiStatePolicyManager& mgr = p.mgr;

        p.set(S::Idle);
        QVERIFY(mgr.isEnabled(E::EditStartPosition));
        QVERIFY(!mgr.isEnabled(E::EditEndPosition));

        p.set(S::DuringPositionEdit);
        QVERIFY(!mgr.isEnabled(E::EditStartPosition));
        QVERIFY(mgr.isEnabled(E::EditEndPosition));
    }
};

QTEST_MAIN(TestAppUiStatePolicy)

#include "tst_app_ui_state_policy.moc"
