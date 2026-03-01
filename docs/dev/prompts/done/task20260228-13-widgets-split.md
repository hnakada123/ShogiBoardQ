# Task 13: widgets 大型ファイル分割

## 概要

`src/widgets/` 配下の大型ファイルを分割し、600行以下に削減する。

## 前提条件

- なし（他タスクと並行可能）

## 現状

対象ファイル:
- `src/widgets/considerationtabmanager.cpp`: 634行（ISSUE-062 ウィジェット分割）
- `src/widgets/branchtreemanager.cpp`: 632行（ISSUE-062 ウィジェット分割）
- `src/widgets/engineanalysistab.cpp`: 607行（ISSUE-062 ウィジェット分割）

関連（分割済み）:
- `src/widgets/recordpane.cpp`: 677→430行（分割済み）
- `src/widgets/evaluationchartwidget.cpp`: 1018→635行（分割済み）

## 実施内容

### Step 1: considerationtabmanager.cpp の分割

1. ファイルを読み込み、責務を分析
2. 検討タブの管理ロジックを分類:
   - **タブ生成/破棄** — タブの動的追加・削除
   - **モデル連携** — 思考モデルとの紐付け
   - **UI更新** — タブ内容の表示更新
3. 適切なクラスに分割

### Step 2: branchtreemanager.cpp の分割

1. ファイルを読み込み、責務を分析
2. 分岐ツリーの管理ロジックを分類:
   - **ツリーデータモデル** — 分岐構造のデータ管理
   - **ツリーUI操作** — ノードの展開/折りたたみ、選択
   - **ナビゲーション連携** — 選択ノードの棋譜位置との同期
3. 適切なクラスに分割

### Step 3: engineanalysistab.cpp の分割

1. ファイルを読み込み、責務を分析
2. エンジン解析タブのロジックを分類:
   - **レイアウト構築** — UIコンポーネントの配置
   - **解析結果表示** — PV行の表示・更新
   - **エンジン制御連携** — 解析開始/停止の制御
3. 適切なクラスに分割

### Step 4: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. 600行以下になったものは例外リストから削除
3. CMakeLists.txt に新規ファイルを追加

## 完了条件

- 3ファイルが 600行以下
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過

## 注意事項

- ウィジェットクラスは `QWidget` 継承のため、parent ownership で管理
- `connect()` にラムダを使わないこと
- `recordpane.cpp` の分割パターン（677→430行）を参考にする
- `evaluationchartwidget.cpp` の分割パターン（1018→635行）も参考にする
