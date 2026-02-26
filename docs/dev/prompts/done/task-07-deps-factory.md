# Task 07: 巨大 Deps 組み立てメソッドの factory 分離

元ドキュメント: `docs/dev/mainwindow-responsibility-delegation-methods.md` セクション 4.7

## 対象メソッド

- `MainWindow::ensureKifuNavigationCoordinator`
- `MainWindow::ensureSessionLifecycleCoordinator`

## 現状問題

- Deps 構築だけで長く、変更差分が `MainWindow` に集中する。
- 依存項目の追加時に `MainWindow` の責務境界を壊しやすい。

## 移譲先

- 新規 `KifuNavigationDepsFactory`
- 新規 `SessionLifecycleDepsFactory`

## 実装手順

1. `src/app/kifunavigationdepsfactory.h/.cpp` を追加し、`KifuNavigationCoordinator::Deps build(MainWindow&)` を実装。
2. `src/app/sessionlifecycledepsfactory.h/.cpp` を追加し、`SessionLifecycleCoordinator::Deps build(MainWindow&)` を実装。
3. `MainWindow` は
   - `auto deps = KifuNavigationDepsFactory::build(*this);`
   - `m_kifuNavCoordinator->updateDeps(deps);`
   だけにする。
4. `friend class` は必要最小限にし、可能なら `MainWindowRuntimeRefs` 経由に揃える。

## 受け入れ条件

- 対象2メソッドが 10〜15 行程度に縮小する。
- `MainWindow` 内の `std::bind` 密度がさらに減る。

## 回帰確認

- 棋譜ナビ同期、終局後処理、時間設定適用が従来通り動作する。

## 共通実装ルール（全タスク共通）

- `ensure*` は「生成と `updateDeps` 呼び出し」だけにする。
- Deps 構築は factory に集約する。
- `connect()` は専用 wiring/coordinator に寄せ、`MainWindow` 内では最小化する。
- UI文言やフォーマット組み立ては wiring/service 側へ寄せる。
- `MainWindow` メソッド1つあたり目安 3〜10 行を上限とする。
