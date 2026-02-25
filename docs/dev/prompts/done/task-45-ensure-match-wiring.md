# Task 45: MatchCoordinator 関連配線の ensure* 分離

## Workstream D-5: MainWindow ensure* 群の用途別分離（実装 4/4）

## 前提

- 調査結果（`docs/dev/ensure-inventory.md`）を参照すること
- Task 42, 43, 44 が完了していること（分離パターンが確立済み）
- **実施順序**: 4タスク中4番目（最も複雑・影響範囲が広いため最後に実施）

## 目的

`MainWindow` の `ensureMatchCoordinatorWiring()` と関連する builder/wiring ヘルパー群、および `ensureGameStartCoordinator()` の配線を整理・分離する。

## 背景

- MatchCoordinator 関連は MainWindow 内で**最大の配線ブロック**（約152行, 16 connect）
- 以下のヘルパーメソッドが MainWindow に集中:
  - `buildMatchHooks()` (33行): MatchCoordinator::Hooks の構築
  - `buildMatchUndoHooks()` (9行): UndoHooks の構築
  - `buildMatchWiringDeps()` (50行): Deps 構造体構築（lazy init callback 含む）
  - `wireMatchWiringSignals()` (18行, 5 connect)
  - `initMatchCoordinator()` (22行): GameSessionFacade 経由の初期化
- `ensureGameStartCoordinator()` (14行) + `wireGameStartCoordinatorSignals()` (40行, **11 connect**) は MatchCoordinator と密結合（GameSessionFacade 経由で生成される）
- `MatchCoordinatorWiring` は既存クラスだが、MainWindow 側に builder メソッドが多数残っている

## 対象メソッド・行数

| メソッド | 行数 | 責務 |
|---|---|---|
| `ensureMatchCoordinatorWiring()` | 20行 (L2053-2072) | create + bind + 委譲 |
| `buildMatchHooks()` | 33行 (L2074-2107) | hooks 構築 |
| `buildMatchUndoHooks()` | 9行 (L2109-2118) | undoHooks 構築 |
| `buildMatchWiringDeps()` | 50行 (L2120-2171) | Deps 構築 (lazy callback含む) |
| `wireMatchWiringSignals()` | 18行 (L2173-2190) | wire (5 connect) |
| `initMatchCoordinator()` | 22行 (L2193-2214) | GameSessionFacade 初期化 |
| `ensureGameStartCoordinator()` | 14行 (L3221-3234) | create + bind + 委譲 |
| `wireGameStartCoordinatorSignals()` | 40行 (L3237-3277) | wire (**11 connect**) |

**合計**: 約206行, 16 connect

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/matchcoordinatorwiring.h` / `.cpp`（既存クラスの拡張）
- `CMakeLists.txt`（変更がある場合）

## 実装内容

### Phase 1: wireGameStartCoordinatorSignals の移動

- 11 connect を持つ `wireGameStartCoordinatorSignals()` を MatchCoordinatorWiring に移動
- GameStartCoordinator は MatchCoordinator と同じ GameSessionFacade から生成されるため、同じ wiring クラスに含めるのが妥当

### Phase 2: builder メソッドの移動検討

- `buildMatchHooks()` / `buildMatchUndoHooks()` / `buildMatchWiringDeps()` は MainWindow メンバへの参照が多い
- 移動可能な範囲で MatchCoordinatorWiring に移す
- MainWindow メンバへの依存が深いものは std::function callback 経由で注入する

### Phase 3: wireMatchWiringSignals の統合

- `wireMatchWiringSignals()` の 5 connect を MatchCoordinatorWiring 内部に移動
- MainWindow のスロットへの接続はシグナル転送で対応

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する
- 既存の初期化順序を変更しない
- `buildMatchHooks` 内の既存ラムダ（`sendGo`, `sendStop`, `sendRaw`）は維持する

## 受け入れ条件

- MatchCoordinator + GameStartCoordinator 関連の connect() が MatchCoordinatorWiring に集約されている
- MainWindow から builder/wiring ヘルパーの一部が削除されている
- `MainWindow` の行数が有意に削減されている（目標: 100行以上の削減）
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
- MatchCoordinatorWiring に移動したメソッド/connect の一覧
- MainWindow の行数変化
- 回帰リスク
- 残課題
