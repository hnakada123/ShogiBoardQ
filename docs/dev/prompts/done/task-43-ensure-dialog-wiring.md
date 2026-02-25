# Task 43: DialogCoordinator 関連配線の ensure* 分離

## Workstream D-3: MainWindow ensure* 群の用途別分離（実装 2/4）

## 前提

- 調査結果（`docs/dev/ensure-inventory.md`）を参照すること
- Task 42 が完了していること（同様のパターンで実施）
- Task 40（ConsiderationPositionResolver 統合）が完了していると効率的（検討コンテキスト設定が簡素化される可能性あり）
- **実施順序**: 4タスク中2番目

## 目的

`MainWindow` の `ensureDialogCoordinator()` と関連ヘルパーの配線・コンテキスト設定を専用の wiring クラスに分離する。

## 背景

- `ensureDialogCoordinator()` (11行) に生成・依存注入・配線が集中
- `bindDialogCoordinatorContexts()` (42行): 3つのコンテキスト構造体（Consideration, TsumeSearch, KifuAnalysis）の設定
- `wireDialogCoordinatorSignals()` (17行): 4 connect + DialogOrchestrator 経由の配線
- 既存の `DialogLaunchWiring` はメニューアクション→ダイアログ起動の配線を担当（別責務）

## 対象メソッド・行数

| メソッド | 行数 | 責務 |
|---|---|---|
| `ensureDialogCoordinator()` | 11行 (L2674-2684) | create + 委譲 |
| `bindDialogCoordinatorContexts()` | 42行 (L2687-2728) | bind (3 Context構造体) |
| `wireDialogCoordinatorSignals()` | 17行 (L2732-2749) | wire (4 connect + Orchestrator) |

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/` 配下に新規クラスを追加（例: `dialogcoordinatorwiring.h/.cpp`）
- `CMakeLists.txt`（新規ファイル追加時）

## 実装内容

1. 新規 wiring クラスを作成する:
   - `wireDialogCoordinatorSignals()` の connect を移動
   - `bindDialogCoordinatorContexts()` の 3コンテキスト設定も移動
   - `DialogOrchestrator::wireConsiderationSignals` / `wireUiStateSignals` の呼び出しも含める

2. MainWindow 側を修正する:
   - `ensureDialogCoordinator()` から bind/wire を除去し、wiring クラスの呼び出しに置換
   - `bindDialogCoordinatorContexts()` と `wireDialogCoordinatorSignals()` を MainWindow から削除
   - lazy init パターンは維持

3. 注意点:
   - `wireDialogCoordinatorSignals()` 内で `ensureConsiderationWiring()` と `ensureUiStatePolicyManager()` を呼んでいる。これらの ensure 呼び出しをどう扱うか判断する
   - `KifuAnalysisContext.getBoardFlipped` のラムダ（L2727）は既存のもの。維持する

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する
- 既存の初期化順序を変更しない

## 受け入れ条件

- DialogCoordinator 関連の connect() と Context 設定が wiring クラスに移動している
- MainWindow から `bindDialogCoordinatorContexts` / `wireDialogCoordinatorSignals` が削除されている
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
