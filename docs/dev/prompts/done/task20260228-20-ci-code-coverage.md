# Task 20: コードカバレッジCI導入（Phase 1: ガードレール強化）

## 目的

CI に gcovr ベースのコードカバレッジレポートを追加し、テストギャップを数値的に可視化する。

## 背景

- テストカバレッジの数値的な可視化がなく、改善の効果測定が困難
- 全体カバレッジは推定 ~16% で、UI/App/Widget層が 0%
- CI に追加することで PR ごとのカバレッジ変化を追跡可能にする
- 包括的改善分析 §9.2.1, §12 P1

## 対象ファイル

- `.github/workflows/ci.yml`

## 事前調査

### Step 1: 現在のCI構成確認

```bash
cat .github/workflows/ci.yml
```

### Step 2: gcovr の動作確認（ローカル）

```bash
# gcovr がインストールされていなければ
pip install gcovr

# カバレッジ付きビルド
cmake -B build-cov -S . -DBUILD_TESTING=ON \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage"

cmake --build build-cov --parallel $(nproc)

# テスト実行
QT_QPA_PLATFORM=offscreen ctest --test-dir build-cov --output-on-failure

# レポート生成
gcovr --root src --object-directory build-cov \
  --xml build-cov/coverage.xml \
  --html build-cov/coverage.html \
  --print-summary
```

## 実装手順

### Step 3: CI に coverage ジョブを追加

`.github/workflows/ci.yml` に新規ジョブを追加:

```yaml
  coverage:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4

      - name: Install Qt6
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.7.*'
          modules: 'qtcharts'

      - name: Install gcovr
        run: pip install gcovr

      - name: Configure with Coverage
        run: |
          cmake -B build -S . -DBUILD_TESTING=ON \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCMAKE_CXX_FLAGS="--coverage -fprofile-arcs -ftest-coverage"

      - name: Build
        run: cmake --build build --parallel 2

      - name: Test
        env:
          QT_QPA_PLATFORM: offscreen
        run: cd build && ctest --output-on-failure

      - name: Generate Coverage Report
        run: |
          gcovr --root src --object-directory build \
            --xml build/coverage.xml \
            --html-details build/coverage.html \
            --print-summary | tee build/coverage-summary.txt

      - name: Coverage Summary
        run: |
          echo "## Code Coverage" >> $GITHUB_STEP_SUMMARY
          echo '```' >> $GITHUB_STEP_SUMMARY
          cat build/coverage-summary.txt >> $GITHUB_STEP_SUMMARY
          echo '```' >> $GITHUB_STEP_SUMMARY

      - name: Upload Coverage Report
        uses: actions/upload-artifact@v4
        with:
          name: coverage-report
          path: |
            build/coverage.xml
            build/coverage.html
            build/coverage-summary.txt
```

### Step 4: Codecov 連携（オプション）

Codecov を利用する場合:

```yaml
      - name: Upload to Codecov
        uses: codecov/codecov-action@v4
        with:
          files: build/coverage.xml
          fail_ci_if_error: false
```

### Step 5: ローカル検証

```bash
# ローカルでカバレッジ付きビルド・テスト・レポート生成が成功することを確認
```

### Step 6: 既存ジョブへの影響確認

既存の4ジョブ（build-and-test, build-debug, static-analysis, translation-check）に影響がないことを確認。

## 完了条件

- [ ] `coverage` ジョブが `.github/workflows/ci.yml` に追加されている
- [ ] CI Step Summary にカバレッジサマリが表示される
- [ ] カバレッジレポート（HTML/XML）が artifact としてアップロードされる
- [ ] 既存CIジョブに影響がない
- [ ] ローカルでカバレッジレポート生成が動作する

## KPI変化目標

- コードカバレッジCI: 0 → 1 ジョブ
- カバレッジ数値の可視化: なし → あり

## 注意事項

- カバレッジビルドは Debug モードで行う（最適化によるカバレッジ不正確を防止）
- `--parallel 2` で CI のメモリ制限を考慮
- `fail_ci_if_error: false` で Codecov 連携の失敗がCIをブロックしないようにする
- カバレッジ閾値（最低カバレッジ%）は将来タスクで設定する（現時点では可視化のみ）
