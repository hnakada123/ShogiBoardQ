# Task 29: ダイアログ重複共通化（Phase 4: 品質仕上げ）

## 目的

ダイアログ群に共通する重複パターン（初期化、フォント設定、サイズ保存/復元、ボタン配置）を共通ユーティリティに抽出する。

## 背景

- `src/dialogs/` に25個のダイアログが存在（12,467行）
- 各ダイアログに類似の初期化パターン（フォント設定、サイズ復元、ボタン配置）が繰り返されている
- 変更時に全ダイアログを個別修正する必要があり、メンテナンスコストが高い
- 包括的改善分析 §11.2, code-quality-execution-tasks ISSUE-060〜063

## 事前調査

### Step 1: ダイアログの共通パターンの抽出

```bash
# フォント設定パターン
rg "setFont\|QFont" src/dialogs/*.cpp | head -20

# サイズ復元パターン
rg "resize\|restoreGeometry\|SettingsService" src/dialogs/*.cpp | head -20

# ボタン配置パターン
rg "QDialogButtonBox\|QPushButton.*OK\|QPushButton.*Cancel" src/dialogs/*.cpp | head -20

# closeEvent パターン
rg "closeEvent" src/dialogs/*.cpp | head -20
```

### Step 2: 重複パターンの頻度集計

```bash
# SettingsService 利用ダイアログ
rg -l "SettingsService\|settingsService" src/dialogs/*.cpp

# フォントサイズ調整機能
rg -l "fontSiz\|A-.*A+" src/dialogs/*.cpp
```

### Step 3: 既存のユーティリティの確認

```bash
cat src/common/dialogutils.h 2>/dev/null
cat src/common/fontsizehelper.h 2>/dev/null
```

## 実装手順

### Step 4: 共通パターンの特定

調査結果から、以下の共通パターンを抽出対象として確定:

1. **サイズ保存/復元**: `SettingsService` を使ったウィンドウサイズの永続化
2. **フォントサイズ調整**: A-/A+ ボタンによるフォントサイズ変更
3. **初期化ヘルパー**: ダイアログ共通の初期設定（フラグ、アイコン等）

### Step 5: DialogUtils の拡張

既存の `DialogUtils`（または新規作成）に共通メソッドを追加:

```cpp
// src/common/dialogutils.h
namespace DialogUtils {

// サイズ保存/復元
void restoreDialogSize(QDialog* dialog, const QString& settingsKey,
                        const QSize& defaultSize);
void saveDialogSize(QDialog* dialog, const QString& settingsKey);

// フォントサイズ調整
void setupFontSizeButtons(QDialog* dialog, QPushButton* decreaseBtn,
                           QPushButton* increaseBtn,
                           const QString& settingsKey);

// 共通初期化
void initializeDialog(QDialog* dialog, const QString& title,
                       const QSize& defaultSize,
                       const QString& settingsKey);

} // namespace DialogUtils
```

### Step 6: ダイアログ群の移行（段階的）

1. 最もシンプルなダイアログから移行開始
2. 各ダイアログの重複コードを共通メソッド呼び出しに置換
3. 1-2ファイルずつビルド・テスト確認

```cpp
// Before (各ダイアログの closeEvent)
void MyDialog::closeEvent(QCloseEvent* event)
{
    SettingsService::setMyDialogSize(size());
    QDialog::closeEvent(event);
}

// After
void MyDialog::closeEvent(QCloseEvent* event)
{
    DialogUtils::saveDialogSize(this, "MyDialog");
    QDialog::closeEvent(event);
}
```

### Step 7: 共通パターンのテスト

```cpp
// tests/tst_dialogutils.cpp（既存拡張または新規）
void TestDialogUtils::testSaveRestoreSize();
void TestDialogUtils::testFontSizeAdjustment();
```

### Step 8: CMakeLists.txt 更新

```bash
scripts/update-sources.sh
```

### Step 9: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] 共通ユーティリティ（`DialogUtils` 拡張）が実装されている
- [ ] 5個以上のダイアログが共通パターンを使用するよう移行済み
- [ ] 重複コードが削減されている（各ダイアログで5-15行削減）
- [ ] 全テスト通過
- [ ] SettingsService の保存/復元機能が正しく動作する

## KPI変化目標

- ダイアログ共通パターン使用率: 0% → 20%以上（5/25ダイアログ）
- `src/dialogs/` 総行数: 12,467 → 12,000以下

## 注意事項

- 全ダイアログを一度に変更しない（段階的移行）
- 各ダイアログの固有ロジックは維持する（共通化するのは定型パターンのみ）
- SettingsService の既存の getter/setter との互換性を維持
- QGroupBox のフォント問題（KDE Breeze）に注意（MEMORY.md 参照）
- CLAUDE.md のSettingsService更新ガイドラインに従う
