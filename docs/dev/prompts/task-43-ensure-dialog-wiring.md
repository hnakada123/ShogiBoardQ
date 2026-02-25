# Task 43: DialogCoordinator 関連配線の ensure* 分離

## Workstream D-3: MainWindow ensure* 群の用途別分離（実装 2）

## 前提

- Task 41 の調査結果（`docs/dev/ensure-inventory.md`）を参照すること
- Task 42 が完了していること（同様のパターンで実施）

## 目的

`MainWindow` の `ensure*` メソッドのうち、`DialogCoordinator` 関連の配線を専用の wiring クラスに分離する。

## 背景

- `ensureDialogCoordinator()` に DialogCoordinator の生成・依存注入・シグナル配線が集中している
- 既存の `DialogLaunchWiring`（`src/ui/wiring/dialoglaunchwiring.h/.cpp`）はダイアログ起動のメニューアクション配線を担当しているが、DialogCoordinator 自体の配線は MainWindow に残っている
- DialogCoordinator の依存設定と配線を分離することで MainWindow を軽量化する

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/dialoglaunchwiring.h/.cpp`（既存クラスの拡張）、または新規 wiring クラス
- `CMakeLists.txt`（新規ファイル追加時）

## 実装内容

1. `ensureDialogCoordinator()` の実装を分析し、配線部分を特定する

2. 分離先を決定する:
   - 既存の `DialogLaunchWiring` を拡張するか、新規クラスを作成するか判断する
   - DialogCoordinator のシグナル（`considerationModeStarted`, `analysisModeStarted` 等）の接続先を確認する

3. 配線部分を移動する:
   - `connect()` 呼び出し群を wiring クラスへ移動
   - コンテキスト設定（`setConsiderationContext`, `setKifuAnalysisContext` 等）も移動を検討する

4. MainWindow 側を修正する:
   - `ensureDialogCoordinator()` から配線部分を除去し、wiring クラスの呼び出しに置換

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する
- 既存の初期化順序を変更しない

## 受け入れ条件

- DialogCoordinator 関連の connect() が wiring クラスに移動している
- MainWindow の `ensureDialogCoordinator()` の行数が削減されている
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
- 新規作成 or 拡張したクラスの責務説明
- 移動した connect() の数
- MainWindow の行数変化
- 回帰リスク
- 残課題
