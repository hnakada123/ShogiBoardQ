# Task 21: Sanitizer定期実行化（Phase 1: ガードレール強化）

## 目的

既存の `sanitize` ジョブを週次cron実行に拡張し、ランタイム不具合の自動検出を定期化する。

## 背景

- Task 14 で `sanitize` ジョブを追加済み（手動トリガーまたはPRラベルで起動）
- 定期実行がないため、回帰が長期間見逃されるリスクがある
- cron トリガーの追加は3行程度の変更で済む
- 包括的改善分析 §9.2.4, §12 P2

## 対象ファイル

- `.github/workflows/ci.yml`

## 事前調査

### Step 1: 現在の sanitize ジョブの確認

```bash
# sanitize ジョブの内容を確認
rg -A 30 "sanitize:" .github/workflows/ci.yml
```

### Step 2: 現在のトリガー設定の確認

```bash
rg -A 10 "^on:" .github/workflows/ci.yml
```

## 実装手順

### Step 3: schedule トリガーの追加

`.github/workflows/ci.yml` の `on:` セクションに `schedule` を追加:

```yaml
on:
  push:
    branches: [main]
  pull_request:
  workflow_dispatch:
  schedule:
    - cron: '0 3 * * 1'  # 毎週月曜 03:00 UTC (日本時間 12:00)
```

### Step 4: sanitize ジョブの条件を更新

schedule トリガーでも sanitize ジョブが実行されるよう `if` 条件を更新:

```yaml
  sanitize:
    runs-on: ubuntu-latest
    if: >
      github.event_name == 'workflow_dispatch' ||
      github.event_name == 'schedule' ||
      contains(github.event.pull_request.labels.*.name, 'sanitize')
```

### Step 5: schedule 時に他のジョブが不要に実行されないよう確認

schedule トリガーで不要なジョブが実行されないよう、他のジョブの `if` 条件を必要に応じて追加:

```yaml
  build-and-test:
    if: github.event_name != 'schedule'
    # ...

  build-debug:
    if: github.event_name != 'schedule'
    # ...

  static-analysis:
    if: github.event_name != 'schedule'
    # ...

  translation-check:
    if: github.event_name != 'schedule'
    # ...
```

### Step 6: 検証

```bash
# CI YAML の構文検証（act がインストールされている場合）
# act -l
# または yamllint
yamllint .github/workflows/ci.yml 2>/dev/null || echo "yamllint not installed, skip"
```

## 完了条件

- [ ] `schedule` トリガーが `.github/workflows/ci.yml` に追加されている
- [ ] `sanitize` ジョブが schedule トリガーで実行される
- [ ] schedule 時に sanitize 以外のジョブが実行されない
- [ ] 既存の push/PR トリガーに影響がない

## KPI変化目標

- Sanitizer定期実行: 手動のみ → 週次自動

## 注意事項

- cron 実行は GitHub Actions の無料枠を消費するため、週1回に抑える
- schedule イベントは `push`/`pull_request` と異なり、デフォルトブランチ（main）でのみ動作する
- schedule 実行の結果は Actions タブで確認可能
