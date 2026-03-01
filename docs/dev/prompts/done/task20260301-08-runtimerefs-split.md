# Task 08: MainWindowRuntimeRefs 用途別分割

## 概要

`src/app/mainwindowruntimerefs.h` の57ポインタメンバを用途別サブ構造体に再編し、各コンシューマが必要最小限の参照のみ受け取れるようにする。

## 前提条件

- Task 01（GameSubRegistry分割）完了推奨（参照の利用先が変わるため）

## 現状

- `mainwindowruntimerefs.h`: 6つのサブ構造体 + トップレベル参照
  - `RuntimeUiRefs`: 7メンバ
  - `RuntimeModelRefs`: 8メンバ（ダブルポインタ含む）
  - `RuntimeKifuRefs`: 10メンバ
  - `RuntimeStateRefs`: 6メンバ
  - `RuntimeBranchNavRefs`: 3メンバ
  - `RuntimePlayerRefs`: 4メンバ
  - トップレベル: サービス5 + コントローラ/配線11 + その他3 = 19メンバ
- 合計: 57 raw ポインタメンバ
- 全メンバが raw ポインタ（非所有参照）

## 分析

トップレベルの19メンバが問題。以下の用途別に再分類:

### GameRuntimeRefs（対局関連、~8メンバ）
- `match`, `gameController`, `replayController`, `timeController`, `gameStateController`, `consecutiveGamesController`, `dialogCoordinator`, `playerInfoWiring`

### KifuRuntimeRefs（棋譜関連、~4メンバ）
- `recordPresenter`, `boardSync`, `uiStatePolicy`
- （既存の `RuntimeKifuRefs` と統合を検討）

### UiRuntimeRefs（UI関連、~7メンバ）
- `shogiView`, `boardController`, `positionEditController`, `gameInfoController`, `evalGraphController`, `analysisPresenter`, `csaGameCoordinator`

## 実施内容

### Step 1: 利用状況の調査

1. `mainwindowruntimerefs.h` の各トップレベルメンバがどのファイルから参照されるか調査
2. `RuntimeRefs` を `Deps` 構造体に渡しているクラスを列挙
3. 各クラスが実際に使用しているメンバを特定

### Step 2: サブ構造体の再編

1. トップレベルの19メンバを `GameRuntimeRefs`, `UiRuntimeRefs` 等に移動
2. 既存のサブ構造体（`RuntimeUiRefs` 等）との統合・名称調整
3. `MainWindowRuntimeRefs` は全サブ構造体を内包する集約として維持

### Step 3: コンシューマの更新

1. `RuntimeRefs` を受け取る各クラスの `Deps`/`Refs` 構造体を更新
2. `refs.match` → `refs.game.match` のようなアクセスパスに変更
3. コンパイルエラーを解消

### Step 4: ビルドとテスト

1. ビルド確認
2. 全テスト通過確認

## 完了条件

- トップレベル raw ポインタ: 19 → 0（全てサブ構造体に移動）
- 各サブ構造体が用途ごとに整理されている
- `mainwindow.h` の raw ポインタメンバ数: 42 → 32（中期目標）
- ビルド成功
- 全テスト通過

## 注意事項

- `MainWindowRuntimeRefs` 自体は値型（非QObject）の参照キャリアであり、所有権を持たない
- ダブルポインタ `T**` は `QPointer` と互換性がないため維持する
- サブ構造体名は既存の命名（`RuntimeUiRefs` 等）と整合を取る
- 変更影響範囲が広い（RuntimeRefs を参照する全クラス）ため、段階的に実施する
- `mainwindow.h` のメンバ削減は別タスク。本タスクは RuntimeRefs の構造整理のみ
