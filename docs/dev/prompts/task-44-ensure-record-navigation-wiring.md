# Task 44: RecordNavigationHandler 関連配線の ensure* 分離

## Workstream D-4: MainWindow ensure* 群の用途別分離（実装 3）

## 前提

- Task 41 の調査結果（`docs/dev/ensure-inventory.md`）を参照すること
- Task 42, 43 が完了していること（同様のパターンで実施）

## 目的

`MainWindow` の `ensure*` メソッドのうち、`RecordNavigationHandler` 関連の配線を専用の wiring クラスに分離する。

## 背景

- `ensureRecordNavigationHandler()` は棋譜ペインの行選択変更ハンドラの生成・依存注入・配線を担う
- 棋譜ナビゲーション関連の配線は `RecordNavigationHandler` 以外にも `ensureRecordPresenter()`, `ensureReplayController()` 等に分散している可能性がある
- 関連する配線を一箇所にまとめることで見通しを改善する

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/` 配下に新規クラスを追加（例: `recordnavigationwiring.h/.cpp`）
- `CMakeLists.txt`（新規ファイル追加時）

## 実装内容

1. `ensureRecordNavigationHandler()` の実装を分析し、配線部分を特定する

2. 関連する ensure* メソッドを調査する:
   - `ensureRecordPresenter()`
   - `ensureReplayController()`
   - その他、棋譜ナビゲーション関連の配線を含む ensure*
   - Task 41 の調査結果を参照して対象範囲を確定する

3. 新規 wiring クラスを作成する:
   - 棋譜ナビゲーション関連の connect() を集約する
   - 必要な sender/receiver オブジェクトを Deps 構造体で受け取る

4. MainWindow 側を修正する:
   - 対象 ensure* メソッドから配線部分を除去し、wiring クラスの呼び出しに置換

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する
- 既存の初期化順序を変更しない

## 受け入れ条件

- RecordNavigationHandler 関連の connect() が wiring クラスに移動している
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
- 新規作成クラスの責務説明
- 移動した connect() の数
- MainWindow の行数変化
- 回帰リスク
- 残課題
