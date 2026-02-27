# Task 10: JosekiWindow Presenter/Repository 分離

## 目的
`JosekiWindow` から操作フローとファイルI/Oを分離し、各ファイル 600行以下を達成する。

## 前提
- Task 09 の QAbstractTableModel 導入が完了していること

## 作業内容

### Phase A: JosekiRepository 抽出
`src/dialogs/josekirepository.h/.cpp` を新規作成する。

以下の責務を移動する:
- 定跡ファイルの読み込み（`loadJosekiFile` 等）
- 定跡ファイルの保存（`saveJosekiFile` 等）
- データフォーマットの変換
- ファイルパスの管理

### Phase B: JosekiPresenter 抽出
`src/dialogs/josekipresenter.h/.cpp` を新規作成する。

以下の責務を移動する:
- 定跡データの検索・フィルタリングロジック
- マージ・移動・削除の操作フロー
- 盤面との連携ロジック（SFEN 受け渡し）
- バリデーション処理

### Phase C: JosekiWindow の薄い View 化
`JosekiWindow` に残す責務:
- UI要素の初期化（`.ui` ファイルとの接続）
- ユーザーイベントを Presenter に委譲するスロット
- Presenter からの表示更新を受け取るスロット
- `JosekiPresenter` と `JosekiRepository` の所有・初期化

### Phase D: シグナル/スロット接続の整理
- `JosekiWindow` → `JosekiPresenter` の委譲接続
- `JosekiPresenter` → `JosekiWindow` の表示更新接続
- `JosekiPresenter` → `JosekiRepository` のI/O呼び出し
- ラムダは使用しない（CLAUDE.md のルール）

### テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] `josekiwindow.cpp` が 600行以下
- [ ] `josekipresenter.cpp` が 600行以下
- [ ] `josekirepository.cpp` が 600行以下
- [ ] 全テストが PASS する（特に `tst_josekiwindow.cpp`）
- [ ] 構造KPIテストの例外リストから `josekiwindow.cpp` を削除可能

## 注意事項
- 分離の順序は Repository → Presenter の順が安全（I/O は独立性が高い）
- Phase ごとにビルド＋テストを実行する
- JosekiWindow の既存シグナルが外部（MainWindow 等）から接続されている箇所を `grep` で確認し、変更しないよう注意する
- CLAUDE.md のコードスタイル（connect でラムダ禁止、std::as_const 等）を厳守する
