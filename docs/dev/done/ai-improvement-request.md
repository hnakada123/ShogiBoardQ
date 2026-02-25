# ShogiBoardQ 改修依頼仕様書（AI向け）

作成日: 2026-02-24
対象リポジトリ: `ShogiBoardQ`

## 1. 目的

このドキュメントは、コード品質レビューで特定した改善点を AI に安全かつ段階的に実装させるための依頼仕様書である。  
主眼は以下の 3 点:

1. 巨大クラスの責務整理（`MainWindow` / `MatchCoordinator`）
2. 所有権・可変性の整理（`const_cast` と手動 `delete` の削減）
3. 品質保証の維持（警告ゼロ、既存テスト全通過）

## 2. 現状サマリ（2026-02-24 時点）

### 2.1 良い点

- 層分割（`ui/wiring`, `ui/coordinators`, `ui/presenters`）が進んでいる
- コンパイル警告設定が厳格（`-Wall -Wextra -Wpedantic -Wshadow -Wconversion ...`）
- テストが 22 本整備されている
- `QT_QPA_PLATFORM=offscreen` で既存テスト 22/22 が通過

### 2.2 主な課題

- `MainWindow` が依然大きい
  - `src/app/mainwindow.cpp`: 4,395 行
  - `src/app/mainwindow.h`: 多数の依存と状態を保持
  - `ensure*` 定義: 37 個
  - `connect()` 呼び出し: 70 箇所
- `MatchCoordinator` が司令塔として肥大化
  - `src/game/matchcoordinator.cpp`: 2,598 行
  - `configureAndStart()` が長大（履歴探索・開始設定・モード分岐を同居）
- 可変性/所有権のにおい
  - `src/app/mainwindow.cpp` で `const_cast` による `liveDisp` 可変化
  - 一部で `new`/`delete` 手動管理が残存（例: `GameStartCoordinator` 内の dialog 管理）

## 3. 改修の前提条件（必須）

1. 既存挙動を変えない
- 対局開始フロー（平手/駒落ち/現在局面/分岐/連続対局）を維持
- USI/CSA 連携挙動を維持

2. 既存方針を守る
- Qt signal/slot はメンバ関数ポインタ構文を使う
- `connect()` にラムダを増やさない

3. 品質ゲートを満たす
- ビルド成功
- 新規警告を出さない
- 既存テスト + 追加テストが通る

## 4. 全体実行順（推奨）

1. Workstream A: `MainWindow` の責務分割
2. Workstream B: `MatchCoordinator` の責務分割
3. Workstream C: 所有権/可変性の是正
4. Workstream D: 回帰テスト拡充と最終整備

上から順に実施すること。各 Workstream は「小さな PR 単位」に分割する。

## 5. Workstream A: MainWindow 責務分割

### 5.1 目的

- `MainWindow` の「状態保持」「配線」「画面構築」「対局開始準備」を分離し、可読性と変更安全性を上げる

### 5.2 対象

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- 必要に応じて `src/ui/coordinators/` `src/ui/wiring/` に新規クラス追加

### 5.3 実装タスク

1. サブシステム単位で状態を集約
- 解析系、棋譜系、ドック系、対局開始系のメンバを構造体化または専用クラス化

2. 長大メソッドの分解
- 候補（優先）:
  - `setupEngineAnalysisTab`
  - `initializeComponents`
  - `wireBranchNavigationSignals`
  - `resetModels`
  - `onBuildPositionRequired`

3. `ensure*` の責務明確化
- 「生成」「依存注入」「配線」を同一メソッドで混在させない
- 必要なら `create*`, `wire*`, `bind*` へ分割

### 5.4 受け入れ条件

- `MainWindow` の巨大メソッドが明確に短縮されている
- 各抽出クラス/関数の責務が 1 つに寄っている
- 既存 UI 操作（メニュー、ドック、棋譜選択、解析表示）に回帰がない

## 6. Workstream B: MatchCoordinator 責務分割

### 6.1 目的

- 司令塔クラスから「履歴探索」「開始オプション展開」「検討モード管理」「UNDO 処理」を分離する

### 6.2 対象

- `src/game/matchcoordinator.h`
- `src/game/matchcoordinator.cpp`
- `src/game/` 配下の新規ヘルパ/サービス

### 6.3 実装タスク

1. `configureAndStart()` の分解（最優先）
- 役割を最低 4 つへ分離:
  - 既存履歴同期と探索
  - 開始 SFEN 正規化と position 文字列構築
  - フック呼び出し/UI 初期化
  - `PlayMode` 別起動

2. 検討モード関連の分離
- `startAnalysis`, `updateConsiderationPosition`, 再開処理を専用コンポーネントへ

3. UNDO 処理の整理
- `UndoRefs` / `UndoHooks` の責務境界を明確化
- 処理の再利用可能部分を抽出

### 6.4 受け入れ条件

- `configureAndStart()` の読みやすさが向上し、分岐ごとの責務が明確
- EvE/HvE/HvH の起動シナリオで回帰がない
- 既存テストに加え、開始フロー回帰のテストを追加/強化

## 7. Workstream C: 所有権/可変性の是正

### 7.1 目的

- `const_cast` と手動解放のリスクを減らし、破壊的変更に強いコードへ寄せる

### 7.2 対象

- `src/app/mainwindow.cpp`
- `src/ui/presenters/gamerecordpresenter.h/.cpp`
- `src/kifu/gamerecordmodel.h/.cpp`
- `src/game/gamestartcoordinator.cpp`

### 7.3 実装タスク

1. `const_cast` の除去
- 現状:
  - `MainWindow::ensureGameRecordModel()` で `liveDisp()` を `const_cast` して `GameRecordModel::bind()` へ渡している
- 改善案:
  - `GameRecordPresenter` に非 const の明示 API を追加（例: `liveDispMutable()`）
  - もしくは `GameRecordModel` 側を参照同期 API へ変更して可変ポインタ依存を除去

2. ダイアログ寿命管理の安全化
- `StartGameDialog* dlg = new ...; delete dlg;` パターンを段階的に削減
- 可能な箇所は自動寿命（スタック）または Qt 親子管理へ寄せる

3. 所有/非所有コメントの実態合わせ
- メンバコメントの「非所有」と実際のライフサイクルが一致するよう見直し

### 7.4 受け入れ条件

- 対象箇所で `const_cast` 不要化
- 手動 `delete` 由来の分岐依存解放ロジックが削減
- クラッシュ/リーク回帰がない

## 8. Workstream D: テスト拡充と品質ゲート

### 8.1 目的

- 上記リファクタリングの回帰を確実に検出する

### 8.2 追加/強化テスト候補

1. 対局開始フロー
- 現在局面開始（分岐あり/なし）
- プリセット開始（平手/駒落ち）
- 連続対局設定

2. 棋譜コメント同期
- `GameRecordModel` と `GameRecordPresenter` の同期整合
- コメント/しおり更新の双方向反映

3. 検討モード遷移
- `startAnalysis` → `updateConsiderationPosition` → 再開のシーケンス

### 8.3 実行コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

注意:
- GUI 環境がない実行環境では `QT_QPA_PLATFORM=offscreen` を付けること

## 9. PR 分割ルール（必須）

1. 1 PR = 1 テーマ
- 例:
  - PR-1: MainWindow のメソッド分解のみ
  - PR-2: MatchCoordinator の開始処理分解のみ
  - PR-3: `const_cast` 除去のみ

2. 各 PR に必ず含める内容
- 変更目的
- 挙動差分（ある/なし）
- テスト結果（実行コマンドと結果）
- 未解決の技術的負債（次 PR への引き継ぎ）

3. 非機能改修でも必ず検証する
- ビルド
- 既存テスト全件

## 10. AI への実行テンプレート

以下を各 Workstream 実行時に AI へ渡すこと:

```text
ShogiBoardQ の改修を行ってください。
対象は Workstream <A|B|C|D> のみです。

制約:
- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- 変更後に build と test を実行する

必須コマンド:
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4

出力:
- 変更ファイル一覧
- 実装内容
- 回帰リスク
- 残課題
```

## 11. 完了条件（Definition of Done）

以下をすべて満たした時点で本改善を完了とする。

1. `MainWindow` / `MatchCoordinator` の主要長大処理が段階分割済み
2. 対象箇所の `const_cast` 依存を除去
3. 手動寿命管理の危険箇所を削減
4. 既存 + 追加テストが安定して通る
5. ビルド警告を増やしていない

