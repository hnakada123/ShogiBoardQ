# 所有権ガイドライン

本ドキュメントは、ShogiBoardQ における C++ オブジェクトの所有権（ライフタイム管理）ルールを定める。
PRレビュー時の判断基準として使用する。

## 所有権判定フローチャート

新規クラスやメンバ変数を追加する際、以下のフローに従って所有権パターンを選択する。

```
Q1: QObject 派生クラスか？
├─ Yes → Q2: 親オブジェクトが明確か？
│   ├─ Yes → parent ownership (new T(parent))
│   └─ No → Q3: 他の QObject の子として管理可能か？
│       ├─ Yes → 親を設定する（ensure* で parent 引数を渡す）
│       └─ No → unique_ptr + deleteLater or explicit delete
└─ No → Q4: ライフサイクルが所有者と一致するか？
    ├─ Yes → unique_ptr<T>
    └─ No → Q5: shared access が必要か？
        ├─ Yes → 所有者が unique_ptr、他は raw T*（非所有参照）
        └─ No → unique_ptr<T>
```

**補足:**
- Q1 で Yes かつ再生成されるオブジェクトの場合 → 「再生成オブジェクトの寿命管理」セクションも参照
- 非所有参照（raw `T*`）は Deps/Refs 構造体で受け渡す
- 再生成対象への非所有参照は `QPointer<T>` を検討（「QPointer の使用基準」セクション参照）

## 基本ルール

### 1. QObject 派生クラス — parent ownership

- コンストラクタで `parent` を指定し、親の破棄に連動させる
- `new MyWidget(this)` — `this` が親となり、`this` の破棄時に自動 delete
- CompositionRoot の `ensure*()` パターンでは `parent` 引数を必ず渡す

**parent を設定する基準:**
- `QWidget` 派生 → 必ず親ウィジェットを指定（レイアウト管理のため）
- `QObject` 派生で MainWindow 寿命と同一 → `&m_mw` または `this` を親に
- Registry/Bootstrapper が生成するオブジェクト → `&m_mw` を親に指定

```cpp
// Good — parent ownership
auto* controller = new GameStateController(parent);

// Good — ensure* パターン（mainwindowcompositionroot.cpp）
void ensureDialogCoordinator(..., QObject* parent, ...)
{
    wiring = new DialogCoordinatorWiring(parent);
}

// Good — Registry での生成（&m_mw を親に指定）
m_mw.m_timeController = new TimeControlController(&m_mw);
```

### 2. 非QObject クラス — `std::unique_ptr`

- `std::unique_ptr` を使用してRAIIで管理する
- `std::make_unique<T>(...)` を優先
- 裸の `new` + 生ポインタ保持は原則禁止

**unique_ptr を使用する基準:**
- 非QObject クラス（`GameEndHandler`, `EvaluationGraphController` 等）
- QObject 派生だが parent chain 不要の場合（`MatchCoordinatorWiring` 等）
- MainWindow が排他的に寿命を管理するサービス

```cpp
// Good — unique_ptr
std::unique_ptr<GameEndHandler> m_gameEndHandler;
m_gameEndHandler = std::make_unique<GameEndHandler>(...);

// Bad — 裸の new + 生ポインタ
GameEndHandler* m_gameEndHandler = new GameEndHandler(...);
```

### 3. 非所有参照（借用）— 生ポインタ `T*` / `QPointer<T>`

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

### 4. 裸の `new` が許容される場合

QObject 派生クラスで、`new` の直後に parent が設定される場合のみ裸の `new` を許容する:

```cpp
// OK — parent 即時設定
m_mw.m_boardSync = new BoardSyncPresenter(d, &m_mw);

// OK — ensure* パターンで parent 引数あり
controller = new GameStateController(parent);

// NG — parent なしで裸の new
auto* obj = new SomeQObject();  // 誰が破棄するのか不明
```

## 再生成オブジェクトの寿命管理

### 再生成パターン

アプリケーションのライフサイクル中に破棄・再生成されるオブジェクトがある。
これらは特に注意が必要で、寿命コメントとダングリングポインタ対策が求められる。

**再生成されるオブジェクト:**

| オブジェクト | 再生成タイミング | 所有者 |
|-------------|-----------------|--------|
| `MatchCoordinator` | 対局セッション開始ごと | `MatchCoordinatorWiring` (QObject parent) |
| `GameStartCoordinator` | MC 再生成に連動 | `MatchCoordinatorWiring` (QObject parent) |
| `KifuLoadCoordinator` | 棋譜読込ごと（`deleteLater`） | ファクトリ生成（QObject parent） |
| `QProcess` (エンジン) | エンジン起動/停止ごと | `EngineProcessManager` (QObject parent) |

**寿命コメントの記法:**

```cpp
// Lifetime: owned by MainWindow (QObject parent)
// Created: once at startup, never recreated
m_mw.m_timeController = new TimeControlController(&m_mw);

// Lifetime: owned by MatchCoordinatorWiring (QObject parent)
// Recreated: on each game session start via initializeSession()
m_mw.m_match = m_mw.m_matchWiring->match();
```

### 寿命注釈規約

再生成可能オブジェクトや `QPointer` メンバには、ヘッダーのメンバ変数宣言に寿命注釈コメントを付与する。

**注釈テンプレート:**

| テンプレート | 意味 | 例 |
|-------------|------|-----|
| `Created once in <method>` | 1回生成、以後再生成なし | `ensureX()` で1回だけ生成 |
| `Recreated per <event>` | 特定イベントで破棄・再生成 | 棋譜読込ごと、セッション開始ごと |
| `Destroyed by <owner>` | 明示的な破棄元がある | `unique_ptr` 破棄、`deleteLater` |

**ヘッダーでの記述例:**

```cpp
// Lifecycle: Created once in ensureX(), destroyed with parent
QPointer<BoardSyncPresenter> m_boardSync;

// Lifecycle: Recreated per kifu load (deleteLater + new)
QPointer<KifuLoadCoordinator> m_kifuLoadCoordinator;

// Lifecycle: Created once, destroyed in ~MainWindow via unique_ptr
std::unique_ptr<MainWindowLifecyclePipeline> m_pipeline;
```

**適用範囲:**
- **必須**: 新規追加する `QPointer` メンバ、再生成対象メンバ
- **任意**: 既存メンバ（修正時に追記を推奨）
- **不要**: Deps/Refs 構造体のメンバ（短寿命かつ非所有が自明）

### ダングリングポインタ防止

再生成オブジェクトへの非所有参照は、破棄時にダングリングポインタとなるリスクがある。
以下の対策を適用する:

1. **QPointer の使用**: 再生成される QObject への MainWindow メンバ参照に `QPointer<T>` を使用
2. **明示的な nullptr 代入**: 再生成前に参照ポインタを nullptr にクリア
3. **nullptr ガード**: 使用前に必ず nullptr チェック

```cpp
// QPointer — 参照先が破棄されると自動的に nullptr になる
QPointer<MatchCoordinator> m_match;

// 明示的な nullptr 代入（QPointer でない場合の代替策）
m_mw.m_match = nullptr;  // 旧 MC 破棄前にクリア
```

## QPointer の使用基準

`QPointer<T>` は QObject 派生型にのみ使用可能な「寿命追跡付き非所有参照」である。
参照先が破棄されると自動的に `nullptr` になる。

### QPointer を使用すべき場合

1. **参照先が再生成される場合**: `MatchCoordinator` のようにセッション単位で破棄・再生成される
2. **参照先の寿命が参照元より短い可能性がある場合**: ダイアログやエンジンプロセス等
3. **既に nullptr チェックを行っている箇所**: QPointer の自動 null 化が安全性を向上
4. **外部管理のオブジェクトへの参照**: 所有者が別にいて、予期しないタイミングで破棄される可能性
5. **コントローラ/ハンドラが保持する非所有メンバ変数**: セッターで受け取る QObject ポインタで、
   参照先のライフサイクルが独立しているもの（例: `GameStateController::m_match`）

### QPointer を使用しなくてよい場合

1. **Deps/Refs 構造体のメンバ**: 構造体自体が短寿命（関数スコープ）のため不要
2. **unique_ptr が管理するオブジェクト**: 所有権が明確で、破棄タイミングも制御下
3. **寿命が参照元と同一の場合**: 同じ親の子オブジェクト間の参照等
4. **ダブルポインタ（`T**`）パターンで参照される場合**: `GameSessionOrchestrator::Deps` 等の
   ダブルポインタは `QPointer<T>*` に変更する必要があり、テンプレート `deref()` との互換性が
   崩れるため、MainWindow の直接メンバには適用しない。代わりに消費側クラスで QPointer を使用する

### 使用例

```cpp
// MainWindow — 再生成されるオブジェクトへの参照（ダブルポインタ互換のため raw pointer）
MatchCoordinator* m_match = nullptr;          // ダブルポインタ経由で参照されるため QPointer 不可
QPointer<KifuLoadCoordinator> m_kifuLoadCoordinator;  // deleteLater で破棄されるため QPointer

// コントローラ — 再生成オブジェクトへの非所有参照
QPointer<MatchCoordinator> m_match;  // MC はセッション単位で再生成される

// 外部管理オブジェクトへの参照
QPointer<ShogiBoard> m_board;  // 寿命は外部管理（QPointerで安全）
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
| 再生成オブジェクトへの生ポインタ（QPointer 不使用） | `QPointer<T>` |

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

## 所有権カテゴリ一覧

### MainWindow の直接メンバ

| カテゴリ | 所有方法 | 例 |
|---------|---------|-----|
| UI フレームワーク | `std::unique_ptr` | `ui` (Ui::MainWindow) |
| インフラ層 | `std::unique_ptr` | `m_compositionRoot`, `m_registry`, `m_signalRouter` |
| 非QObject サービス | `std::unique_ptr` | `m_evalGraphController`, `m_playModePolicy` |
| QObject コントローラ | QObject parent (`&m_mw`) | `m_timeController`, `m_replayController` |
| 再生成される QObject（ダブルポインタ経由） | 生ポインタ + nullptr ガード | `m_match`, `m_gameStart` |
| 再生成される QObject（単一参照） | `QPointer<T>` | `m_kifuLoadCoordinator` |
| 非所有（CompositionRoot 生成） | `QPointer<T>` | `m_posEdit`, `m_boardSync`, `m_commentCoordinator` |
| UI ウィジェット | QObject parent (レイアウト経由) | `m_shogiView`, `m_evalChart` |

## 消費側クラスでの QPointer 適用

再生成されるオブジェクト（`MatchCoordinator`, `GameStartCoordinator` 等）への非所有参照を
保持するコントローラ/ハンドラクラスでは、メンバ変数に `QPointer<T>` を使用する。

MainWindow 側ではダブルポインタ（`T**`）パターンとの互換性のため生ポインタを維持するが、
セッターで受け取る消費側クラスでは `QPointer` で安全にダングリングを防止する。

**対象クラスの例:**

| クラス | QPointer メンバ | 理由 |
|--------|----------------|------|
| `GameStartCoordinator` | `m_match` | MC はセッション単位で再生成 |
| `GameStateController` | `m_match` | MC はセッション単位で再生成 |
| `ReplayController` | `m_match` | MC はセッション単位で再生成 |
| `BoardSetupController` | `m_match` | MC はセッション単位で再生成 |
| `ConsecutiveGamesController` | `m_gameStart` | GSC は MC 再生成に連動 |

## PRレビュー観点

1. **新規 `delete` の追加** → 上記の例外パターンに該当するか確認。該当しない場合は `unique_ptr` または parent ownership への置換を求める
2. **新規 `new`** → QObject 派生なら `parent` 指定があるか確認。非QObject なら `unique_ptr` を使用しているか確認
3. **所有者の明確性** → 誰が破棄責任を持つか、コードから追跡可能か
4. **破棄順序** → 依存関係がある場合、安全な順序で破棄されるか（`unique_ptr` の宣言順序 = 逆順破棄に注意）
5. **`delete` 後の `nullptr` 代入** → ポインタを再利用する場合、`delete` 直後に `nullptr` を代入しているか
6. **寿命コメント** → Registry/CompositionRoot で生成される `new` に寿命コメントがあるか
7. **再生成オブジェクト** → 再生成されるオブジェクトへの参照に `QPointer` を使用しているか

## レビューチェックリスト

新規クラスやメンバ変数を追加する PR で確認すべき項目:

- [ ] 所有権パターンが判定フローチャートに従っているか
- [ ] 再生成対象オブジェクトへの参照に `QPointer` が使われているか
- [ ] 寿命注釈が `QPointer` メンバ・再生成対象メンバの宣言に記載されているか
- [ ] `new` に対応する破棄パス（parent / `unique_ptr` / `deleteLater`）が存在するか
- [ ] 裸の `delete` を使っていないか（`deleteLater` or `unique_ptr` を優先）
- [ ] Deps/Refs 構造体のメンバが全て非所有参照であるか
- [ ] `unique_ptr` の宣言順序が破棄順序として安全か（逆順破棄）
