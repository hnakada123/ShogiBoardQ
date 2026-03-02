# Task 10: 巨大コンストラクタ分割バッチ1（ISSUE-014 / P1）

## 概要

巨大コンストラクタ（100行超関数）を `buildUi` / `bindSignals` / `restoreSettings` / `bindModels` に分解し、可読性を改善する。

## 現状

`long_functions_over_100 = 136` で可読性負債が大きい。

対象ダイアログファイル（コンストラクタ自体は30行程度だが、関連する初期化メソッドが大きい可能性）:

| ファイル | 総行数 | コンストラクタ行数 |
|---|---|---|
| `src/dialogs/sfencollectiondialog.cpp` | - | ~34行（ただし `buildUi()` 等に委譲済み） |
| `src/dialogs/startgamedialog.cpp` | 481 | ~30行（`connectSignalsAndSlots()` 45行） |
| `src/dialogs/csagamedialog.cpp` | 450 | ~32行 |

## 手順

### Step 1: 100行超関数の特定

1. `tst_structural_kpi` のテスト実行結果から100行超関数の一覧を取得する
2. ダイアログ以外にも100行超の関数がある場合、分割効果が高いものを優先する
3. 対象ファイル候補を5-10個選定する

### Step 2: 分割パターンの適用

各巨大関数について:

**コンストラクタの場合:**
```cpp
// Before
MyDialog::MyDialog(QWidget* parent) : QDialog(parent) {
    // 100行以上の初期化コード
}

// After
MyDialog::MyDialog(QWidget* parent) : QDialog(parent) {
    ui->setupUi(this);
    buildUi();
    bindSignals();
    restoreSettings();
}
```

**一般メソッドの場合:**
- 論理的なブロックをサブメソッドに抽出
- 状態マシン的な処理は case ごとにメソッド化

### Step 3: 初期化順序の文書化

1. 分割したメソッドの呼び出し順序が重要な場合、コメントで順序を明記する
2. 特に `restoreSettings()` は `bindSignals()` の後に呼ぶ必要がある場合が多い

### Step 4: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_ui_display_consistency|tst_game_start_flow|tst_wiring_csagame" --output-on-failure
   ```
3. `long_functions_over_100` が減少していることを確認（監視指標）

## 完了条件

- 対象コンストラクタが 100行未満または責務分離され、可読性が改善
- UI関連テスト通過

## 注意事項

- コンストラクタ自体は既に30行程度に抑えられている場合がある。その場合は他の100行超関数を対象にする
- `long_functions_over_100 = 136` は監視KPIのため、閾値は設けずに改善を進める
- ISSUE-013 完了後に着手する
