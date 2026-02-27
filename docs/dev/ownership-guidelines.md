# 所有権ガイドライン

本ドキュメントは、ShogiBoardQ における C++ オブジェクトの所有権（ライフタイム管理）ルールを定める。
PRレビュー時の判断基準として使用する。

## 基本ルール

### 1. QObject 派生クラス — parent ownership

- コンストラクタで `parent` を指定し、親の破棄に連動させる
- `new MyWidget(this)` — `this` が親となり、`this` の破棄時に自動 delete
- CompositionRoot の `ensure*()` パターンでは `parent` 引数を必ず渡す

```cpp
// Good — parent ownership
auto* controller = new GameStateController(parent);

// Good — ensure* パターン（mainwindowcompositionroot.cpp）
void ensureDialogCoordinator(..., QObject* parent, ...)
{
    wiring = new DialogCoordinatorWiring(parent);
}
```

### 2. 非QObject クラス — `std::unique_ptr`

- `std::unique_ptr` を使用してRAIIで管理する
- `std::make_unique<T>(...)` を優先
- 裸の `new` + 生ポインタ保持は原則禁止

```cpp
// Good — unique_ptr
std::unique_ptr<GameEndHandler> m_gameEndHandler;
m_gameEndHandler = std::make_unique<GameEndHandler>(...);

// Bad — 裸の new + 生ポインタ
GameEndHandler* m_gameEndHandler = new GameEndHandler(...);
```

### 3. 非所有参照（借用）— 生ポインタ `T*`

- 生ポインタ (`T*`) は「所有しない」ことを意味する
- Deps 構造体・Refs 構造体のメンバは全て非所有参照
- ライフサイクルは所有者が管理し、借用側は破棄しない

```cpp
// Good — Deps 構造体は非所有参照のみ
struct Deps {
    ShogiView* shogiView = nullptr;       // 所有しない
    ShogiGameController* gc = nullptr;    // 所有しない
};
```

## モデルが所有するアイテム

`AbstractListModel<T>` 派生クラス（`KifuRecordListModel`、`ShogiEngineThinkingModel` 等）は、
格納アイテムの所有権をモデルが持つ。アイテム削除時は手動 `delete` が必要。

```cpp
// KifuRecordListModel::removeLastItem()
auto* ptr = list.takeLast();
delete ptr;  // 所有権がモデル側にある前提
```

このパターンでは `delete` の使用が許可される。コメントで所有権の前提を明記すること。

## QProcess の再生成パターン

`EngineProcessManager` のように、オブジェクトのライフサイクル中に子オブジェクトを
破棄・再生成するケースでは、parent ownership と手動 `delete` を併用する。

```cpp
// startProcess() — parent ownership で生成
m_process = new QProcess(this);

// stopProcess() — 明示的に破棄して再生成に備える
delete m_process;
m_process = nullptr;
```

## 禁止パターン

| パターン | 代替手段 |
|---------|---------|
| 裸の `new` + 生ポインタ保持（非QObject） | `std::unique_ptr` |
| 関数内 `new` + 手動 `delete`（スコープ内完結） | `std::unique_ptr` またはスタック変数 |
| `delete` 後にポインタを放置 | `delete` 直後に `nullptr` を代入 |

## 例外が許される場合

以下のケースでは手動 `delete` の使用を許可する。理由をコメントで明記すること。

1. **Qt の `takeItem()` / `takeWidget()` 系API** — 所有権がコンテナから呼び出し側に移転するため
   ```cpp
   // engineregistrationdialog.cpp
   delete ui->engineListWidget->takeItem(i);  // takeItem で所有権移転
   ```

2. **QLayout のアイテム解放** — レイアウト解体時の手動クリーンアップ
   ```cpp
   // flowlayout.cpp
   while (auto* item = takeAt(0)) {
       delete item;
   }
   ```

3. **モデル所有アイテムの削除** — 上記「モデルが所有するアイテム」を参照

4. **QProcess 等の再生成パターン** — parent ownership 下でも明示破棄が必要な場合

## PRレビュー観点

1. **新規 `delete` の追加** → 上記の例外パターンに該当するか確認。該当しない場合は `unique_ptr` または parent ownership への置換を求める
2. **新規 `new`** → QObject 派生なら `parent` 指定があるか確認。非QObject なら `unique_ptr` を使用しているか確認
3. **所有者の明確性** → 誰が破棄責任を持つか、コードから追跡可能か
4. **破棄順序** → 依存関係がある場合、安全な順序で破棄されるか（`unique_ptr` の宣言順序 = 逆順破棄に注意）
5. **`delete` 後の `nullptr` 代入** → ポインタを再利用する場合、`delete` 直後に `nullptr` を代入しているか
