# Task 30: MainWindow ensure* の責務明確化

## Workstream A-3: MainWindow 責務分割

## 目的

`MainWindow` の `ensure*` メソッド群（37 個）の責務を明確化し、「生成」「依存注入」「配線」の混在を解消する。

## 背景

- `ensure*` メソッドが 37 個存在
- 1 つの `ensure*` メソッド内で、オブジェクト生成、依存注入（Deps 設定）、シグナル/スロット接続が混在している
- 責務が不明確なため、変更時の影響範囲が把握しにくい

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`

## 実装内容

1. 全 37 個の `ensure*` メソッドの現状を調査し、以下の責務に分類する:
   - **生成（create）**: オブジェクトのインスタンス化
   - **依存注入（bind/inject）**: Deps 構造体の設定、updateDeps() 呼び出し
   - **配線（wire）**: connect() によるシグナル/スロット接続

2. 混在が顕著なメソッドを優先的に分割する:
   - `ensure*` → `create*` + `wire*` + `bind*`（必要に応じて）
   - ただし、シンプルな ensure*（生成のみ）はそのまま維持してよい

3. 分割にあたっては以下の方針に従う:
   - lazy init パターン（初回呼び出し時に生成）は維持する
   - 分割後も呼び出し側の変更を最小限にする
   - 過度な分割は避け、混在が問題になっているメソッドのみ対象とする

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- lazy init パターンを維持する

## 受け入れ条件

- 混在が顕著だった `ensure*` メソッドが責務別に分割されている
- 各メソッドの役割が名前から判断できる
- 既存の呼び出しパターンが大きく変わっていない

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- 分割した `ensure*` メソッドの一覧（分割前 → 分割後）
- 回帰リスク
- 残課題
