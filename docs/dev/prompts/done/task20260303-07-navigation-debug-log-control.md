# Task 20260303-07: ナビゲーション系デバッグログの出力量制御

## 概要
`KifuDisplayCoordinator` / `KifuNavigationCoordinator` に高頻度の `qCDebug` が多く、実行時ノイズが増加し問題切り分け時に必要ログが埋もれる。重要イベントは `qCInfo` へ格上げし、詳細トレースは専用カテゴリに分離する。

## 優先度
Low

## 背景
- ナビゲーション操作は高頻度（矢印キー連打等）で発火するため、全て `qCDebug` だとログが大量になる
- `docs/dev/debug-logging-guidelines.md` に従い、レベルと category を適切に分離する
- ENTER/LEAVE パターンの全ログは開発時トレース用であり、通常運用では不要

## 対象ファイル

### 修正対象
1. `src/ui/coordinators/kifudisplaycoordinator.cpp` - ログレベル調整
2. `src/app/kifunavigationcoordinator.cpp` - ログレベル調整・カテゴリ分離
3. `src/app/kifunavigationcoordinator.h` - カテゴリ宣言（必要な場合）

### 参照（変更不要だが理解が必要）
- `docs/dev/debug-logging-guidelines.md` - ログ規約

## 実装手順

### Step 1: トレース専用カテゴリを定義

`src/app/kifunavigationcoordinator.cpp` の冒頭に追加（既存の `lcApp` と並べる）:

```cpp
Q_LOGGING_CATEGORY(lcNavTrace, "shogi.nav.trace")
```

※ `kifudisplaycoordinator.cpp` も同様にトレースカテゴリを使う場合は、共通ヘッダーに宣言するか各 cpp に個別定義する。

### Step 2: KifuNavigationCoordinator のログ整理

各メソッドの ENTER/LEAVE ログを `lcNavTrace` に変更:

**navigateToRow (36行目付近):**
```cpp
// Before
qCDebug(lcApp).noquote() << "navigateKifuViewToRow ENTER ply=" << ply;
// After
qCDebug(lcNavTrace).noquote() << "navigateKifuViewToRow ENTER ply=" << ply;
```

**syncBoardAndHighlightsAtRow (86行目付近):**
```cpp
// Before
qCDebug(lcApp) << "syncBoardAndHighlightsAtRow ENTER ply=" << ply;
// After
qCDebug(lcNavTrace) << "syncBoardAndHighlightsAtRow ENTER ply=" << ply;
```

**handleBranchNodeHandled (157行目付近):**
```cpp
// Before
qCDebug(lcApp).noquote() << "onBranchNodeHandled ENTER ply=" << ply ...
// After
qCDebug(lcNavTrace).noquote() << "onBranchNodeHandled ENTER ply=" << ply ...
```

LEAVE ログも同様に `lcNavTrace` に変更する。

エラー系（null ガード等）は `qCWarning(lcApp)` に格上げ:
```cpp
// Before
qCDebug(lcApp).noquote() << "navigateKifuViewToRow ABORT: recordPane or kifuRecordModel is null";
// After
qCWarning(lcApp).noquote() << "navigateKifuViewToRow ABORT: recordPane or kifuRecordModel is null";
```

### Step 3: KifuDisplayCoordinator のログ整理

高頻度発火するハンドラ内のログを確認し、同様にトレースカテゴリに分離する。`kifudisplaycoordinator.cpp` 内の `qCDebug` を確認し:
- 状態遷移の重要イベント → `qCInfo` または `qCDebug(lcApp)` のまま
- 高頻度トレース → `qCDebug(lcNavTrace)` に変更

### Step 4: ビルド確認
```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

### Step 5: 動作確認

アプリケーションを起動し、棋譜ナビゲーション操作を行い:
- デフォルト設定でトレースログが出ないことを確認
- `QT_LOGGING_RULES="shogi.nav.trace=true"` 環境変数でトレースが有効化できることを確認

## 受け入れ基準
- ENTER/LEAVE 系トレースログが専用カテゴリ (`shogi.nav.trace`) に分離される
- エラー系ログが `qCWarning` に格上げされる
- デフォルトでナビゲーション操作時のログ出力量が削減される
- ビルド成功（warning なし）
- 既存テスト全件成功

## カテゴリ命名規約
- `docs/dev/debug-logging-guidelines.md` の命名規約 `shogi.<module>` に従う
- トレース系は `shogi.<module>.trace` サブカテゴリとする
