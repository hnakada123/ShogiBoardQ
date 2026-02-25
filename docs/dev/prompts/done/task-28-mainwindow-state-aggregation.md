# Task 28: MainWindow サブシステム単位の状態集約

## Workstream A-1: MainWindow 責務分割

## 目的

`MainWindow` が保持する多数のメンバ変数を、サブシステム単位で構造体化または専用クラス化し、可読性と変更安全性を向上させる。

## 背景

- `src/app/mainwindow.h` に多数の依存と状態が保持されている
- `ensure*` 定義が 37 個存在し、状態管理が散在している
- 解析系、棋譜系、ドック系、対局開始系のメンバが混在している

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- 必要に応じて `src/ui/coordinators/` `src/ui/wiring/` に新規クラス追加

## 実装内容

1. `MainWindow` のメンバ変数を以下のサブシステム単位で分類する:
   - **解析系**: エンジン解析タブ、検討モード関連の状態
   - **棋譜系**: 棋譜モデル、棋譜表示関連の状態
   - **ドック系**: ドックウィジェット管理関連の状態
   - **対局開始系**: 対局開始フロー関連の状態

2. 各サブシステムの状態を構造体（`struct`）または専用クラスに集約する

3. `MainWindow` 内のメンバ変数を集約後の構造体/クラスに置き換える

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- `connect()` にラムダを増やさない

## 受け入れ条件

- サブシステムごとに状態が整理されている
- `MainWindow` のメンバ変数の見通しが改善されている
- 各抽出クラス/関数の責務が 1 つに寄っている

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- 実装内容の説明
- 回帰リスク
- 残課題
