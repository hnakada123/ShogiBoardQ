# Task 01: 構造KPIテスト追加

## 目的
コード品質の指標を自動検証するテストを追加し、今後のリファクタリング進捗を定量的に追跡可能にする。

## 背景
現在の指標（2026-02-27時点）:
- `src/app/mainwindow.h`: 534行
- `MainWindow` の `friend class`: 11件
- `MainWindow` の `ensure*` 宣言: 38件
- 600行超の `.cpp` ファイル: 約40件

## 作業内容

### 1. テストファイル作成
`tests/tst_structural_kpi.cpp` を新規作成する。

### 2. テストケース
以下の構造KPIを検証するテストを実装する:

#### a) ファイル行数上限テスト
- `src/` 配下の全 `.cpp` / `.h` ファイルをスキャンする
- 現時点で 600行超のファイルを「既知の例外リスト」としてハードコードする
- 例外リスト以外のファイルが 600行を超えたら FAIL にする
- 例外リストのファイルが縮小された場合は、上限値も更新する仕組みにする（コメントで明記）

#### b) MainWindow friend class 上限テスト
- `src/app/mainwindow.h` の `friend class` 行数をカウント
- 現在値 11 を上限としてテスト（今後段階的に下げる）

#### c) MainWindow ensure* 上限テスト
- `src/app/mainwindow.h` の `void ensure` 行数をカウント
- 現在値 38 を上限としてテスト

#### d) 1クラスあたり公開メソッド上限テスト（オプション）
- `public slots:` セクションのメソッド数をカウント
- 過度な肥大化を検知

### 3. CMakeLists.txt 更新
- `tests/CMakeLists.txt` に新テストを追加する
- Qt Test フレームワークを使用する

### 4. テスト実行確認
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] `tst_structural_kpi.cpp` が追加され、全テストが PASS する
- [ ] 既存の全テスト（31件）が引き続き PASS する
- [ ] CI で新テストが自動実行される

## 注意事項
- テスト内のファイルパスは `CMAKE_SOURCE_DIR` から相対パスで指定する（`QFINDTESTDATA` または CMake 定義を使用）
- ファイル行数カウントは `QFile` + `readAll().count('\n')` で実装可能
- 既知の例外リストは `QMap<QString, int>` でファイル名→現在行数のペアを管理する
