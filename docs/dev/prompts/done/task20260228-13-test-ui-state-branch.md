# Task 13: UI状態遷移/分岐ナビテスト追加（テーマD: app層テスト補完）

## 目的

UI状態遷移（UiStatePolicyManager）と分岐ナビゲーション更新フロー（BranchNavigationWiring）のテストを追加する。

## 背景

- `UiStatePolicyManager` は7つのAppState間の遷移を状態テーブルで管理
- `BranchNavigationWiring` は分岐ナビゲーションの配線クラス
- いずれも結線変更時の回帰リスクが高い
- テーマD（app層テスト補完）の残り作業

## 事前調査

### Step 1: UiStatePolicyManagerの確認

```bash
cat src/ui/coordinators/uistatepolicymanager.h
cat src/ui/coordinators/uistatepolicymanager.cpp
```

7つの状態:
1. Idle
2. DuringGame
3. DuringAnalysis
4. DuringCsaGame
5. DuringTsumeSearch
6. DuringConsideration
7. DuringPositionEdit

### Step 2: BranchNavigationWiringの確認

```bash
cat src/ui/wiring/branchnavigationwiring.h
cat src/ui/wiring/branchnavigationwiring.cpp
```

## 実装手順

### Step 3: UI状態遷移テスト

`tests/tst_app_ui_state_policy.cpp` を作成:

テスト項目:
1. 各状態遷移が正しいUI要素の有効/無効を設定する
2. Idle → DuringGame → Idle の遷移サイクル
3. Idle → DuringAnalysis → Idle の遷移サイクル
4. 不正な状態遷移の防止（あれば）
5. 各状態でのメニュー/ボタン有効状態の検証

### Step 4: 分岐ナビゲーションテスト

`tests/tst_app_branch_navigation.cpp` を作成:

テスト項目:
1. 分岐ノード選択時のシグナル伝播
2. 分岐ツリー更新時のUI同期
3. 分岐削除時のハンドリング

### Step 5: CMakeLists.txt更新・ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] UI状態遷移テストが追加されている
- [ ] 分岐ナビゲーションテストが追加されている
- [ ] 全テスト通過
- [ ] app層テスト合計6本以上達成（Task 11-13合計）

## KPI変化目標

- テスト数: +2（UI状態1本 + 分岐ナビ1本）
- app層テスト合計: 6本以上（目標達成）

## 注意事項

- `UiStatePolicyManager` はシグナル入力で状態遷移するため、テストではシグナルを `emit` して検証する
- UI要素のスタブが必要（メニューバー、ツールバー等のモック）
- `connect()` でラムダを使わない
