# Task 62: CSA/考慮モード/補助機能の入口整理（C14）

## マスタープラン参照
`docs/dev/mainwindow-delegation-master-plan.md` の C14 に対応。
推奨実装順序の第6段階（adapter化と補助機能）。

## 背景

CSA配線・考慮モード・持将棋/入玉・USIコマンド・PVクリック・定跡強制成りなど、補助機能の入口が MainWindow に残っている。

## 目的

補助機能の入口を `AnalysisInteractionCoordinator`（新規）または既存 Wiring クラスに統合し、MainWindow からモード依存分岐を排除する。

## 対象メソッド

| メソッド | 責務 |
|---|---|
| `ensureCsaGameWiring` | CSA配線 |
| `ensureJosekiWiring` | 定跡配線 |
| `ensureConsiderationWiring` | 検討配線 |
| `ensureJishogiController` | 持将棋コントローラ |
| `ensureNyugyokuHandler` | 入玉ハンドラ |
| `ensureUsiCommandController` | USIコマンドコントローラ |
| `onJosekiForcedPromotion` | 定跡成り強制 |
| `onPvRowClicked` | PV行クリック |

## 対象ファイル

- `src/app/mainwindow.h` / `.cpp`
- 既存拡張: `src/ui/wiring/csagamewiring.h` / `.cpp`
- 既存拡張: `src/ui/wiring/considerationwiring.h` / `.cpp`
- 既存拡張: `src/ui/controllers/pvclickcontroller.h` / `.cpp`
- 既存拡張: `src/ui/wiring/dialoglaunchwiring.h` / `.cpp`
- 新規候補: `src/ui/coordinators/analysisinteractioncoordinator.h` / `.cpp`
- `CMakeLists.txt`（変更がある場合）

## 実装手順

### Phase 1: AnalysisInteractionCoordinator 作成
1. 考慮モード・PVクリック・USIコマンド入口を統合するコーディネータを作成。
2. Deps パターンで依存を注入。

### Phase 2: PVクリックの移動
1. `onPvRowClicked` の依存同期（名前/SFEN/index）を PvClickController 側 `updateContext(...)` API へ移す。
2. MainWindow の中間スロットを削除。

### Phase 3: 定跡成り強制の直接化
1. `onJosekiForcedPromotion` を `JosekiWindowWiring` から `BoardSetupController`（または `BoardInteractionController`）に直接接続。
2. MainWindow の中間スロットを削除。

### Phase 4: ensure* の整理
1. `ensureCsaGameWiring`, `ensureJosekiWiring`, `ensureConsiderationWiring` は C13（ServiceRegistry）で移動済みの場合はスキップ。
2. `ensureJishogiController`, `ensureNyugyokuHandler`, `ensureUsiCommandController` も同様。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う（lambda connect 禁止）
- CSA 対局中の操作制約を壊さない
- 検討エンジン変更の挙動を維持

## 受け入れ条件

- 補助機能の入口がコーディネータ/既存 Wiring に統合されている
- MainWindow からモード依存分岐が排除されている
- ビルドが通る

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 回帰テスト

- PVクリックダイアログ
- 検討エンジン変更
- CSA中の操作制約
- 定跡成り強制
- 持将棋/入玉判定

## 出力

- 変更ファイル一覧
- 移動したメソッドの一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
