# Task 20260303-04: ライフサイクル runtime テスト追加

## 概要
現状はソース文字列解析による「構造検証」テスト比率が高い。実行時の寿命バグや queued event 問題を検出するための runtime ライフサイクルテストを追加する。

## 優先度
High

## 背景
- `tst_app_lifecycle_pipeline.cpp` と `tst_lifecycle_scenario.cpp` はソースファイルを読み込んで文字列パターンマッチする構造検証方式
- 実際のオブジェクト生成・破棄・イベント処理パスは通らない
- shutdown 後の queued event によるクラッシュ等の実行時バグを検出できない
- 3.1（callback 安全化）、3.2（closeEvent 順序修正）の効果を実行時に検証するテストが必要

## 対象ファイル

### 新規作成
1. `tests/tst_lifecycle_runtime.cpp` - runtime ライフサイクルテスト

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

### 参照（変更不要だが理解が必要）
- `tests/tst_app_lifecycle_pipeline.cpp` - 既存テスト構造の確認
- `tests/tst_lifecycle_scenario.cpp` - 既存テスト構造の確認
- `src/app/mainwindowlifecyclepipeline.h/.cpp` - テスト対象
- `src/app/matchruntimequeryservice.h/.cpp` - shutdown 安全性の確認対象

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_lifecycle_runtime.cpp` を新規作成する。

テストケース方針:
1. **MatchRuntimeQueryService の Deps 無効化テスト**: Deps を無効化した後にクエリしてもクラッシュしないことを確認
2. **queryService null 時の中継メソッドテスト**: Task 01 で追加した中継メソッドが null ガードで安全にデフォルト値を返すことを確認
3. **二重 shutdown テスト**: `runShutdown()` を2回呼んでもクラッシュしないことを確認

テスト実装の方針:
- `MatchRuntimeQueryService` は非QObject なので単体でインスタンス化可能
- 中継メソッドのテストには `MainWindow` のスタブが必要な場合、`MatchRuntimeQueryService` 単体テストに絞る
- `MainWindow` を実際にインスタンス化するテストは、依存が重すぎる場合はスキップして `MatchRuntimeQueryService` の Deps 無効化テストに集中する

テンプレート:
```cpp
/// @file tst_lifecycle_runtime.cpp
/// @brief ライフサイクル runtime テスト
///
/// shutdown パスでのオブジェクト寿命安全性を実行時に検証する。

#include <QtTest>

#include "matchruntimequeryservice.h"

class TestLifecycleRuntime : public QObject
{
    Q_OBJECT

private slots:
    void testQueryServiceWithNullDeps();
    void testQueryServiceAfterDepsInvalidation();
};

void TestLifecycleRuntime::testQueryServiceWithNullDeps()
{
    MatchRuntimeQueryService service;
    // Deps 未設定（デフォルト null）のまま呼び出し
    // クラッシュしないことが成功条件
    QCOMPARE(service.isGameActivelyInProgress(), false);
    QCOMPARE(service.getByoyomiMs(), 0);
}

void TestLifecycleRuntime::testQueryServiceAfterDepsInvalidation()
{
    MatchRuntimeQueryService service;

    // 一度有効な Deps を設定（必要に応じてモックを用意）
    // その後 null Deps で無効化
    MatchRuntimeQueryService::Deps nullDeps;
    service.updateDeps(nullDeps);

    // 無効化後の呼び出しでクラッシュしないことを確認
    QCOMPARE(service.isGameActivelyInProgress(), false);
    QCOMPARE(service.getByoyomiMs(), 0);
    QCOMPARE(service.getRemainingMsFor(MatchCoordinator::Player::Player1), 0);
    QCOMPARE(service.getIncrementMsFor(MatchCoordinator::Player::Player1), 0);
}

QTEST_MAIN(TestLifecycleRuntime)
#include "tst_lifecycle_runtime.moc"
```

注意: `MatchRuntimeQueryService` のメソッドシグネチャと戻り値型は実際のヘッダーを確認して合わせること。`getRemainingMsFor` 等の引数の型（`MatchCoordinator::Player`）も正確に合わせる。

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加（既存の `add_shogi_test` マクロを使用）:

```cmake
add_shogi_test(tst_lifecycle_runtime
    tst_lifecycle_runtime.cpp
    ${TEST_STUBS}
    ${SRC}/app/matchruntimequeryservice.cpp
)
```

必要なインクルードパスと依存ソースは、コンパイルエラーに応じて追加する。`MatchRuntimeQueryService` が依存する型（`MatchCoordinator::Player` 等）のヘッダーが必要。

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_lifecycle_runtime
```

コンパイルエラーが出た場合、不足している依存ソースやスタブを `CMakeLists.txt` に追加する。

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- runtime ライフサイクルテストが1本以上追加される
- shutdown パスでのクラッシュ防止が実行時に検証される
- ビルド成功
- `ctest --test-dir build --output-on-failure` 全件成功

## 補足
- 将来的に ASan（AddressSanitizer）ビルドの CI ジョブを追加することで、Use-after-free を自動検出できるようになる
- ASan は nightly/optional ジョブとして追加が望ましい（ビルド時間が長いため）
