# Task 04: app層異常系テスト追加

## 概要

app層（初期化・ライフサイクル・配線）の異常系テストを追加し、回帰検知を厚くする。
初期化失敗、依存未生成、二重終了の3パターンを最低限カバーする。

## 前提条件

- なし（他タスクと並行可能）

## 現状

- 既存テストファイル:
  - `tests/tst_app_lifecycle_pipeline.cpp` — 起動/終了フローの順序検証
  - `tests/tst_wiring_contracts.cpp` — MatchCoordinatorWiring の結線契約
  - `tests/tst_app_kifu_load.cpp` — KifuFileController の契約
  - `tests/tst_app_branch_navigation.cpp` — 分岐ナビゲーション
  - `tests/tst_preset_gamestart_cleanup.cpp` — 対局開始クリーンアップ
- テスト総数: 50件（全パス）
- 異常系テストは薄い

## 実施内容

### Step 1: テスト対象の特定

以下の異常系シナリオを分析し、テスト可能なものを特定:

1. **初期化失敗系**
   - ServiceRegistry の ensure* 呼び出し順序が不正な場合
   - 依存オブジェクトが nullptr のまま操作された場合
   - FoundationRegistry が未初期化の場合

2. **二重実行系**
   - `MainWindowLifecyclePipeline::runShutdown()` の二重呼び出し
   - `ensureLiveGameSessionStarted()` の二重呼び出し
   - MatchCoordinator の二重初期化

3. **依存未生成系**
   - Wiring クラスの `updateDeps()` 前にメソッド呼び出し
   - GameSessionOrchestrator のダブルポインタが nullptr の場合

### Step 2: テストファイル作成

1. `tests/tst_app_error_handling.cpp` を新規作成
2. 各異常系シナリオについて、最低3件のテストケースを実装
3. テスト内では実際のクラスをインスタンス化するか、必要に応じてスタブを使用

### Step 3: CMake登録

1. `tests/CMakeLists.txt` にテストを追加
2. ビルド・テスト実行で全件パスを確認

## テストケース例

```cpp
void TestAppErrorHandling::shutdownDoubleCall()
{
    // runShutdown() を2回呼んでもクラッシュしないことを確認
    // 二重実行防止フラグが機能していること
}

void TestAppErrorHandling::ensureWithNullDependency()
{
    // 依存が nullptr の状態で ensure* を呼んだ場合の挙動
    // クラッシュせず、適切に early return されること
}
```

## 完了条件

- 異常系テスト 3件以上追加
- 全テスト通過（既存50件 + 新規）
- ビルド成功

## 注意事項

- テストは `QT_QPA_PLATFORM=offscreen` で実行可能であること
- GUI ウィジェットの生成が必要な場合は `QTest::qWaitForWindowExposed` 等を使用
- 既存テストのパターン（ソースコード解析型 vs 実インスタンス型）を踏襲する
