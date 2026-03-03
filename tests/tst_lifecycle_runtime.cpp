/// @file tst_lifecycle_runtime.cpp
/// @brief ライフサイクル runtime テスト
///
/// shutdown パスでのオブジェクト寿命安全性を実行時に検証する。
/// MatchRuntimeQueryService の Deps 無効化後もクラッシュせずデフォルト値を返すことを確認。

#include <QtTest>

#include "matchruntimequeryservice.h"
#include "playmodepolicyservice.h"
#include "timecontrolcontroller.h"

class TestLifecycleRuntime : public QObject
{
    Q_OBJECT

private slots:
    void testQueryServiceWithNullDeps();
    void testQueryServiceAfterDepsInvalidation();
    void testQueryServiceWithLiveDeps();
    void testQueryServiceSfenRecordWithNullMatch();
    void testPlayModePolicyServiceWithNullDeps();
    void testDoubleShutdownPattern();
};

/// Deps 未設定（デフォルト null）のまま全メソッドを呼び出してもクラッシュしないことを確認
void TestLifecycleRuntime::testQueryServiceWithNullDeps()
{
    MatchRuntimeQueryService service;

    QCOMPARE(service.isGameActivelyInProgress(), false);
    QCOMPARE(service.isHvH(), false);
    QCOMPARE(service.isHumanTurnNow(), true);
    QCOMPARE(service.isHumanSide(ShogiGameController::Player1), true);
    QCOMPARE(service.isHumanSide(ShogiGameController::Player2), true);

    QCOMPARE(service.getByoyomiMs(), static_cast<qint64>(0));
    QCOMPARE(service.getRemainingMsFor(MatchCoordinator::P1), static_cast<qint64>(0));
    QCOMPARE(service.getRemainingMsFor(MatchCoordinator::P2), static_cast<qint64>(0));
    QCOMPARE(service.getIncrementMsFor(MatchCoordinator::P1), static_cast<qint64>(0));
    QCOMPARE(service.getIncrementMsFor(MatchCoordinator::P2), static_cast<qint64>(0));

    QCOMPARE(service.sfenRecord(), nullptr);
}

/// 有効な Deps を設定した後に null Deps で無効化し、全メソッドが安全にデフォルト値を返すことを確認
void TestLifecycleRuntime::testQueryServiceAfterDepsInvalidation()
{
    MatchRuntimeQueryService service;
    PlayModePolicyService policy;
    TimeControlController timeCtrl;

    // 有効な Deps を設定
    MatchRuntimeQueryService::Deps liveDeps;
    liveDeps.playModePolicy = &policy;
    liveDeps.timeController = &timeCtrl;
    service.updateDeps(liveDeps);

    // null Deps で無効化（shutdown パスのシミュレーション）
    MatchRuntimeQueryService::Deps nullDeps;
    service.updateDeps(nullDeps);

    // 無効化後の呼び出しでクラッシュしないことを確認
    QCOMPARE(service.isGameActivelyInProgress(), false);
    QCOMPARE(service.isHvH(), false);
    QCOMPARE(service.isHumanTurnNow(), true);
    QCOMPARE(service.isHumanSide(ShogiGameController::Player1), true);

    QCOMPARE(service.getByoyomiMs(), static_cast<qint64>(0));
    QCOMPARE(service.getRemainingMsFor(MatchCoordinator::P1), static_cast<qint64>(0));
    QCOMPARE(service.getIncrementMsFor(MatchCoordinator::P2), static_cast<qint64>(0));

    QCOMPARE(service.sfenRecord(), nullptr);
}

/// 有効な Deps（PolicyService + TimeController）を設定した状態でクエリが正常動作することを確認
void TestLifecycleRuntime::testQueryServiceWithLiveDeps()
{
    MatchRuntimeQueryService service;
    PlayModePolicyService policy;
    TimeControlController timeCtrl;

    MatchRuntimeQueryService::Deps deps;
    deps.playModePolicy = &policy;
    deps.timeController = &timeCtrl;
    service.updateDeps(deps);

    // PolicyService の Deps が未設定なのでデフォルト動作
    QCOMPARE(service.isGameActivelyInProgress(), false);
    QCOMPARE(service.isHvH(), false);

    // TimeController は初期状態なので 0
    QCOMPARE(service.getByoyomiMs(), static_cast<qint64>(0));
    QCOMPARE(service.getRemainingMsFor(MatchCoordinator::P1), static_cast<qint64>(0));
}

/// sfenRecord() が match ダブルポインタ null 時に安全に nullptr を返すことを確認
void TestLifecycleRuntime::testQueryServiceSfenRecordWithNullMatch()
{
    MatchRuntimeQueryService service;

    // match ダブルポインタ未設定
    QCOMPARE(service.sfenRecord(), nullptr);
    QCOMPARE(std::as_const(service).sfenRecord(), nullptr);

    // match ダブルポインタはあるが中身が null
    MatchCoordinator* nullMatch = nullptr;
    MatchRuntimeQueryService::Deps deps;
    deps.match = &nullMatch;
    service.updateDeps(deps);

    QCOMPARE(service.sfenRecord(), nullptr);
    QCOMPARE(std::as_const(service).sfenRecord(), nullptr);
}

/// PlayModePolicyService の Deps 未設定時に全メソッドがクラッシュせずデフォルト値を返すことを確認
void TestLifecycleRuntime::testPlayModePolicyServiceWithNullDeps()
{
    PlayModePolicyService policy;

    QCOMPARE(policy.isHumanTurnNow(), true);
    QCOMPARE(policy.isHumanSide(ShogiGameController::Player1), true);
    QCOMPARE(policy.isHvH(), false);
    QCOMPARE(policy.isGameActivelyInProgress(), false);
}

/// 二重 shutdown パターンのシミュレーション: updateDeps(nullDeps) を2回呼んでもクラッシュしないことを確認
void TestLifecycleRuntime::testDoubleShutdownPattern()
{
    MatchRuntimeQueryService service;
    PlayModePolicyService policy;
    TimeControlController timeCtrl;

    // 有効な Deps を設定
    MatchRuntimeQueryService::Deps liveDeps;
    liveDeps.playModePolicy = &policy;
    liveDeps.timeController = &timeCtrl;
    service.updateDeps(liveDeps);

    // 1回目の shutdown
    MatchRuntimeQueryService::Deps nullDeps;
    service.updateDeps(nullDeps);

    // 2回目の shutdown（二重実行）
    service.updateDeps(nullDeps);

    // 二重無効化後もクラッシュしない
    QCOMPARE(service.isGameActivelyInProgress(), false);
    QCOMPARE(service.getByoyomiMs(), static_cast<qint64>(0));
    QCOMPARE(service.sfenRecord(), nullptr);
}

QTEST_MAIN(TestLifecycleRuntime)
#include "tst_lifecycle_runtime.moc"
