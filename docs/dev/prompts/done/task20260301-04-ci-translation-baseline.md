# Task 04: CI翻訳チェックをベースライン回帰禁止方式へ変更

## 概要

`.github/workflows/ci.yml` の `translation-check` ジョブを「固定閾値超過でfail」から「前回ベースラインからの増加でfail」方式に変更する。

## 前提条件

- なし（CI設定変更のため他タスクと独立して実施可能）

## 現状

- 現在の方式: `THRESHOLD=1202` を固定値として、未翻訳数がこれを超えたら fail
- 問題点: 閾値が高すぎて実質的に退行を検知できない。翻訳が増えても減っても気づかない
- 目標: 未翻訳数の増加を常時禁止する

## 実施内容

### Step 1: ベースラインファイルの導入

1. 現在の未翻訳数を計測:
   ```bash
   grep -c 'type="unfinished"' resources/translations/*.ts
   ```
2. `resources/translations/baseline-unfinished.txt` を作成し、各ファイルの未翻訳数を記録:
   ```
   ShogiBoardQ_en.ts:XXX
   ShogiBoardQ_ja_JP.ts:XXX
   ```

### Step 2: CI ジョブの改修

1. `translation-check` ジョブを改修:
   ```yaml
   - name: Check translation regression
     run: |
       baseline_file="resources/translations/baseline-unfinished.txt"
       failed=false
       while IFS=: read -r file expected; do
         actual=$(grep -c 'type="unfinished"' "resources/translations/$file" || true)
         if [ "$actual" -gt "$expected" ]; then
           echo "::error::Translation regression in $file: $actual > $expected (baseline)"
           failed=true
         elif [ "$actual" -lt "$expected" ]; then
           echo "::warning::Translation improved in $file: $actual < $expected. Update baseline!"
         fi
       done < "$baseline_file"
       if [ "$failed" = true ]; then exit 1; fi
   ```
2. ベースラインを下回った場合は warning を出し、ベースライン更新を促す

### Step 3: ドキュメント更新

1. `CLAUDE.md` の「Translation Threshold (CI)」セクションを更新
2. ベースライン更新手順を記載:
   ```
   # 翻訳を追加したらベースラインを更新
   grep -c 'type="unfinished"' resources/translations/*.ts | \
     sed 's|resources/translations/||' > resources/translations/baseline-unfinished.txt
   ```

## 完了条件

- `ci.yml` の `translation-check` がベースライン方式に変更
- ベースラインファイルが作成されている
- CI が正常に動作する（既存の翻訳状態で pass する）
- CLAUDE.md の該当セクションが更新されている

## 注意事項

- ベースラインファイルは git 管理する
- 翻訳を追加した際のベースライン更新を忘れると CI で warning が出る設計にする
- `THRESHOLD` 変数は削除する
