# Task 05: 所有権ガイドラインの拡充と寿命注釈の導入

## 概要

`docs/dev/ownership-guidelines.md` に所有権判定フローチャートとレビュー項目を追加し、再生成可能オブジェクトへの寿命注釈規約を導入する。

## 前提条件

- なし（ドキュメント更新のため他タスクと独立して実施可能）

## 現状

- `ownership-guidelines.md` に基本ルール（QObject親所有、unique_ptr、非所有参照、QPointer、ダブルポインタ）が記載済み
- 8カテゴリの所有権テーブルあり
- 課題:
  - 判定フローがなく、新規クラス作成時にどのパターンを選ぶか迷う
  - 再生成可能オブジェクト（`KifuLoadCoordinator`, `BoardSyncPresenter` 等）の寿命注釈がない
  - `QPointer` の採用基準が暗黙的
  - レビューチェックリストがない

## 実施内容

### Step 1: 所有権判定フローチャートの追加

以下の判定フローを `ownership-guidelines.md` に追記:

```
Q1: QObject派生クラスか？
  Yes → Q2: 親オブジェクトが明確か？
    Yes → parent ownership (new T(parent))
    No → Q3: 他のQObjectの子として管理されるか？
      Yes → 親を設定する
      No → unique_ptr + deleteLater or explicit delete
  No → Q4: ライフサイクルが所有者と一致するか？
    Yes → unique_ptr<T>
    No → Q5: shared access が必要か？
      Yes → 所有者が unique_ptr、他は raw T*
      No → unique_ptr<T>
```

### Step 2: QPointer採用基準の明文化

以下の条件をすべて満たす場合に `QPointer` を使用:
1. 参照先が QObject 派生
2. 参照先が参照元より先に破棄される可能性がある
3. 参照先が再生成される（`deleteLater` + `new` パターン）
4. 参照元が参照先の有効性を実行時にチェックする必要がある

現在の `QPointer` メンバ（MainWindow の6件）を例示として記載。

### Step 3: 寿命注釈規約の導入

再生成可能オブジェクトに対して、ヘッダーのメンバ変数宣言にコメントを付与する規約を制定:

```cpp
// Lifecycle: Created once in ensureX(), destroyed with parent
QPointer<BoardSyncPresenter> m_boardSync;

// Lifecycle: Recreated per kifu load (deleteLater + new)
QPointer<KifuLoadCoordinator> m_kifuLoadCoordinator;

// Lifecycle: Created once, destroyed in ~MainWindow via unique_ptr
std::unique_ptr<MainWindowLifecyclePipeline> m_pipeline;
```

注釈テンプレート:
- `Created once` — 1回生成、以後再生成なし
- `Recreated per <event>` — 特定イベントで再生成
- `Destroyed by <owner>` — 明示的破棄元

### Step 4: レビューチェックリストの追加

新規クラス/メンバ追加時のレビュー項目:
- [ ] 所有権パターンが判定フローに従っているか
- [ ] 再生成対象に `QPointer` が使われているか
- [ ] 寿命注釈がメンバ宣言に記載されているか
- [ ] `new` に対応する破棄パスが存在するか
- [ ] 裸の `delete` を使っていないか（`deleteLater` or `unique_ptr`）

### Step 5: 既存コードへの寿命注釈適用（任意）

優先度の高いファイルに注釈を適用:
- `src/app/mainwindow.h` — 全 `QPointer` メンバに適用
- `src/app/mainwindowruntimerefs.h` — 各サブ構造体に用途コメント追記

## 完了条件

- `ownership-guidelines.md` に判定フロー、QPointer基準、寿命注釈規約、レビューチェックリストが追加
- 既存 `QPointer` メンバ6件に寿命注釈が記載（任意）
- ビルド成功（コメント追加のみのため当然だが確認）

## 注意事項

- ドキュメントは日本語で記述（既存に合わせる）
- 既存のルールと矛盾しないこと
- 過度な注釈規約にしない（新規追加・再生成対象のみ必須、既存は任意）
