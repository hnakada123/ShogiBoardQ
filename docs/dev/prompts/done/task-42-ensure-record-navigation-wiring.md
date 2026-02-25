# Task 42: RecordNavigationHandler 関連配線の ensure* 分離

## Workstream D-2: MainWindow ensure* 群の用途別分離（実装 1/4）

## 前提

- 調査結果（`docs/dev/ensure-inventory.md`）を参照すること
- **実施順序**: 4タスク中1番目（最も構造が整理済みで、分離パターン確立に最適）

## 目的

`MainWindow` の `ensureRecordNavigationHandler()` と関連ヘルパーの配線を専用の wiring クラスに分離する。

## 背景

- `ensureRecordNavigationHandler()` は既に create/wire/bind の3ヘルパーに分離済み
- `wireRecordNavigationHandlerSignals()`: 6 connect 呼び出し（14行）
- `bindRecordNavigationHandlerDeps()`: Deps 構造体設定（18行）
- 他サブシステムとの依存が少なく、独立性が高い
- このタスクで分離パターンを確立し、後続タスク（43〜45）に適用する

## 対象メソッド・行数

| メソッド | 行数 | 責務 |
|---|---|---|
| `ensureRecordNavigationHandler()` | 10行 (L4325-4334) | create + 委譲 |
| `wireRecordNavigationHandlerSignals()` | 14行 (L4337-4351) | wire (6 connect) |
| `bindRecordNavigationHandlerDeps()` | 18行 (L4354-4373) | bind (Deps構造体) |

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/` 配下に新規クラスを追加（例: `recordnavigationwiring.h/.cpp`）
- `CMakeLists.txt`（新規ファイル追加時）

## 実装内容

1. 新規 wiring クラスを作成する:
   - `wireRecordNavigationHandlerSignals()` の 6 connect を移動
   - `bindRecordNavigationHandlerDeps()` の Deps 構造体設定も移動を検討
   - 必要な sender/receiver オブジェクトを Deps 構造体で受け取る

2. MainWindow 側を修正する:
   - `ensureRecordNavigationHandler()` から wire/bind を除去し、wiring クラスの呼び出しに置換
   - `wireRecordNavigationHandlerSignals()` と `bindRecordNavigationHandlerDeps()` を MainWindow から削除
   - lazy init パターンは維持する

3. 関連する ensure* メソッドの確認:
   - `ensureRecordPresenter()` (1 connect): 棋譜表示関連で近いが独立性あり。含めるか判断する
   - `ensureReplayController()` (0 connect): bind のみなので対象外

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する
- 既存の初期化順序を変更しない

## 受け入れ条件

- RecordNavigationHandler 関連の connect() が wiring クラスに移動している
- MainWindow から `wireRecordNavigationHandlerSignals` / `bindRecordNavigationHandlerDeps` が削除されている
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
