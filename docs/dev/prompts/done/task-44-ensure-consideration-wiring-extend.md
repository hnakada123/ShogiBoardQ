# Task 44: ConsiderationWiring 関連配線の ensure* 拡充

## Workstream D-4: MainWindow ensure* 群の用途別分離（実装 3/4）

## 前提

- 調査結果（`docs/dev/ensure-inventory.md`）を参照すること
- Task 42, 43 が完了していること（同様のパターンで実施）
- Task 43 完了後、DialogCoordinator の配線が整理済みの前提
- **実施順序**: 4タスク中3番目

## 目的

`MainWindow` の `ensureConsiderationWiring()` 内の複雑なコードを整理し、`ConsiderationWiring`（既存）に移動可能な配線・設定を拡充する。

## 背景

- `ConsiderationWiring`（`src/ui/wiring/considerationwiring.h/.cpp`）は既に MainWindow から抽出済み
- `ensureConsiderationWiring()` (40行) には Deps 構造体の構築と、`ensureDialogCoordinator` の lazy callback（deps再コピーパターン）が含まれている
- この lazy callback（L4233-4250）は `ensureDialogCoordinator` が呼ばれた後に Deps を再構築して `updateDeps()` する冗長なパターン → Task 43 の DialogCoordinator 配線分離により改善の余地あり
- connect は 1つのみ（`stopRequested` → `stopTsumeSearch`）

## 対象メソッド・行数

| メソッド | 行数 | 責務 |
|---|---|---|
| `ensureConsiderationWiring()` | 40行 (L4218-4257) | create + bind + wire (1 connect) |
| `ensureConsiderationUIController()` | 7行 (L4209-4215) | fetch (ConsiderationWiring から委譲) |

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/considerationwiring.h`
- `src/ui/wiring/considerationwiring.cpp`

## 実装内容

1. 現在の `ConsiderationWiring` の責務範囲を確認する

2. `ensureConsiderationWiring()` 内の改善:
   - lazy `ensureDialogCoordinator` callback 内の Deps 再コピー（L4233-4250）の冗長さを解消する
   - Task 43 で DialogCoordinator の配線が wiring クラスに移動済みなら、callback の簡素化が可能
   - `stopRequested` → `stopTsumeSearch` の connect を ConsiderationWiring 内部に移動可能か検討

3. Deps 構造体の整理:
   - 現在 Deps に含まれる `ensureDialogCoordinator` コールバックの必要性を再評価
   - 不要になったフィールドがあれば削除

4. MainWindow 側を修正する:
   - `ensureConsiderationWiring()` を簡素化

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する
- 既存の初期化順序を変更しない

## 受け入れ条件

- `ensureConsiderationWiring()` の冗長な Deps 再コピーパターンが解消されている
- ConsiderationWiring の責務範囲が明確である
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
- ConsiderationWiring の変更点
- MainWindow の行数変化
- 回帰リスク
- 残課題
