# Task 03: josekimovedialog.cpp 責務分割

## 概要

`src/dialogs/josekimovedialog.cpp`（838行）を責務分割し、600行以下に削減する。

## 前提条件

- なし（Task 01, 02 と並行可能）

## 現状

- `src/dialogs/josekimovedialog.cpp`: 838行
- `src/dialogs/josekimovedialog.h`: 285行
- ISSUE-057 ダイアログ分割の対象
- KPI例外リストに登録済み（上限 838）

## 分析すべき責務

ファイルを読み込んで以下のカテゴリに分類すること:

1. **UIの初期化・レイアウト** — ダイアログ本体に残す
2. **定跡データモデル** — 定跡手のデータ構造・検索・フィルタリング
3. **動的UI生成** — 局面に基づくUIの動的構築ロジック
4. **入力バリデーション** — ユーザー入力の検証ロジック

## 実施内容

### Step 1: 責務分析

`josekimovedialog.cpp` と `.h` を読み、メソッドごとの責務を分類する。
ヘッダも 285行と大きいため、メンバ変数・型定義の分離も検討する。

### Step 2: モデル/コントローラ抽出

分析結果に基づき分割する（実際の責務に応じて調整）:

- `src/dialogs/josekimovemodel.h/.cpp` — 定跡データの管理・検索ロジック
- `src/dialogs/josekimoveinputcontroller.h/.cpp` — 入力制御・バリデーション

### Step 3: ダイアログからの委譲

1. ダイアログクラスから抽出した責務を新クラスに委譲
2. シグナル/スロット接続を更新
3. `.ui` ファイルがある場合はダイアログ本体に残す

### Step 4: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. CMakeLists.txt に新規ファイルを追加

## 完了条件

- `josekimovedialog.cpp` が 650行以下
- ヘッダも可能な限り 200行以下に削減
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過

## 注意事項

- `connect()` にラムダを使わないこと
- SettingsService にダイアログサイズ等の永続化がある場合は維持する
- 定跡ロジックが `core/` に属するべき場合は適切なディレクトリに配置
