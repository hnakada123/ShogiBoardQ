/// @file tst_lifecycle_runtime.cpp
/// @brief ライフサイクル runtime テスト
///
/// shutdown パスでのオブジェクト寿命安全性を実行時に検証する。
/// OperationPolicy の Deps が無いときも安全側の既定値を返すことを確認（終了処理の二重実行は tst_app_lifecycle_pipeline）。

#include <QtTest>

#include "operationpolicy.h"

class TestLifecycleRuntime : public QObject
{
    Q_OBJECT

private slots:
    void testPlayModePolicyServiceWithNullDeps();
};

/// OperationPolicy の Deps 未設定時に全メソッドがクラッシュせずデフォルト値を返すことを確認
void TestLifecycleRuntime::testPlayModePolicyServiceWithNullDeps()
{
    OperationPolicy policy({});

    QCOMPARE(policy.isHumanTurnNow(), false);
    QCOMPARE(policy.isGameActivelyInProgress(), false);
}

QTEST_MAIN(TestLifecycleRuntime)
#include "tst_lifecycle_runtime.moc"
