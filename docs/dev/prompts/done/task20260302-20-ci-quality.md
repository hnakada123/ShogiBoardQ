# Task 20: CI セキュリティ + 品質向上（P3 / §5.4+§5.5+§5.6）

## 概要

CI パイプラインのセキュリティと品質を向上させる3つの小改善をまとめて実施する。

## 改善項目

### 20-A: カバレッジ外部サービス連携（§5.4）

`coverage` ジョブが HTML/XML を Artifact にアップロードしているが、Codecov 等に送信されていない。

### 20-B: GitHub Actions SHA ピン留め（§5.5）

`actions/checkout@v4` 等がタグ参照のみ。

### 20-C: Qt バージョン下限指定（§5.6）

`find_package(Qt6 ...)` にバージョン下限がない。

## 手順

### Step 1: カバレッジ外部サービス連携

1. Codecov（無料 OSS プラン）の導入を検討する
2. `.github/workflows/ci.yml` の `coverage` ジョブに Codecov アップロードステップを追加:
   ```yaml
   - name: Upload to Codecov
     uses: codecov/codecov-action@v4
     with:
       files: build/coverage.xml
       fail_ci_if_error: false
     env:
       CODECOV_TOKEN: ${{ secrets.CODECOV_TOKEN }}
   ```
3. `codecov.yml` 設定ファイルを作成（カバレッジ閾値等）
4. README にカバレッジバッジを追加する（ユーザーが要望した場合）

### Step 2: GitHub Actions SHA ピン留め

1. 使用中の全アクションのコミット SHA を調べる:
   - `actions/checkout@v4` → `actions/checkout@<sha>`
   - `jurplel/install-qt-action@v4` → `jurplel/install-qt-action@<sha>`
   - `actions/upload-artifact@v4` → `actions/upload-artifact@<sha>`
2. タグ参照を SHA 参照に変更し、コメントでバージョンを記載:
   ```yaml
   - uses: actions/checkout@<sha>  # v4.1.7
   ```

### Step 3: Qt バージョン下限指定

1. `CMakeLists.txt` の `find_package(Qt6 ...)` に最低バージョンを追加:
   ```cmake
   find_package(Qt6 6.7 REQUIRED COMPONENTS Widgets Charts Network LinguistTools)
   ```
2. CI で使用しているバージョン `6.7.*` に合わせて `6.7` を指定する

### Step 4: ビルド・テスト

1. ビルドが通ることを確認
2. CI ワークフローの yaml 構文をチェック（`yamllint` 等）

## 注意事項

- Codecov のトークンは GitHub Secrets に設定が必要。パブリックリポジトリの場合は不要な場合もある
- SHA ピン留めは Dependabot での自動更新を設定すると便利（`.github/dependabot.yml`）
- Qt バージョン下限を `6.7` にすると、6.5/6.6 ユーザーはビルドできなくなる。意図的かどうか確認する
