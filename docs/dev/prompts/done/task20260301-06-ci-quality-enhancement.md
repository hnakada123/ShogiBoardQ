# Task 20260301-06: CI 品質ゲート補強（P2）

## 背景

CIには build-and-test, build-debug, static-analysis, sanitize, coverage, translation-check の6ジョブが存在するが、
以下の観点で「実行しているが保証していない」領域がある:

1. **Sanitizer**: `sanitize` ジョブは手動/週次のみで、PR では実行されない（ラベル `sanitize` 付与時のみ）
2. **依存削減チェック（IWYU）**: include の適切性を自動検証する仕組みがない
3. **翻訳更新漏れ**: `translation-check` はベースライン比較のみで、`tr()` 変更と翻訳更新の整合性は見ていない

### 現在の CI 構成

```yaml
# sanitize ジョブの条件
if: >
  github.event_name == 'workflow_dispatch' ||
  github.event_name == 'schedule' ||
  contains(github.event.pull_request.labels.*.name, 'sanitize')
```

## 作業内容

### Phase 1: Sanitizer ジョブの常時化準備

#### Step 1-1: Sanitizer ビルドオプションの CMake 化

`CMakeLists.txt` に Sanitizer オプションを追加する:

```cmake
option(ENABLE_SANITIZERS "Enable AddressSanitizer and UBSan" OFF)

if(ENABLE_SANITIZERS)
    add_compile_options(-fsanitize=address,undefined -fno-sanitize=vptr -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address,undefined -fno-sanitize=vptr)
endif()
```

#### Step 1-2: CI ジョブの更新

`.github/workflows/ci.yml` の `sanitize` ジョブを更新:

```yaml
sanitize:
  runs-on: ubuntu-latest
  # 週次 + 手動 + ラベル（既存条件を維持）
  if: >
    github.event_name == 'workflow_dispatch' ||
    github.event_name == 'schedule' ||
    contains(github.event.pull_request.labels.*.name, 'sanitize')
  steps:
    # ...
    - name: Configure with Sanitizers
      run: cmake -B build -S . -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo -DENABLE_SANITIZERS=ON
```

**注意**: PR での常時実行はビルド時間の観点から見送り。週次実行の安定化を先に行う。

#### Step 1-3: Sanitizer 失敗時のドキュメント

`docs/dev/ci-troubleshooting.md` を新規作成:

```markdown
# CI トラブルシューティング

## Sanitizer ジョブの失敗

### AddressSanitizer (ASan)
- **heap-buffer-overflow**: 配列の境界外アクセス
- **use-after-free**: 解放済みメモリへのアクセス
- **stack-buffer-overflow**: スタック変数の境界外アクセス

### UndefinedBehaviorSanitizer (UBSan)
- **signed-integer-overflow**: 符号付き整数のオーバーフロー
- **null**: NULL ポインタのデリファレンス
- **alignment**: アラインメント違反

### 対処方法
1. ローカルで再現: `cmake -B build -S . -DENABLE_SANITIZERS=ON && cmake --build build && cd build && ctest`
2. ASAN_OPTIONS でリーク検出を無効化済み: `detect_leaks=0`
3. Qt 内部の vptr 警告は `-fno-sanitize=vptr` で抑制済み
```

### Phase 2: include 監視ジョブの導入

#### Step 2-1: include カウント監視スクリプトの作成

`scripts/check-include-counts.sh` を新規作成:

```bash
#!/bin/bash
# 主要ファイルの include 数を監視
# 閾値超過時に警告を出力

set -euo pipefail

THRESHOLD_FILE="resources/include-baseline.txt"
FAILED=false

while IFS=: read -r file limit; do
    actual=$(grep -c '#include' "$file" 2>/dev/null || echo 0)
    if [ "$actual" -gt "$limit" ]; then
        echo "::error::Include count regression in $file: $actual > $limit"
        FAILED=true
    fi
done < "$THRESHOLD_FILE"

if [ "$FAILED" = true ]; then exit 1; fi
echo "All include counts within limits"
```

#### Step 2-2: ベースラインファイルの作成

`resources/include-baseline.txt`:
```
src/app/mainwindow.cpp:49
src/app/mainwindowcompositionroot.cpp:35
src/app/mainwindowserviceregistry.cpp:10
```

#### Step 2-3: CI ジョブへの追加

`ci.yml` に include チェックステップを追加（`build-and-test` ジョブ内）:

```yaml
      - name: Include Count Check
        run: bash scripts/check-include-counts.sh
```

### Phase 3: 翻訳整合性チェックの強化

#### Step 3-1: tr() 変更検出スクリプト

`scripts/check-translation-sync.sh` を新規作成:

```bash
#!/bin/bash
# git diff で tr() を含む行が変更されている場合、
# .ts ファイルも同時に変更されているか確認

set -euo pipefail

BASE_BRANCH="${1:-origin/main}"

# tr() を含む変更があるか
TR_CHANGES=$(git diff "$BASE_BRANCH" -- 'src/**/*.cpp' 'src/**/*.h' | grep -c 'tr(' || true)

if [ "$TR_CHANGES" -gt 0 ]; then
    # .ts ファイルの変更があるか
    TS_CHANGES=$(git diff --name-only "$BASE_BRANCH" -- 'resources/translations/*.ts' | wc -l)
    if [ "$TS_CHANGES" -eq 0 ]; then
        echo "::warning::tr() strings were modified but no .ts files were updated. Run: cmake --build build --target update_translations"
    fi
fi
```

#### Step 3-2: CI への統合

PR 時のみ実行するステップとして追加:

```yaml
      - name: Translation Sync Check
        if: github.event_name == 'pull_request'
        run: bash scripts/check-translation-sync.sh origin/${{ github.base_ref }}
```

### Step 4: ビルド・テスト確認

```bash
# CMake の Sanitizer オプション確認
cmake -B build -S . -DENABLE_SANITIZERS=ON -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build build
cd build && ctest --output-on-failure

# スクリプトの動作確認
bash scripts/check-include-counts.sh
bash scripts/check-translation-sync.sh HEAD~1
```

## 完了条件

- [x] CMakeLists.txt に ENABLE_SANITIZERS オプションが追加される
- [x] sanitize ジョブが CMake オプション経由で動作する
- [x] `docs/dev/ci-troubleshooting.md` が作成される
- [x] include カウント監視スクリプトとベースラインが作成される
- [x] 翻訳整合性チェックスクリプトが作成される
- [x] CI の各ジョブが正常動作すること
- [x] 全テスト pass
