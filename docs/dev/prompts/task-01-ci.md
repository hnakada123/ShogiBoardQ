# Task 1: CI パイプライン構築

ShogiBoardQ プロジェクトに GitHub Actions CI パイプラインを構築してください。

## 要件

`.github/workflows/ci.yml` を新規作成し、以下のジョブを定義する。

### ジョブ 1: build-and-test

- **トリガー**: push（main）、pull_request
- **環境**: Ubuntu latest
- **Qt6 セットアップ**: `jurplel/install-qt-action` を使用し、Qt 6 の Widgets, Charts, Network, LinguistTools モジュールをインストール
- **ビルド手順**:
  ```bash
  cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
  cmake --build build --parallel
  ```
- **テスト実行**:
  ```bash
  cd build && ctest --output-on-failure
  ```
  テスト実行時には `QT_QPA_PLATFORM=offscreen` 環境変数を設定すること

### ジョブ 2: static-analysis

- **環境**: Ubuntu latest（Qt6 もインストール）
- **clang-tidy チェック**:
  ```bash
  cmake -B build -S . -DENABLE_CLANG_TIDY=ON
  cmake --build build --parallel 2>&1 | tee build.log
  ```
  clang-tidy の警告がある場合はステップを失敗させる

### ジョブ 3: translation-check

- **翻訳未完了チェック**:
  - `resources/translations/*.ts` ファイル内の `type="unfinished"` の件数を集計
  - 結果を Job Summary に出力
  - 件数が増加した場合に警告（fail はさせない）

## 制約

- CLAUDE.md のビルド手順に準拠すること
- Qt Creator ではなく cmake コマンドラインビルドを使用
- テストは `tests/CMakeLists.txt` で定義済み（`add_shogi_test` マクロ）

## 確認事項

- YAML の文法が正しいこと
- 作成後、`act` 等でのローカル検証は不要（構文のみ確認）
