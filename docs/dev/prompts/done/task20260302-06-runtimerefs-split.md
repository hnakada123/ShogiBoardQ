# Task 06: MainWindowRuntimeRefs を用途別分割（ISSUE-010 / P1）

## 概要

`MainWindowRuntimeRefs`（171行、57メンバ）を用途別のサブ構造体に分割し、変更影響範囲を縮小する。

## 現状

`src/app/mainwindowruntimerefs.h` には既に6つのサブ構造体がある:

| サブ構造体 | メンバ数 | 領域 |
|---|---|---|
| `RuntimeUiRefs` | 7 | UI widgets/form |
| `RuntimeModelRefs` | 8 | データモデル |
| `RuntimeKifuRefs` | 10 | 棋譜データ |
| `RuntimeStateRefs` | 6 | ゲーム状態 |
| `RuntimeBranchNavRefs` | 3 | 分岐ナビゲーション |
| `RuntimePlayerRefs` | 4 | プレイヤー名 |
| フラット（サービス系）| 5 | match, GC, view 等 |
| フラット（コントローラ系）| 11 | presenter, wiring 等 |
| フラット（その他）| 3 | controller 等 |

フラットなメンバが19個あり、依存集約点が大きい。

## 手順

### Step 1: フラットメンバの分類

1. フラットメンバ19個を領域ごとに分類する:
   - **GameServiceRefs**: `match`, `gameController`, `csaGameCoordinator`
   - **ViewRefs**: `shogiView`
   - **KifuServiceRefs**: `kifuLoadCoordinator`, `recordPresenter`, `replayController`
   - **UiControllerRefs**: `timeController`, `boardController`, `positionEditController`, `dialogCoordinator`, `uiStatePolicy`, `boardSync`
   - **WiringRefs**: `playerInfoWiring`
   - **AnalysisRefs**: `gameInfoController`, `evalGraphController`, `analysisPresenter`
   - **GameControllerRefs**: `gameStateController`, `consecutiveGamesController`

### Step 2: 新しいサブ構造体の作成

1. フラットメンバを新しいサブ構造体に整理する
2. 既存の6サブ構造体と合わせて、トップレベルの `MainWindowRuntimeRefs` は各サブ構造体の集約のみにする

### Step 3: 呼び出し元の段階移行

1. `MainWindowCompositionRoot` の参照を新しいサブ構造体経由に更新
2. `MainWindow` の参照を更新
3. Wiring クラスの参照を更新
4. 各 ensure* メソッドの引数で必要なサブ構造体のみを受け取るよう変更（段階的に）

### Step 4: 不要参照の削除

1. 分割後、各サブ構造体に含まれるメンバが実際に使われているか確認
2. 未使用メンバがあれば削除する

### Step 5: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_app_lifecycle_pipeline|tst_app_game_session|tst_app_kifu_load" --output-on-failure
   ```

## 完了条件

- `MainWindowRuntimeRefs` の単一巨大構造体依存を縮小
- 関連テスト通過

## 注意事項

- 既存の6サブ構造体は変更しない（後方互換性）
- フラットメンバの移動は一括で行い、コンパイルエラーを逐次修正する
- ISSUE-005 完了後に着手する
