# Task 35: ダイアログ寿命管理の安全化

## Workstream C-2: 所有権/可変性の是正

## 目的

手動 `new`/`delete` によるダイアログ管理を段階的に安全なパターンへ移行する。

## 背景

- `StartGameDialog* dlg = new ...; delete dlg;` のようなパターンが残存している
- 手動メモリ管理はリーク・二重解放のリスクがある
- `GameStartCoordinator` 内のダイアログ管理が特に対象

## 対象ファイル

- `src/game/gamestartcoordinator.cpp`（主要対象）
- `src/app/mainwindow.cpp`（該当箇所があれば）
- その他、`new`/`delete` パターンでダイアログを管理している箇所

## 実装内容

1. ダイアログの手動 `new`/`delete` パターンを洗い出す

2. 以下の方針で安全化する:
   - **スタック変数化**: モーダルダイアログで `exec()` を使っている場合はスタック変数へ
   - **Qt 親子管理**: 親ウィジェットを指定して Qt のオブジェクトツリーに任せる
   - **スマートポインタ**: 上記が適用できない場合は `std::unique_ptr` を使用

3. 段階的に移行する（一度にすべてを変更しない）:
   - まず `GameStartCoordinator` 内の明確なケースから着手
   - 影響範囲が大きい箇所は後回しにする

## 制約

- 既存挙動を変更しない
- 対局開始フローの動作を維持
- ダイアログの表示・入力・結果取得の動作を維持

## 受け入れ条件

- 手動 `delete` 由来の分岐依存解放ロジックが削減されている
- クラッシュ/リーク回帰がない
- ダイアログの表示が正常に動作する

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- 移行したダイアログの一覧（移行前パターン → 移行後パターン）
- 回帰リスク
- 残課題
