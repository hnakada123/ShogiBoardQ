# Task 17: CI の厳格化

## 目的
翻訳未完了を段階的に fail 条件に近づけ、静的解析のカバレッジを拡張する。

## 背景
現在の CI 構成:
- `build-and-test`: Release + テスト実行
- `static-analysis`: clang-tidy チェック
- `translation-check`: 未翻訳数の warning 表示（fail しない）

## 作業内容

### 1. 翻訳チェックの厳格化
`.github/workflows/ci.yml` の `translation-check` ジョブを修正する:

#### Phase 1: 閾値ベースの警告
```yaml
# 現在の未翻訳数を閾値として設定
# 閾値を超えたら fail にする（新規未翻訳の追加を防止）
THRESHOLD=<現在の未翻訳数>
if [ "$total" -gt "$THRESHOLD" ]; then
    echo "::error::Translation regression: $total unfinished (threshold: $THRESHOLD)"
    exit 1
fi
```

#### Phase 2: 閾値の段階的引き下げ
翻訳が追加されるたびに閾値を下げていく運用ルールを `CLAUDE.md` に追記する。

### 2. Debug ビルドの CI 追加
新しいジョブ `build-debug` を追加する:
```yaml
build-debug:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v4
      - name: Install Qt6
        uses: jurplel/install-qt-action@v4
        with:
          version: '6.7.*'
          modules: 'qtcharts'
      - name: Configure (Debug)
        run: cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
      - name: Build
        run: cmake --build build --parallel 2
      - name: Test
        env:
          QT_QPA_PLATFORM: offscreen
        run: cd build && ctest --output-on-failure
```

### 3. clang-tidy チェックセットの拡張
`.clang-tidy` の設定を確認し、以下のチェックを段階的に追加する:
- `modernize-*`: C++17 モダン化チェック
- `performance-*`: パフォーマンス改善チェック
- `bugprone-*`: バグ潜在箇所の検出
- 既存の警告を先に解消してからチェックを追加する

### 4. 構造テストの CI 統合
Task 01 で追加した構造KPIテストが CI で自動実行されることを確認する。

### テスト実行
```bash
# ローカルで CI と同等のチェックを実行
cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] 翻訳チェックに閾値ベースの fail 条件が追加されている
- [ ] Debug ビルドが CI に追加されている
- [ ] clang-tidy のチェックセットが拡張されている（既存警告が 0 であること）
- [ ] 構造KPIテストが CI で実行されている

## 注意事項
- CI の変更は慎重に行う。既存の成功しているジョブを壊さない
- 翻訳の閾値は `grep -c 'type="unfinished"'` で現在値を確認してから設定する
- clang-tidy のチェック追加は、既存コードが PASS する範囲で行う（新規チェックで大量の警告が出る場合は除外リストで対応）
