# Task 07: ensure*呼び出しグラフ可視化・分析（テーマB: MainWindow依存密度低減）

## 目的

`MainWindowServiceRegistry` の `ensure*` 50件の呼び出しグラフを可視化し、再分割設計を行う。

## 背景

- `MainWindowServiceRegistry` に `ensure*` メソッドが50件集中
- 依存の集約点としての認知負荷が高い
- テーマB（MainWindow周辺の依存密度低減）の前提作業

## 作業手順

### Step 1: ensure*メソッドの完全一覧

```bash
# ensure*メソッドの定義一覧
rg -n "^void MainWindowServiceRegistry::ensure|^void MainWindow.*Registry::ensure" src/app/

# ヘッダでの宣言一覧
rg -n "void ensure" src/app/mainwindowserviceregistry.h

# 各Registryファイルの行数
wc -l src/app/mainwindow*registry*.cpp src/app/mainwindow*registry*.h
```

### Step 2: 呼び出し関係の分析

```bash
# ensure*の呼び出し元を特定
rg -n "ensure[A-Za-z0-9_]+\(" src/app/mainwindow*.cpp | sort

# ensure*間の相互呼び出し（依存グラフの辺）
for method in $(rg -o "ensure[A-Za-z0-9_]+" src/app/mainwindowserviceregistry.h | sort -u); do
    echo "=== $method ==="
    rg "$method" src/app/mainwindow*registry*.cpp | grep -v "^.*::$method"
done
```

### Step 3: ドメイン別クラスタリング

ensure*メソッドを以下のカテゴリに分類する:

1. **Game系**: 対局・ゲーム進行に関連するensure*
2. **Kifu系**: 棋譜管理・読み書きに関連するensure*
3. **Analysis系**: 解析・検討に関連するensure*
4. **UI系**: UI表示・ウィジェットに関連するensure*
5. **Board系**: 盤面操作・編集に関連するensure*
6. **Engine系**: エンジン管理に関連するensure*

### Step 4: 再分割設計書の作成

`docs/dev/ensure-graph-analysis.md` を作成し、以下を記録:

1. 全ensure*メソッドのカテゴリ分類表
2. 呼び出し依存グラフ（テキストベース）
3. 循環依存の有無
4. 推奨分割案（既存Registry→新子Registry）
5. 移行順序の提案

### Step 5: 分割案のリスク評価

- 循環依存が発生するパターンの洗い出し
- 分割により`updateDeps()`チェーンが複雑化しないかの検証
- 段階的移行が可能かの確認

## 完了条件

- [ ] 全50件のensure*が分類されている
- [ ] 呼び出しグラフがドキュメント化されている
- [ ] 循環依存のリスクが評価されている
- [ ] 具体的な再分割案が文書化されている
- [ ] コード変更はなし（分析のみ）

## 成果物

- `docs/dev/ensure-graph-analysis.md` — 分析結果と再分割設計

## 注意事項

- この作業はコード変更を伴わない分析タスク
- Task 08（実際の再分割）の前提作業
- `MainWindowCompositionRoot` の `ensure*` も含めて分析する
