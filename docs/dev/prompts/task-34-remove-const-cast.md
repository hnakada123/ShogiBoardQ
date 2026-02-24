# Task 34: const_cast の除去

## Workstream C-1: 所有権/可変性の是正

## 目的

`MainWindow` 内の `const_cast` を除去し、型安全性を向上させる。

## 背景

- `MainWindow::ensureGameRecordModel()` で `liveDisp()` を `const_cast` して `GameRecordModel::bind()` へ渡している
- `const_cast` は型安全性を損ない、未定義動作のリスクがある

## 対象ファイル

- `src/app/mainwindow.cpp`（`const_cast` の使用箇所）
- `src/ui/presenters/gamerecordpresenter.h/.cpp`（`liveDisp()` の定義）
- `src/kifu/gamerecordmodel.h/.cpp`（`bind()` の定義）

## 実装内容

1. 現在の `const_cast` 使用箇所を特定し、なぜ必要になっているかを分析する

2. 以下のいずれかのアプローチで `const_cast` を除去する:

   **案 A: GameRecordPresenter に非 const API を追加**
   - `liveDispMutable()` のような非 const アクセサを追加
   - `const_cast` なしでアクセスできるようにする

   **案 B: GameRecordModel 側を変更**
   - `bind()` のインターフェースを変更し、const ポインタを受け取れるようにする
   - 内部で必要な可変操作のみを別途管理

3. どちらのアプローチが適切かは、実装を読んでから判断すること

## 制約

- 既存挙動を変更しない
- `GameRecordModel` と `GameRecordPresenter` の同期整合を維持
- コメント/しおり更新の双方向反映を維持

## 受け入れ条件

- 対象箇所で `const_cast` が不要化されている
- 型安全性が向上している
- クラッシュ/リーク回帰がない

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- 採用したアプローチの説明
- 回帰リスク
- 残課題
