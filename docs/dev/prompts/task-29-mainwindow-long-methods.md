# Task 29: MainWindow 長大メソッドの分解

## Workstream A-2: MainWindow 責務分割

## 目的

`MainWindow` 内の長大メソッドを分解し、各メソッドの責務を明確にする。

## 背景

- `src/app/mainwindow.cpp` は 4,395 行と巨大
- 複数の長大メソッドが存在し、可読性と保守性が低い

## 対象ファイル

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- 必要に応じて新規クラスを `src/ui/coordinators/` `src/ui/wiring/` に追加

## 実装内容

以下のメソッドを優先的に分解する:

1. **`setupEngineAnalysisTab`**
   - エンジン解析タブの構築を段階的に分割
   - UI 構築、シグナル接続、初期状態設定を分離

2. **`initializeComponents`**
   - コンポーネント初期化を機能単位で分割
   - 各サブシステムの初期化を独立メソッドまたは別クラスへ

3. **`wireBranchNavigationSignals`**
   - 分岐ナビゲーション関連のシグナル接続を整理
   - 接続グループごとに分割

4. **`resetModels`**
   - モデルリセット処理を機能単位で分割

5. **`onBuildPositionRequired`**
   - 局面構築処理の段階を明確に分離

各メソッドの分解にあたっては、まず現在の行数と責務を確認し、適切な粒度で分割すること。

## 制約

- 既存挙動を変更しない
- Qt signal/slot はメンバ関数ポインタ構文を使う
- 新しい lambda connect を追加しない
- 分解後のメソッド名は責務を明確に表現すること

## 受け入れ条件

- 対象メソッドが明確に短縮されている
- 分解後の各メソッドの責務が 1 つに寄っている
- 既存 UI 操作（メニュー、ドック、棋譜選択、解析表示）に回帰がない

## 検証コマンド

```bash
cmake -B build -S .
cmake --build build
QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4
```

## 出力

- 変更ファイル一覧
- 各メソッドの分解前後の行数比較
- 回帰リスク
- 残課題
