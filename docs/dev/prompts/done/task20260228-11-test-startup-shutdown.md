# Task 11: 起動/終了フローテスト追加（テーマD: app層テスト補完）

## 目的

app層の起動パイプラインと終了パイプラインのシナリオテストを追加する。

## 背景

- 現在41テストあるが、app層のオーケストレーション検証は薄い
- `MainWindowLifecyclePipeline`（runStartup: 8段階、runShutdown）の振る舞いテストが不足
- `MainWindowUiBootstrapper` のUI初期化ステップも未テスト
- テーマD（app層テスト補完）の最初の作業

## 事前調査

### Step 1: 起動/終了フローの確認

```bash
# LifecyclePipeline
cat src/app/mainwindowlifecyclepipeline.h
cat src/app/mainwindowlifecyclepipeline.cpp

# UiBootstrapper
cat src/app/mainwindowuibootstrapper.h
cat src/app/mainwindowuibootstrapper.cpp

# 既存テストのスタブパターン
ls tests/test_stubs*.cpp tests/test_stubs*.h 2>/dev/null
rg "class.*Stub|class.*Mock" tests/ -l
```

### Step 2: テスト可能な契約の特定

起動/終了フローで検証すべき契約:
1. **起動**: 各ステップが正しい順序で呼ばれる
2. **終了**: 二重実行防止が機能する
3. **終了**: リソースが正しく解放される

## 実装手順

### Step 3: テストファイル作成

`tests/tst_app_lifecycle_pipeline.cpp` を作成:

```cpp
#include <QTest>
// 必要なスタブを用意
// LifecyclePipelineの各ステップを検証
```

### Step 4: テスト項目

1. `runStartup()` が全8段階を順序通りに実行する
2. `runShutdown()` が二重実行を防止する
3. `runShutdown()` が呼ばれた後にリソース解放が完了する
4. 起動中のエラーハンドリング（可能であれば）

### Step 5: テスト用スタブの作成

既存の `test_stubs_*.cpp` パターンに従い、依存クラスのスタブを作成:
- MainWindowやUI Formの最小スタブ
- ServiceRegistryのモックまたはスタブ

### Step 6: CMakeLists.txt更新

`tests/CMakeLists.txt` に新テストを追加:

```cmake
add_test_executable(tst_app_lifecycle_pipeline
    tst_app_lifecycle_pipeline.cpp
    # 必要なスタブファイル
)
```

### Step 7: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R tst_app_lifecycle_pipeline --output-on-failure
```

## 完了条件

- [ ] `tst_app_lifecycle_pipeline.cpp` が作成されている
- [ ] 起動/終了の基本シナリオがテストされている
- [ ] 全テスト通過（42テスト以上）
- [ ] CI通過

## KPI変化目標

- テスト数: 41 → 43（起動1本 + 終了1本、またはまとめて1本）

## 注意事項

- app層テストはGUIなしで実行できるよう `QT_QPA_PLATFORM=offscreen` を前提とする
- MainWindowの完全なインスタンス化は避け、テスト対象のクラスだけを構築する
- 既存の `test_stubs` パターンを参照・再利用する
- `connect()` でラムダを使わない（CLAUDE.md準拠）
