# Task 40: DialogCoordinator 検討局面解決ロジックの統合

## Workstream C-1: DialogCoordinator 重複ロジック統合

## 目的

`DialogCoordinator::startConsiderationFromContext()` 内の局面解決ロジックを `ConsiderationPositionResolver` に統合し、重複実装を解消する。

## 背景

- `ConsiderationPositionResolver`（`src/kifu/`）は MainWindow から抽出された検討局面解決ロジック
- `DialogCoordinator::startConsiderationFromContext()` にも同等の局面解決ロジック（`buildPositionStringForIndex`, `previousFileTo/RankTo` 解決, `lastUsiMove` 解決）が独立実装されている
- 同じ機能の二重実装は修正漏れやバグの温床になる

## 対象ファイル

- `src/kifu/considerationpositionresolver.h`
- `src/kifu/considerationpositionresolver.cpp`
- `src/ui/coordinators/dialogcoordinator.h`
- `src/ui/coordinators/dialogcoordinator.cpp`

## 実装内容

### Step 1: ConsiderationPositionResolver の入力構造を拡張

`DialogCoordinator` が使う追加情報に対応できるよう `Inputs` 構造体を見直す:
- `currentSfenStr`（現在表示中の分岐局面 SFEN）の追加を検討
- `ConsiderationContext` と `ConsiderationPositionResolver::Inputs` のフィールドを比較し、不足分を補う

### Step 2: DialogCoordinator::buildPositionStringForIndex() を置換

- `ConsiderationPositionResolver::buildPositionStringForIndex()` が `DialogCoordinator` 側のロジック（`currentSfenStr` 優先参照、`sfenRecord` フォールバック）もカバーできるよう拡張する
- `DialogCoordinator::buildPositionStringForIndex()` を削除し、`ConsiderationPositionResolver::resolveForRow()` 呼び出しに置換する

### Step 3: previousFileTo/RankTo/lastUsiMove 解決の統合

- `startConsiderationFromContext()` 内の `previousFileTo`/`previousRankTo` 解決処理と `ConsiderationPositionResolver::resolvePreviousMoveCoordinates()` を比較する
- `lastUsiMove` 解決処理と `ConsiderationPositionResolver::resolveLastUsiMoveForPly()` を比較する
- 共通化可能な部分を `ConsiderationPositionResolver` に統合する
- `DialogCoordinator` 独自のフォールバック（`extractUsiMoveFromKanjiLabel` 等）は `DialogCoordinator` 側に残して良い

### Step 4: 不要コードの削除

- 置換後に不要となった `#include`、ヘルパーメソッド、コメントを削除する
- `ConsiderationContext` のフィールドで不要になったものがあれば削除する

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- `ConsiderationPositionResolver` は UI 非依存を維持する（Qt GUI クラスへの依存を増やさない）

## 受け入れ条件

- `DialogCoordinator` 内の `buildPositionStringForIndex()` が削除されている
- `previousFileTo`/`previousRankTo`/`lastUsiMove` の解決が可能な範囲で `ConsiderationPositionResolver` に統合されている
- 既存機能（検討開始、分岐局面、棋譜読み込み局面）で挙動差分がない
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 確認観点

- 通常対局中の検討開始
- 棋譜読込後の任意手数から検討開始
- 分岐ライン選択中の検討開始

## 出力

- 変更ファイル一覧
- `ConsiderationPositionResolver::Inputs` の変更点
- 統合できなかったロジック（`DialogCoordinator` に残したもの）の理由
- 回帰リスク
- 残課題
