# Task 06: changeenginesettingsdialog.cpp 責務分割

## 概要

`src/dialogs/changeenginesettingsdialog.cpp`（804行）を責務分割し、600行以下に削減する。

## 前提条件

- Task 02（engineregistrationdialog分割）完了推奨（同系統ダイアログの分割パターンを再利用できる）

## 現状

- `src/dialogs/changeenginesettingsdialog.cpp`: 804行
- `src/dialogs/changeenginesettingsdialog.h`: 187行
- ISSUE-057 ダイアログ分割の対象
- KPI例外リストに登録済み（上限 804）

## 分析すべき責務

ファイルを読み込んで以下のカテゴリに分類すること:

1. **UIの初期化・レイアウト** — ダイアログ本体に残す
2. **エンジンオプション管理** — USIオプションの読み取り・表示・編集
3. **設定永続化** — QSettings への読み書き
4. **バリデーション** — 入力値の検証ロジック

## 実施内容

### Step 1: 責務分析

`changeenginesettingsdialog.cpp` を読み、メソッドごとの責務を分類する。
Task 02 で確立した分割パターンを再利用する。

### Step 2: モデル/ハンドラ抽出

分析結果に基づき分割する:

- `src/dialogs/enginesettingsmodel.h/.cpp` — エンジン設定データモデル
- 必要に応じて追加のハンドラクラス

### Step 3: ダイアログからの委譲

1. ダイアログクラスから抽出した責務を新クラスに委譲
2. シグナル/スロット接続を更新

### Step 4: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. CMakeLists.txt に新規ファイルを追加

## 完了条件

- `changeenginesettingsdialog.cpp` が 650行以下
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過
- SettingsService の既存永続化ロジックが維持されていること

## 注意事項

- `connect()` にラムダを使わないこと
- Task 02 で作成した `engineregistrationmodel` や `engineregistrationhandler` との共通ロジックがあれば、共有クラスへの統合を検討
- SettingsService の getter/setter を維持すること
