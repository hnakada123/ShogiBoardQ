# Task 42: MatchCoordinator 関連配線の ensure* 分離

## Workstream D-2: MainWindow ensure* 群の用途別分離（実装 1）

## 前提

Task 41 の調査結果（`docs/dev/ensure-inventory.md`）を参照すること。

## 目的

`MainWindow` の `ensure*` メソッドのうち、`MatchCoordinator` 関連の配線（connect 呼び出し）を専用の wiring クラスに分離する。

## 背景

- `ensureMatchCoordinatorWiring()` は MainWindow 内で最も配線量が多い ensure* の一つ
- MatchCoordinator への依存注入と信号配線が混在している
- 配線部分を専用クラスに移すことで、MainWindow の責務を軽減する

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/` 配下に新規クラスを追加（例: `matchcoordinatorwiring.h/.cpp`）
- `CMakeLists.txt`（新規ファイル追加時）

## 実装内容

1. `ensureMatchCoordinatorWiring()` の現在の実装を分析し、以下を分離する:
   - **配線（wire）**: `connect()` 呼び出し群 → 新規 wiring クラスへ
   - **生成（create）+ 依存注入（bind）**: MainWindow 側に残す

2. 新規 wiring クラスを作成する:
   - 配線に必要な sender/receiver オブジェクトを Deps 構造体またはコンストラクタで受け取る
   - `wireAll()` または個別の `wire*()` メソッドで connect を実行する
   - `Qt::UniqueConnection` は維持する

3. MainWindow 側を修正する:
   - `ensureMatchCoordinatorWiring()` から配線部分を除去し、wiring クラスの呼び出しに置換する
   - lazy init パターンは維持する

4. 関連する他の ensure* メソッドで MatchCoordinator 関連の connect がある場合は、同じ wiring クラスにまとめることを検討する

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する
- 既存の初期化順序を変更しない

## 受け入れ条件

- MatchCoordinator 関連の connect() が wiring クラスに移動している
- MainWindow の `ensureMatchCoordinatorWiring()` の行数が削減されている
- 既存のシグナル配線（`Qt::UniqueConnection`）が維持されている
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
