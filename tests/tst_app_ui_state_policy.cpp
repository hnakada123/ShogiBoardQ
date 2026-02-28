/// @file tst_app_ui_state_policy.cpp
/// @brief UiStatePolicyManager の状態遷移・ポリシーテーブル検証テスト
///
/// UiStatePolicyManager は deps が null でも安全に動作する設計のため、
/// 実インスタンスを生成してポリシーテーブルの正確性と状態遷移を検証する。

#include <QtTest>
#include <QSignalSpy>

#include "uistatepolicymanager.h"

class TestAppUiStatePolicy : public QObject
{
    Q_OBJECT

private:
    using S = UiStatePolicyManager::AppState;
    using E = UiStatePolicyManager::UiElement;

    /// 指定状態で指定要素が有効であることを検証するヘルパ
    void verifyEnabled(UiStatePolicyManager& mgr, S state, E element,
                       const char* elementName)
    {
        mgr.applyState(state);
        QVERIFY2(mgr.isEnabled(element),
                 qPrintable(QStringLiteral("%1 should be enabled in state %2")
                                .arg(QLatin1String(elementName))
                                .arg(static_cast<int>(state))));
    }

    /// 指定状態で指定要素が無効であることを検証するヘルパ
    void verifyDisabled(UiStatePolicyManager& mgr, S state, E element,
                        const char* elementName)
    {
        mgr.applyState(state);
        QVERIFY2(!mgr.isEnabled(element),
                 qPrintable(QStringLiteral("%1 should be disabled in state %2")
                                .arg(QLatin1String(elementName))
                                .arg(static_cast<int>(state))));
    }

private slots:
    // ================================================================
    // 初期状態テスト
    // ================================================================

    /// 初期状態が Idle であること
    void initialStateIsIdle()
    {
        UiStatePolicyManager mgr;
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    // ================================================================
    // 状態遷移スロットテスト
    // ================================================================

    /// transitionToIdle() が状態を Idle に遷移させること
    void transitionToIdleSlot()
    {
        UiStatePolicyManager mgr;
        mgr.applyState(S::DuringGame);
        mgr.transitionToIdle();
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    /// transitionToDuringGame() が状態を DuringGame に遷移させること
    void transitionToDuringGameSlot()
    {
        UiStatePolicyManager mgr;
        mgr.transitionToDuringGame();
        QCOMPARE(mgr.currentState(), S::DuringGame);
    }

    /// transitionToDuringAnalysis() が状態を DuringAnalysis に遷移させること
    void transitionToDuringAnalysisSlot()
    {
        UiStatePolicyManager mgr;
        mgr.transitionToDuringAnalysis();
        QCOMPARE(mgr.currentState(), S::DuringAnalysis);
    }

    /// transitionToDuringCsaGame() が状態を DuringCsaGame に遷移させること
    void transitionToDuringCsaGameSlot()
    {
        UiStatePolicyManager mgr;
        mgr.transitionToDuringCsaGame();
        QCOMPARE(mgr.currentState(), S::DuringCsaGame);
    }

    /// transitionToDuringTsumeSearch() が状態を DuringTsumeSearch に遷移させること
    void transitionToDuringTsumeSearchSlot()
    {
        UiStatePolicyManager mgr;
        mgr.transitionToDuringTsumeSearch();
        QCOMPARE(mgr.currentState(), S::DuringTsumeSearch);
    }

    /// transitionToDuringConsideration() が状態を DuringConsideration に遷移させること
    void transitionToDuringConsiderationSlot()
    {
        UiStatePolicyManager mgr;
        mgr.transitionToDuringConsideration();
        QCOMPARE(mgr.currentState(), S::DuringConsideration);
    }

    /// transitionToDuringPositionEdit() が状態を DuringPositionEdit に遷移させること
    void transitionToDuringPositionEditSlot()
    {
        UiStatePolicyManager mgr;
        mgr.transitionToDuringPositionEdit();
        QCOMPARE(mgr.currentState(), S::DuringPositionEdit);
    }

    // ================================================================
    // stateChanged シグナルテスト
    // ================================================================

    /// applyState() が stateChanged シグナルを発火すること
    void applyStateEmitsStateChanged()
    {
        UiStatePolicyManager mgr;
        QSignalSpy spy(&mgr, &UiStatePolicyManager::stateChanged);
        QVERIFY(spy.isValid());

        mgr.applyState(S::DuringGame);
        QCOMPARE(spy.count(), 1);

        const auto args = spy.takeFirst();
        QCOMPARE(args.at(0).value<UiStatePolicyManager::AppState>(), S::DuringGame);
    }

    /// 遷移スロットも stateChanged シグナルを発火すること
    void transitionSlotsEmitStateChanged()
    {
        UiStatePolicyManager mgr;
        QSignalSpy spy(&mgr, &UiStatePolicyManager::stateChanged);
        QVERIFY(spy.isValid());

        mgr.transitionToDuringAnalysis();
        QCOMPARE(spy.count(), 1);

        mgr.transitionToIdle();
        QCOMPARE(spy.count(), 2);
    }

    // ================================================================
    // 遷移サイクルテスト
    // ================================================================

    /// Idle → DuringGame → Idle の遷移サイクルが正しく動作すること
    void idleGameIdleCycle()
    {
        UiStatePolicyManager mgr;
        QCOMPARE(mgr.currentState(), S::Idle);

        mgr.transitionToDuringGame();
        QCOMPARE(mgr.currentState(), S::DuringGame);

        mgr.transitionToIdle();
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    /// Idle → DuringAnalysis → Idle の遷移サイクルが正しく動作すること
    void idleAnalysisIdleCycle()
    {
        UiStatePolicyManager mgr;
        QCOMPARE(mgr.currentState(), S::Idle);

        mgr.transitionToDuringAnalysis();
        QCOMPARE(mgr.currentState(), S::DuringAnalysis);

        mgr.transitionToIdle();
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    /// 全7状態を順に遷移して戻れること
    void fullStateCycle()
    {
        UiStatePolicyManager mgr;
        QSignalSpy spy(&mgr, &UiStatePolicyManager::stateChanged);

        const QList<S> allStates = {
            S::Idle, S::DuringGame, S::DuringAnalysis,
            S::DuringCsaGame, S::DuringTsumeSearch,
            S::DuringConsideration, S::DuringPositionEdit
        };

        for (S state : std::as_const(allStates)) {
            mgr.applyState(state);
            QCOMPARE(mgr.currentState(), state);
        }

        QCOMPARE(spy.count(), allStates.size());

        mgr.transitionToIdle();
        QCOMPARE(mgr.currentState(), S::Idle);
    }

    // ================================================================
    // ファイルメニューのポリシーテスト
    // ================================================================

    /// FileNew: Idle のみ有効
    void fileNew_enabledOnlyInIdle()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::FileNew, "FileNew");
        verifyDisabled(mgr, S::DuringGame, E::FileNew, "FileNew");
        verifyDisabled(mgr, S::DuringAnalysis, E::FileNew, "FileNew");
        verifyDisabled(mgr, S::DuringCsaGame, E::FileNew, "FileNew");
        verifyDisabled(mgr, S::DuringTsumeSearch, E::FileNew, "FileNew");
        verifyDisabled(mgr, S::DuringConsideration, E::FileNew, "FileNew");
        verifyDisabled(mgr, S::DuringPositionEdit, E::FileNew, "FileNew");
    }

    /// FileOpen: Idle のみ有効
    void fileOpen_enabledOnlyInIdle()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::FileOpen, "FileOpen");
        verifyDisabled(mgr, S::DuringGame, E::FileOpen, "FileOpen");
        verifyDisabled(mgr, S::DuringAnalysis, E::FileOpen, "FileOpen");
    }

    /// FileSave: Idle, TsumeSearch, Consideration で有効
    void fileSave_enabledInIdleAndReadonlyStates()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::FileSave, "FileSave");
        verifyEnabled(mgr, S::DuringTsumeSearch, E::FileSave, "FileSave");
        verifyEnabled(mgr, S::DuringConsideration, E::FileSave, "FileSave");
        verifyDisabled(mgr, S::DuringGame, E::FileSave, "FileSave");
        verifyDisabled(mgr, S::DuringAnalysis, E::FileSave, "FileSave");
        verifyDisabled(mgr, S::DuringCsaGame, E::FileSave, "FileSave");
        verifyDisabled(mgr, S::DuringPositionEdit, E::FileSave, "FileSave");
    }

    /// FileSaveBoardImage: 常に有効
    void fileSaveBoardImage_alwaysEnabled()
    {
        UiStatePolicyManager mgr;
        const QList<S> allStates = {
            S::Idle, S::DuringGame, S::DuringAnalysis,
            S::DuringCsaGame, S::DuringTsumeSearch,
            S::DuringConsideration, S::DuringPositionEdit
        };
        for (S state : std::as_const(allStates)) {
            verifyEnabled(mgr, state, E::FileSaveBoardImage, "FileSaveBoardImage");
        }
    }

    /// FileQuit: 常に有効
    void fileQuit_alwaysEnabled()
    {
        UiStatePolicyManager mgr;
        const QList<S> allStates = {
            S::Idle, S::DuringGame, S::DuringAnalysis,
            S::DuringCsaGame, S::DuringTsumeSearch,
            S::DuringConsideration, S::DuringPositionEdit
        };
        for (S state : std::as_const(allStates)) {
            verifyEnabled(mgr, state, E::FileQuit, "FileQuit");
        }
    }

    // ================================================================
    // 編集メニューのポリシーテスト
    // ================================================================

    /// EditCopyKifu: Idle, TsumeSearch, Consideration で有効
    void editCopyKifu_enabledInReadonlyStates()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::EditCopyKifu, "EditCopyKifu");
        verifyEnabled(mgr, S::DuringTsumeSearch, E::EditCopyKifu, "EditCopyKifu");
        verifyEnabled(mgr, S::DuringConsideration, E::EditCopyKifu, "EditCopyKifu");
        verifyDisabled(mgr, S::DuringGame, E::EditCopyKifu, "EditCopyKifu");
        verifyDisabled(mgr, S::DuringAnalysis, E::EditCopyKifu, "EditCopyKifu");
    }

    /// EditCopyPosition: 常に有効
    void editCopyPosition_alwaysEnabled()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::EditCopyPosition, "EditCopyPosition");
        verifyEnabled(mgr, S::DuringGame, E::EditCopyPosition, "EditCopyPosition");
        verifyEnabled(mgr, S::DuringAnalysis, E::EditCopyPosition, "EditCopyPosition");
    }

    /// EditPasteKifu: Idle のみ有効
    void editPasteKifu_enabledOnlyInIdle()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::EditPasteKifu, "EditPasteKifu");
        verifyDisabled(mgr, S::DuringGame, E::EditPasteKifu, "EditPasteKifu");
        verifyDisabled(mgr, S::DuringConsideration, E::EditPasteKifu, "EditPasteKifu");
    }

    /// EditStartPosition: Idle のみ Shown、それ以外 Hidden
    void editStartPosition_shownOnlyInIdle()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::EditStartPosition, "EditStartPosition");
        verifyDisabled(mgr, S::DuringGame, E::EditStartPosition, "EditStartPosition");
        verifyDisabled(mgr, S::DuringPositionEdit, E::EditStartPosition, "EditStartPosition");
    }

    /// 局面編集系要素: DuringPositionEdit のみ Shown
    void positionEditElements_shownOnlyDuringEdit()
    {
        UiStatePolicyManager mgr;
        const QList<E> editElements = {
            E::EditEndPosition, E::EditSetHirate, E::EditSetTsume,
            E::EditReturnAllToStand, E::EditChangeTurn
        };
        for (E elem : std::as_const(editElements)) {
            verifyEnabled(mgr, S::DuringPositionEdit, elem, "PositionEditElement");
            verifyDisabled(mgr, S::Idle, elem, "PositionEditElement");
            verifyDisabled(mgr, S::DuringGame, elem, "PositionEditElement");
        }
    }

    // ================================================================
    // 対局メニューのポリシーテスト
    // ================================================================

    /// GameStart: Idle のみ有効
    void gameStart_enabledOnlyInIdle()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::GameStart, "GameStart");
        verifyDisabled(mgr, S::DuringGame, E::GameStart, "GameStart");
        verifyDisabled(mgr, S::DuringCsaGame, E::GameStart, "GameStart");
    }

    /// GameResign: DuringGame, DuringCsaGame のみ有効
    void gameResign_enabledDuringGameOrCsa()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::DuringGame, E::GameResign, "GameResign");
        verifyEnabled(mgr, S::DuringCsaGame, E::GameResign, "GameResign");
        verifyDisabled(mgr, S::Idle, E::GameResign, "GameResign");
        verifyDisabled(mgr, S::DuringAnalysis, E::GameResign, "GameResign");
        verifyDisabled(mgr, S::DuringTsumeSearch, E::GameResign, "GameResign");
    }

    /// GameUndo: DuringGame のみ有効（CSA中は不可）
    void gameUndo_enabledOnlyDuringGame()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::DuringGame, E::GameUndo, "GameUndo");
        verifyDisabled(mgr, S::DuringCsaGame, E::GameUndo, "GameUndo");
        verifyDisabled(mgr, S::Idle, E::GameUndo, "GameUndo");
    }

    /// GameMakeImmediate: DuringGame, DuringCsaGame で有効
    void gameMakeImmediate_enabledDuringGameOrCsa()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::DuringGame, E::GameMakeImmediate, "GameMakeImmediate");
        verifyEnabled(mgr, S::DuringCsaGame, E::GameMakeImmediate, "GameMakeImmediate");
        verifyDisabled(mgr, S::Idle, E::GameMakeImmediate, "GameMakeImmediate");
    }

    /// GameBreakOff: DuringGame, DuringCsaGame で有効
    void gameBreakOff_enabledDuringGameOrCsa()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::DuringGame, E::GameBreakOff, "GameBreakOff");
        verifyEnabled(mgr, S::DuringCsaGame, E::GameBreakOff, "GameBreakOff");
        verifyDisabled(mgr, S::Idle, E::GameBreakOff, "GameBreakOff");
    }

    /// GameAnalyzeKifu: Idle のみ有効
    void gameAnalyzeKifu_enabledOnlyInIdle()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::GameAnalyzeKifu, "GameAnalyzeKifu");
        verifyDisabled(mgr, S::DuringGame, E::GameAnalyzeKifu, "GameAnalyzeKifu");
        verifyDisabled(mgr, S::DuringAnalysis, E::GameAnalyzeKifu, "GameAnalyzeKifu");
    }

    /// GameCancelAnalysis: DuringAnalysis のみ有効
    void gameCancelAnalysis_enabledOnlyDuringAnalysis()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::DuringAnalysis, E::GameCancelAnalysis, "GameCancelAnalysis");
        verifyDisabled(mgr, S::Idle, E::GameCancelAnalysis, "GameCancelAnalysis");
        verifyDisabled(mgr, S::DuringGame, E::GameCancelAnalysis, "GameCancelAnalysis");
    }

    /// GameTsumeSearch: Idle のみ有効
    void gameTsumeSearch_enabledOnlyInIdle()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::GameTsumeSearch, "GameTsumeSearch");
        verifyDisabled(mgr, S::DuringTsumeSearch, E::GameTsumeSearch, "GameTsumeSearch");
    }

    /// GameStopTsumeSearch: DuringTsumeSearch のみ有効
    void gameStopTsumeSearch_enabledOnlyDuringTsumeSearch()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::DuringTsumeSearch, E::GameStopTsumeSearch, "GameStopTsumeSearch");
        verifyDisabled(mgr, S::Idle, E::GameStopTsumeSearch, "GameStopTsumeSearch");
    }

    /// GameNyugyokuDeclaration: DuringGame, DuringCsaGame のみ有効
    void gameNyugyokuDeclaration_enabledDuringGameOrCsa()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::DuringGame, E::GameNyugyokuDeclaration, "GameNyugyokuDeclaration");
        verifyEnabled(mgr, S::DuringCsaGame, E::GameNyugyokuDeclaration, "GameNyugyokuDeclaration");
        verifyDisabled(mgr, S::Idle, E::GameNyugyokuDeclaration, "GameNyugyokuDeclaration");
    }

    /// GameJishogiScore: 常に有効
    void gameJishogiScore_alwaysEnabled()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::GameJishogiScore, "GameJishogiScore");
        verifyEnabled(mgr, S::DuringGame, E::GameJishogiScore, "GameJishogiScore");
        verifyEnabled(mgr, S::DuringCsaGame, E::GameJishogiScore, "GameJishogiScore");
    }

    // ================================================================
    // 設定メニューのポリシーテスト
    // ================================================================

    /// SettingsEngine: Idle, DuringConsideration のみ有効
    void settingsEngine_enabledInIdleOrConsideration()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::SettingsEngine, "SettingsEngine");
        verifyEnabled(mgr, S::DuringConsideration, E::SettingsEngine, "SettingsEngine");
        verifyDisabled(mgr, S::DuringGame, E::SettingsEngine, "SettingsEngine");
        verifyDisabled(mgr, S::DuringAnalysis, E::SettingsEngine, "SettingsEngine");
        verifyDisabled(mgr, S::DuringCsaGame, E::SettingsEngine, "SettingsEngine");
    }

    // ================================================================
    // ウィジェットのポリシーテスト
    // ================================================================

    /// WidgetNavigation: Idle, TsumeSearch, Consideration で有効
    void widgetNavigation_enabledInReadonlyStates()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::WidgetNavigation, "WidgetNavigation");
        verifyEnabled(mgr, S::DuringTsumeSearch, E::WidgetNavigation, "WidgetNavigation");
        verifyEnabled(mgr, S::DuringConsideration, E::WidgetNavigation, "WidgetNavigation");
        verifyDisabled(mgr, S::DuringGame, E::WidgetNavigation, "WidgetNavigation");
        verifyDisabled(mgr, S::DuringAnalysis, E::WidgetNavigation, "WidgetNavigation");
        verifyDisabled(mgr, S::DuringCsaGame, E::WidgetNavigation, "WidgetNavigation");
        verifyDisabled(mgr, S::DuringPositionEdit, E::WidgetNavigation, "WidgetNavigation");
    }

    /// WidgetBranchTree: Idle, TsumeSearch, Consideration で有効
    void widgetBranchTree_enabledInReadonlyStates()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::WidgetBranchTree, "WidgetBranchTree");
        verifyEnabled(mgr, S::DuringTsumeSearch, E::WidgetBranchTree, "WidgetBranchTree");
        verifyEnabled(mgr, S::DuringConsideration, E::WidgetBranchTree, "WidgetBranchTree");
        verifyDisabled(mgr, S::DuringGame, E::WidgetBranchTree, "WidgetBranchTree");
        verifyDisabled(mgr, S::DuringAnalysis, E::WidgetBranchTree, "WidgetBranchTree");
    }

    /// WidgetBoardClick: Idle, DuringGame, DuringCsaGame で有効
    void widgetBoardClick_enabledForInteraction()
    {
        UiStatePolicyManager mgr;
        verifyEnabled(mgr, S::Idle, E::WidgetBoardClick, "WidgetBoardClick");
        verifyEnabled(mgr, S::DuringGame, E::WidgetBoardClick, "WidgetBoardClick");
        verifyEnabled(mgr, S::DuringCsaGame, E::WidgetBoardClick, "WidgetBoardClick");
        verifyDisabled(mgr, S::DuringAnalysis, E::WidgetBoardClick, "WidgetBoardClick");
        verifyDisabled(mgr, S::DuringTsumeSearch, E::WidgetBoardClick, "WidgetBoardClick");
        verifyDisabled(mgr, S::DuringConsideration, E::WidgetBoardClick, "WidgetBoardClick");
        verifyDisabled(mgr, S::DuringPositionEdit, E::WidgetBoardClick, "WidgetBoardClick");
    }

    // ================================================================
    // ポリシーテーブル網羅性テスト
    // ================================================================

    /// 全7状態で全UI要素にポリシーが定義されていること（isEnabled がクラッシュしない）
    void allStatesHavePoliciesForAllElements()
    {
        UiStatePolicyManager mgr;
        const QList<S> allStates = {
            S::Idle, S::DuringGame, S::DuringAnalysis,
            S::DuringCsaGame, S::DuringTsumeSearch,
            S::DuringConsideration, S::DuringPositionEdit
        };

        for (S state : std::as_const(allStates)) {
            mgr.applyState(state);
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
        UiStatePolicyManager mgr;

        mgr.applyState(S::Idle);
        QVERIFY(mgr.isEnabled(E::GameAnalyzeKifu));
        QVERIFY(!mgr.isEnabled(E::GameCancelAnalysis));

        mgr.applyState(S::DuringAnalysis);
        QVERIFY(!mgr.isEnabled(E::GameAnalyzeKifu));
        QVERIFY(mgr.isEnabled(E::GameCancelAnalysis));
    }

    /// 詰み探索開始/停止が排他的であること
    void tsumeSearchAndStopAreMutuallyExclusive()
    {
        UiStatePolicyManager mgr;

        mgr.applyState(S::Idle);
        QVERIFY(mgr.isEnabled(E::GameTsumeSearch));
        QVERIFY(!mgr.isEnabled(E::GameStopTsumeSearch));

        mgr.applyState(S::DuringTsumeSearch);
        QVERIFY(!mgr.isEnabled(E::GameTsumeSearch));
        QVERIFY(mgr.isEnabled(E::GameStopTsumeSearch));
    }

    /// 局面編集開始/終了が排他的であること
    void positionEditStartAndEndAreMutuallyExclusive()
    {
        UiStatePolicyManager mgr;

        mgr.applyState(S::Idle);
        QVERIFY(mgr.isEnabled(E::EditStartPosition));
        QVERIFY(!mgr.isEnabled(E::EditEndPosition));

        mgr.applyState(S::DuringPositionEdit);
        QVERIFY(!mgr.isEnabled(E::EditStartPosition));
        QVERIFY(mgr.isEnabled(E::EditEndPosition));
    }
};

QTEST_MAIN(TestAppUiStatePolicy)

#include "tst_app_ui_state_policy.moc"
