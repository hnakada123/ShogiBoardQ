# Task 45: ConsiderationWiring 関連配線の ensure* 拡充

## Workstream D-5: MainWindow ensure* 群の用途別分離（実装 4）

## 前提

- Task 41 の調査結果（`docs/dev/ensure-inventory.md`）を参照すること
- Task 42, 43, 44 が完了していること（同様のパターンで実施）

## 目的

`MainWindow` の `ensure*` メソッドのうち、検討モード関連の配線で `ConsiderationWiring`（既存）に移動可能なものを拡充する。

## 背景

- `ConsiderationWiring`（`src/ui/wiring/considerationwiring.h/.cpp`）は既に MainWindow から抽出済み
- しかし `ensureConsiderationWiring()` や `ensureConsiderationUIController()` にはまだ MainWindow 側に残っている配線がある可能性がある
- 既存の ConsiderationWiring を拡張し、検討モード関連の配線を集約する

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/considerationwiring.h`
- `src/ui/wiring/considerationwiring.cpp`
- `CMakeLists.txt`（変更がある場合）

## 実装内容

1. 現在の `ConsiderationWiring` の責務範囲を確認する

2. MainWindow 側に残っている検討モード関連の配線を特定する:
   - `ensureConsiderationWiring()` 内の connect() で wiring クラスに移動可能なもの
   - `ensureConsiderationUIController()` 内の connect()
   - 他の ensure* メソッド内で検討モード関連の connect() があるか
   - Task 41 の調査結果を参照して対象範囲を確定する

3. `ConsiderationWiring` を拡張する:
   - Deps 構造体に不足している依存を追加する
   - 移動対象の connect() を追加する

4. MainWindow 側を修正する:
   - 対象 ensure* メソッドから配線部分を除去し、ConsiderationWiring への委譲に置換

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する
- 既存の初期化順序を変更しない

## 受け入れ条件

- 検討モード関連の connect() が ConsiderationWiring に集約されている
- MainWindow の対象 ensure* メソッドの行数が削減されている
- 既存のシグナル配線が維持されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- ConsiderationWiring に追加した配線の一覧
- 移動した connect() の数
- MainWindow の行数変化
- 回帰リスク
- 残課題
