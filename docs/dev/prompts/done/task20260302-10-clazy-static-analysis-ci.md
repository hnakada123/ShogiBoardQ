# Task 10: clazy CI + static-analysis ジョブ補強（P2 / §5.2+§5.3）

## 概要

1. clazy を CI に追加して、コーディング規約の自動検証を行う
2. 既存の `static-analysis` ジョブの不備を修正する

## 現状

### clazy の不在（§5.2）
- CLAUDE.md で「clazy level0,level1 の警告が出ないようにコードを書くこと」と明記
- CI に clazy ジョブが存在しない

### static-analysis の不備（§5.3）
- `BUILD_TESTING=ON` が未設定 → テストコードが解析されない
- `CMAKE_BUILD_TYPE` が未設定

## 手順

### Step 1: static-analysis ジョブの修正

1. `.github/workflows/ci.yml` の `static-analysis` ジョブを修正:
   ```yaml
   - name: Configure
     run: cmake -B build -S .
       -DENABLE_CLANG_TIDY=ON
       -DCMAKE_BUILD_TYPE=Release
       -DBUILD_TESTING=ON
   ```

### Step 2: clazy ジョブの追加

1. `.github/workflows/ci.yml` に `clazy` ジョブを追加:
   ```yaml
   clazy:
     runs-on: ubuntu-latest
     steps:
       - uses: actions/checkout@v4
       - name: Install Qt
         uses: jurplel/install-qt-action@v4
         with:
           version: '6.7.*'
           modules: 'qtcharts'
       - name: Install clazy
         run: sudo apt-get install -y clazy
       - name: Configure
         run: cmake -B build -S .
           -DCMAKE_CXX_COMPILER=clazy
           -DCMAKE_BUILD_TYPE=Release
         env:
           CLAZY_CHECKS: "level0,level1"
       - name: Build with clazy
         run: cmake --build build 2>&1 | tee clazy-output.txt
       - name: Check for warnings
         run: |
           if grep -q "warning:" clazy-output.txt; then
             echo "::error::clazy warnings found"
             grep "warning:" clazy-output.txt
             exit 1
           fi
   ```

### Step 3: ローカルでの clazy 実行確認

1. ローカル環境で clazy ビルドを実行し、既存の警告がないことを確認:
   ```bash
   cmake -B build-clazy -S . -DCMAKE_CXX_COMPILER=clazy
   CLAZY_CHECKS="level0,level1" cmake --build build-clazy 2>&1 | grep "warning:"
   ```
2. 警告がある場合は修正する

### Step 4: テスト

1. CI のワークフローをローカルで `act` 等でテストするか、PR を作成して CI 実行結果を確認する

## 注意事項

- clazy の `level0,level1` には `range-loop-detach`, `fully-qualified-moc-types`, `const-signal-or-slot` 等が含まれる
- `CLAZY_CHECKS` 環境変数で有効なチェックを制御する
- 初回実行で大量の警告が出る可能性がある。その場合は段階的に対応する
