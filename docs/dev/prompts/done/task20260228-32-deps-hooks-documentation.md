# Task 32: Deps/Hooks/Refs 設計意図ドキュメント（Phase 4: ドキュメント整備）

## 目的

45クラスで使用される Deps/Hooks/Refs パターンの設計意図・使い分けルールを明文化する。

## 背景

- `Deps` 45クラス、`Hooks` 8クラス、`Refs` 9クラスで使用される主要パターン
- コードの一貫性から暗黙的に理解可能だが、明文化されたドキュメントがない
- 新規参画時の学習コスト削減、パターン適用判断の支援
- 包括的改善分析 §4.1, §10.2

## 成果物

- `docs/dev/deps-hooks-refs-pattern.md`

## 事前調査

### Step 1: パターン使用状況の実測

```bash
# Deps struct の使用クラス一覧
rg "struct Deps" src --type-add 'header:*.h' --type header -l

# Hooks struct の使用クラス一覧
rg "struct Hooks" src --type-add 'header:*.h' --type header -l

# Refs struct の使用クラス一覧
rg "struct Refs" src --type-add 'header:*.h' --type header -l

# updateDeps() の宣言
rg "void updateDeps" src --type-add 'header:*.h' --type header -l
```

### Step 2: パターンのバリエーション分析

```bash
# Deps の典型的な構成（ポインタ、コールバック）
rg -A 10 "struct Deps" src/ui/wiring/matchcoordinatorwiring.h

# Hooks の典型的な構成（std::function コールバック）
rg -A 20 "struct Hooks" src/game/matchcoordinator.h

# Refs の典型的な構成（参照ポインタ）
rg -A 10 "struct Refs" src/game/gameendhandler.h
```

### Step 3: パターン選択の判断基準の抽出

各パターンが使用されている文脈を分析し、選択基準を特定:
- いつ Deps を使うか
- いつ Hooks を使うか
- いつ Refs を使うか
- いつ updateDeps() が必要か

## 実装手順

### Step 4: ドキュメントの作成

```markdown
# Deps/Hooks/Refs パターン設計ガイド

## 1. 概要

本プロジェクトでは、依存注入に3種の構造体パターンを使い分けている。

| パターン | 方向 | 用途 |
|---|---|---|
| Deps | 外→内 | 入力依存の注入 |
| Hooks | 内→外 | 上位層への通知コールバック |
| Refs | 内→外 | 状態の読み取り参照 |

## 2. Deps パターン

### 2.1 いつ使うか
- クラスが外部オブジェクトへの参照を必要とする場合
- 遅延注入（ensure*パターン）で依存が段階的に揃う場合

### 2.2 構造
```cpp
struct Deps {
    ShogiGameController* gc = nullptr;
    ShogiView* view = nullptr;
    // コールバック（上位層の操作が必要な場合）
    std::function<void()> ensureSomething;
};
void updateDeps(const Deps& deps);
```

### 2.3 ルール
- 全メンバは nullptr/デフォルト初期化
- updateDeps() で更新（複数回呼ばれることがある）
- nullチェックは呼び出し側で行う

## 3. Hooks パターン

### 3.1 いつ使うか
- 下位層のクラスが上位層に通知/操作を要求する場合
- シグナル/スロットでは表現しにくい、同期的なコールバック

### 3.2 構造
```cpp
struct Hooks {
    struct UI {
        std::function<void(const QString&)> showMessage;
        // ...
    } ui;
    struct Time {
        std::function<int(Player)> remainingMsFor;
        // ...
    } time;
};
```

### 3.3 ルール
- game/ 層のクラスで主に使用
- ネストサブ構造体で責務を分類
- コンストラクタで受け取り、メンバに保存

## 4. Refs パターン

### 4.1 いつ使うか
- 読み取り専用の状態参照が必要な場合
- Deps との違い: 操作ではなく参照のみ

## 5. 判断フローチャート
```

### Step 5: 実例の追加

ドキュメント内に実際のコードからの具体例を含める:
- `MatchCoordinatorWiring` の Deps 例
- `MatchCoordinator` の Hooks 例（ネストサブ構造体）
- `GameEndHandler` の Refs 例

### Step 6: レビューと整合性確認

ドキュメントの内容が実際のコードと整合しているか確認。

## 完了条件

- [ ] `docs/dev/deps-hooks-refs-pattern.md` が作成されている
- [ ] 3パターンの設計意図・使い分けルールが明文化されている
- [ ] 実際のコードからの具体例が含まれている
- [ ] 判断フローチャート（いつどのパターンを使うか）が含まれている
- [ ] CLAUDE.md のアーキテクチャセクションからリンクされている

## KPI変化目標

- パターンドキュメント: 0 → 1

## 注意事項

- コードの変更は不要（ドキュメントのみ）
- 過度に形式的にしない（実用的なガイドを目指す）
- CLAUDE.md と重複する情報は最小限にとどめ、リンクで参照する
- 将来の変更に追従しやすいよう、具体的な行数やファイルパスは最小限にする
