# Task 02: engineregistrationdialog.cpp 責務分割

## 概要

`src/dialogs/engineregistrationdialog.cpp`（842行）を Model/Presenter/ActionHandler パターンで分割し、600行以下に削減する。

## 前提条件

- なし（Task 01 と並行可能）

## 現状

- `src/dialogs/engineregistrationdialog.cpp`: 842行
- `src/dialogs/engineregistrationdialog.h`: 196行
- ISSUE-057 ダイアログ分割の対象
- KPI例外リストに登録済み（上限 842）

## 分析すべき責務

ファイルを読み込んで以下のカテゴリに分類すること:

1. **UIの初期化・レイアウト** — ダイアログ本体に残す
2. **エンジンプロセスライフサイクル** — プロセス起動・停止・通信管理
3. **エンジン設定のバリデーション・永続化** — QSettings への読み書き
4. **USIプロトコルハンドリング** — USIコマンド送受信の解析

## 実施内容

### Step 1: 責務分析

`engineregistrationdialog.cpp` を読み、メソッドごとの責務を分類する。

### Step 2: モデル/ハンドラ抽出

分析結果に基づき、以下のような分割を行う（実際の責務に応じて調整）:

- `src/dialogs/engineregistrationmodel.h/.cpp` — エンジン登録データモデル
- `src/dialogs/engineregistrationhandler.h/.cpp` — エンジンプロセス操作ハンドラ

### Step 3: ダイアログからの委譲

1. ダイアログクラスから抽出した責務を新クラスに委譲
2. Deps パターン（依存注入構造体）の適用を検討
3. シグナル/スロット接続を更新

### Step 4: KPI更新

1. `tst_structural_kpi.cpp` の `knownLargeFiles()` で上限値を更新
2. ダイアログが 600行以下になった場合は例外リストから削除
3. CMakeLists.txt に新規ファイルを追加

## 完了条件

- `engineregistrationdialog.cpp` が 650行以下
- 新規ファイルがそれぞれ 600行以下
- ビルド成功
- 全テスト通過
- SettingsService の既存永続化ロジックが維持されていること

## 注意事項

- `connect()` にラムダを使わないこと
- SettingsService の getter/setter が存在する場合は維持する（CLAUDE.md の SettingsService Update Guidelines 参照）
- `.ui` ファイルが存在する場合はダイアログ本体に残す
- 分割後のクラスは `QObject` 継承 + parent ownership または `std::unique_ptr` で管理
