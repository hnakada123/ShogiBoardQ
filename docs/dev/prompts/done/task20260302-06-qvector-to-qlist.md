# Task 06: `QVector` → `QList` 移行（P2 / §2.3）

## 概要

Qt6 では `QVector` は `QList` のエイリアスだが、CLAUDE.md の規約で `QList` に統一することが明記されている。約400箇所の `QVector` 使用を `QList` に移行する。

## 現状

- `src/` 配下で約402行が `QVector` を使用
- ヘッダ・ソースファイル全体に分散
- `#include <QVector>` も多数存在

## 手順

### Step 1: 一括置換の準備

1. `QVector` の使用箇所を全てリストアップする
2. `QVector` が型エイリアスとしてのみ使われていること（独自の `QVector` サブクラスがないこと）を確認する
3. シグナル・スロットのパラメータで `QVector` を使っている箇所を特定する（MOC の正規化に影響）

### Step 2: 一括置換の実行

1. 全 `.h`, `.cpp` ファイルで以下を置換:
   - `QVector<` → `QList<`
   - `#include <QVector>` → `#include <QList>`
   - `QVector &` / `QVector&` → `QList &` / `QList&`
2. テンプレート引数内の `QVector` も漏れなく置換する
3. コメント内の `QVector` は文脈に応じて置換（技術的なコメントは更新、一般的な説明はそのまま）

### Step 3: MOC 関連の確認

1. `signals:` / `public slots:` で `QVector` を使っているシグネチャがないか確認
2. 存在する場合は `QList` に変更（Qt6 では MOC が `QList` に正規化するため）
3. `Q_DECLARE_METATYPE(QVector<...>)` がある場合は `QList` に変更

### Step 4: ビルド・テスト

1. `cmake --build build` でビルドが通ることを確認
2. テストを実行して全パスを確認
3. `grep -r "QVector" src/` でゼロ件になることを確認

## 注意事項

- Qt6 では `QVector` = `QList` なので動作変更はない。純粋なコードスタイル統一
- `src/core/shogimove.h` の `#include <QVector>` は Task 02 で触れない場合ここで対応する
- テストファイル (`tests/`) 内の `QVector` も同様に置換する
