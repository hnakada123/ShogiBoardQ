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
│       ├─ Yes → 親を設定する（`MainWindowComposer` で parent 引数を渡す）
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
- 合成ルート（`MainWindowComposer`、[composition-root.md](composition-root.md)）が生成する QObject には、親（通常は MainWindow。コード上は `&m_window`）を必ず渡す

**parent を設定する基準:**
- `QWidget` 派生 → 必ず親ウィジェットを指定（レイアウト管理のため）
- `QObject` 派生で MainWindow 寿命と同一 → MainWindow（`&m_window`）または `this` を親に
- `MainWindowComposer` が生成するオブジェクト → MainWindow（`&m_window`）を親に指定

```cpp
// Good — parent ownership
auto* controller = new ReplayController(parent);

// Good — 合成ルートでの生成（mainwindowcomposer_create.cpp、MainWindow を親に指定）
m_c.dialogCoordinatorWiring = new DialogCoordinatorWiring(&m_window);
m_c.timeController = new TimeControlController(&m_window);
```

### 2. 非QObject クラス — `std::unique_ptr`

- `std::unique_ptr` を使用してRAIIで管理する
- `std::make_unique<T>(...)` を優先
- 裸の `new` + 生ポインタ保持は原則禁止

**unique_ptr を使用する基準:**
- 非QObject クラス（`GameModeStrategy`, `HumanTurnPolicy` 等）
- QObject 派生だが parent chain 不要の場合（`CsaGameWiring`、`EvaluationGraphController` など `MainWindowComponents` の `std::unique_ptr` メンバ）
- MainWindow が排他的に寿命を管理するサービス

```cpp
// Good — unique_ptr（MatchCoordinator が持つ対局の種類ごとの戦略）
std::unique_ptr<GameModeStrategy> m_strategy;
m_strategy = std::make_unique<HumanVsHumanStrategy>(...);

// Bad — 裸の new + 生ポインタ
GameModeStrategy* m_strategy = new HumanVsHumanStrategy(...);
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
// OK — parent 即時設定（mainwindowcomposer_create.cpp）
m_c.boardSync = new BoardSyncPresenter(bs, &m_window);
m_c.gameSession = new GameSession(&m_window);

// NG — parent なしで裸の new
auto* obj = new SomeQObject();  // 誰が破棄するのか不明
```

### 5. Wiring クラスの生成ルール

Wiring クラス（`*Wiring`）の生成（`new` / `std::make_unique`）は、合成ルート `MainWindowComposer`（`src/app/mainwindowcomposer_*.cpp`）で起動時に1回だけ行い、`MainWindowComponents` のメンバに置く。ほかのクラスで直接 `new` してはならない。

`UiActionsWiring` は `MainWindowComposer::connectSignals()`（`mainwindowcomposer_finalize.cpp`）が生成する。アクション別の Wiring（`FileActionsWiring` など）は `UiActionsWiring` が子として生成する。

```cpp
// Good — 合成ルートで生成（mainwindowcomposer_create.cpp）
void MainWindowComposer::createServices()
{
    // ...
    m_c.branchNavWiring = std::make_unique<BranchNavigationWiring>();
    // ...
}

// NG — 部品の中で別の部品を直接 new
*m_deps.dialogCoordinatorWiringPtr = new DialogCoordinatorWiring(m_deps.mainWindow);
```

## 再生成オブジェクトの寿命管理

### 再生成パターン

アプリケーションのライフサイクル中に破棄・再生成されるオブジェクトがある。
これらは特に注意が必要で、寿命コメントとダングリングポインタ対策が求められる。

**再生成されるオブジェクト:**

| オブジェクト | 再生成タイミング | 所有者 |
|-------------|-----------------|--------|
| `Usi`（エンジン） | 対局・検討・詰み探索の開始ごと（`EngineLifecycleManager::createEngine()` で作り、`destroyEngines()` などで `deleteLater`） | `EngineLifecycleManager` (QObject parent) |
| `QProcess` (エンジン) | エンジン起動/停止ごと | `EngineProcessManager` (QObject parent) |

`MatchCoordinator`・`GameStartCoordinator`・`KifuLoadCoordinator` は再生成しない。

| オブジェクト | 生成タイミング | 所有者 |
|-------------|---------------|--------|
| `MatchCoordinator` | 起動時に1回（`MainWindowComposer::createServices()`） | MainWindow (QObject parent) |
| `GameStartCoordinator` | 起動時に1回（`MainWindowComposer::createServices()`） | MainWindow (QObject parent) |
| `KifuLoadCoordinator` | 起動時に1回（`KifuLoadCoordinatorFactory::createAndWire()`、`MainWindowComposer::wireKifu()` から） | MainWindow (QObject parent) |

**寿命コメントの記法:**

```cpp
// Lifetime: owned by MainWindow (QObject parent=&m_window)
// Created: once at startup by MainWindowComposer, never recreated
m_c.timeController = new TimeControlController(&m_window);

// Lifetime: owned by EngineLifecycleManager (QObject parent=this)
// Recreated: per game / consideration / mate search start (deleteLater in destroySeat())
auto* engine = new Usi(comm, think, this);
```

### 寿命注釈規約

再生成可能オブジェクトや `QPointer` メンバには、ヘッダーのメンバ変数宣言に寿命注釈コメントを付与する。

**注釈テンプレート:**

| テンプレート | 意味 | 例 |
|-------------|------|-----|
| `Created once in <method>` | 1回生成、以後再生成なし | `MainWindowComposer::createServices()` で1回だけ生成 |
| `Recreated per <event>` | 特定イベントで破棄・再生成 | 対局開始ごと（エンジン） |
| `Destroyed by <owner>` | 明示的な破棄元がある | `unique_ptr` 破棄、`deleteLater` |

**ヘッダーでの記述例:**

```cpp
// Lifecycle: Created once in MainWindowComposer::createServices(), destroyed with parent
QPointer<BoardSyncPresenter> m_boardSync;

// Lifecycle: created once at startup by MainWindowComposer (parent=MainWindow), never recreated
MatchCoordinator* m_match = nullptr;

// Lifecycle: Created once, destroyed in ~MainWindow via unique_ptr
std::unique_ptr<MainWindowComposer> m_composer;
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
// QPointer — 参照先が破棄されると自動的に nullptr になる（DialogLauncher。閉じると破棄される局面集ビューア）
QPointer<SfenCollectionDialog> m_sfenCollectionDialog;

// 明示的な nullptr 代入（QPointer でない場合の代替策。EngineLifecycleManager::destroySeat()）
Usi* engine = std::exchange(target.engine, nullptr);  // 破棄を予約する前にクリア
engine->deleteLater();
```

## QPointer の使用基準

`QPointer<T>` は QObject 派生型にのみ使用可能な「寿命追跡付き非所有参照」である。
参照先が破棄されると自動的に `nullptr` になる。

### QPointer を使用すべき場合

1. **参照先が再生成される場合**: 対局ごとに破棄・再生成される `Usi`（エンジン）など
2. **参照先の寿命が参照元より短い可能性がある場合**: ダイアログやエンジンプロセス等
3. **既に nullptr チェックを行っている箇所**: QPointer の自動 null 化が安全性を向上
4. **外部管理のオブジェクトへの参照**: 所有者が別にいて、予期しないタイミングで破棄される可能性
5. **コントローラ/ハンドラが保持する非所有メンバ変数**: セッターで受け取る QObject ポインタで、
   参照先のライフサイクルが独立しているもの（例: `ReplayController::m_match`）

### QPointer を使用しなくてよい場合

1. **Deps/Refs 構造体のメンバ**: 構造体自体が短寿命（関数スコープ）のため不要
2. **unique_ptr が管理するオブジェクト**: 所有権が明確で、破棄タイミングも制御下
3. **寿命が参照元と同一の場合**: 同じ親の子オブジェクト間の参照等
4. **一度だけ作られ、終了まで変わらない参照先**: `MainWindowComponents::match` / `gameStart` は
   `MainWindowComposer` が起動時に一度だけ作り（親は MainWindow）、作り直さないので生ポインタで持つ（使う前に nullptr を確認する）。
   受け取る側のコントローラでは QPointer を使用する（「消費側クラスでの QPointer 適用」）

### 使用例

```cpp
// MainWindowComponents — 一度だけ作られる司令塔への参照（親は MainWindow。MainWindowComposer が起動時に作る）
MatchCoordinator* match = nullptr;
KifuLoadCoordinator* kifuLoadCoordinator = nullptr;  // 親は MainWindow。起動時に作る

// コントローラ — 所有者が別にいる QObject への非所有参照
QPointer<MatchCoordinator> m_match;  // GameStartCoordinator・ReplayController

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

## 表示モデルの再設定

- 思考・検討欄の delegate はビューごとに再利用する。モデルの `modelReset` や
  `setModels()` のたびに生成すると、置き換え前の delegate が親ビューの破棄まで残る。
- 選択を他のビューと共有しない `QTableView` のモデル切替には
  `TableModelUtils::setModel()` を使用する。旧選択モデルに加えて、ヘッダーが生成する
  不要な選択モデルも解放する。外部ビューと選択を共有する場合はこのヘルパーを使わない。
- 通信ログなど追記型の表示データには保持上限を設け、長時間使用時の増加を制限する。

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

### MainWindow と MainWindowComponents のメンバ

MainWindow 自身が持つのは `ui` と `m_composer`（`MainWindowComposer`）だけである。部品は `MainWindowComposer` が持つ `MainWindowComponents`（`src/app/mainwindowcomponents.h`）に置く。

| カテゴリ | 所有方法 | 例 |
|---------|---------|-----|
| UI フレームワーク | `std::unique_ptr`（MainWindow） | `ui` (Ui::MainWindow) |
| 合成ルート | `std::unique_ptr`（MainWindow） | `m_composer` |
| 非QObject サービス | `std::unique_ptr`（MainWindowComponents） | `humanTurnPolicy` |
| 親を持たない QObject | `std::unique_ptr`（MainWindowComponents） | `evalGraphController`, `turnStateSync`, `csaGameWiring` |
| QObject コントローラ | QObject parent（MainWindow）+ 生ポインタ | `timeController`, `replayController`, `boardSync`, `commentCoordinator`, `match`, `gameStart` |
| 他の部品が作る QObject（一度だけ作る） | 生ポインタ | `dialogCoordinator`（`dialogCoordinatorWiring` が作る）, `gameInfoController`（`playerInfo` が作る） |
| ほかの部品が作る QObject | `QPointer<T>` | `boardController`（`BoardSetupController` が作る） |
| UI ウィジェット | QObject parent (レイアウト経由) | `shogiView`, `evalChart` |

## 消費側クラスでの QPointer 適用

所有者が別にいるオブジェクト（`MatchCoordinator`, `GameStartCoordinator` 等）への非所有参照を
保持するコントローラ/ハンドラクラスでは、メンバ変数に `QPointer<T>` を使用する。

どちらも現在は `MainWindowComposer` が起動時に一度だけ作り（親は MainWindow）、作り直さない。
それでも所有者は受け取る側ではないので、セッターで受け取る消費側クラスは `QPointer` で破棄に備える。
`MainWindowComponents` 側は生ポインタで持つ。MainWindow を親とする同じ寿命の部品（`BoardSetupController`・
`TimeControlController` など）は、「QPointer を使用しなくてよい場合」の 3 に当たるので生ポインタで持つ。

**対象クラスの例:**

| クラス | QPointer メンバ | 理由 |
|--------|----------------|------|
| `GameStartCoordinator` | `m_match` | MC の所有者は MainWindow（合成ルートが作る） |
| `ReplayController` | `m_match` | MC の所有者は MainWindow（合成ルートが作る） |

## PRレビュー観点

1. **新規 `delete` の追加** → 上記の例外パターンに該当するか確認。該当しない場合は `unique_ptr` または parent ownership への置換を求める
2. **新規 `new`** → QObject 派生なら `parent` 指定があるか確認。非QObject なら `unique_ptr` を使用しているか確認
3. **所有者の明確性** → 誰が破棄責任を持つか、コードから追跡可能か
4. **破棄順序** → 依存関係がある場合、安全な順序で破棄されるか（`unique_ptr` の宣言順序 = 逆順破棄に注意）
5. **`delete` 後の `nullptr` 代入** → ポインタを再利用する場合、`delete` 直後に `nullptr` を代入しているか
6. **寿命コメント** → 起動時以外に生成される `new`（エンジンなど）に寿命コメントがあるか
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
