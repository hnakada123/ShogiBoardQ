# Step 05: MainWindowCompositionRoot 導入

## 設計書

`docs/dev/mainwindow-refactor-1-2-3-plan.md` のフェーズ1・ステップ5に対応。

## 前提

Step 01〜04 が完了済み（3箇所の `ensure*()` がすべて factory 化済み）。

## 背景

factory 化で Deps 組み立てはクリーンになったが、`MainWindow` にはまだ「いつ factory を呼ぶか」「オブジェクトの生成と初回配線」の責務が残っている。これを `MainWindowCompositionRoot` に集約する。

## タスク

### 1. MainWindowCompositionRoot の作成

- 配置: `src/app/mainwindowcompositionroot.h`, `src/app/mainwindowcompositionroot.cpp`
- 役割: 「いつ factory を呼ぶか」のみを担当し、実体生成順を制御する
- **所有権は `MainWindow` 側に残す**: CompositionRoot はポインタを受け取って構築・配線するが、`unique_ptr` や `delete` は行わない

### 2. 主要 API

```cpp
class MainWindowCompositionRoot {
public:
    explicit MainWindowCompositionRoot(QObject* parent = nullptr);

    // 各 ensure メソッド: MainWindow の同名メソッドから委譲される
    // refs: 現時点のランタイム参照スナップショット
    // 戻り値またはoutパラメータで生成されたオブジェクトを返す

    void ensureDialogCoordinator(const MainWindowRuntimeRefs& refs,
                                 /* callback params */,
                                 DialogCoordinatorWiring*& wiring,
                                 DialogCoordinator*& coordinator);

    void ensureKifuFileController(const MainWindowRuntimeRefs& refs,
                                  /* callback params */,
                                  KifuFileController*& controller);

    void ensureRecordNavigationWiring(const MainWindowRuntimeRefs& refs,
                                      RecordNavigationWiring*& wiring);
};
```

API設計は上記を参考に、実装しやすい形に調整してよい。

### 3. MainWindow の ensure メソッドを CompositionRoot に委譲

各 `ensure*()` メソッドの実装を `MainWindowCompositionRoot` への委譲に変更:

**Before（Step 02-04 完了後の状態）:**
```cpp
void MainWindow::ensureDialogCoordinator()
{
    if (m_dialogCoordinator) return;
    if (!m_dialogCoordinatorWiring)
        m_dialogCoordinatorWiring = new DialogCoordinatorWiring(this);
    auto refs = buildRuntimeRefs();
    auto deps = MainWindowDepsFactory::createDialogCoordinatorDeps(refs, ...);
    m_dialogCoordinatorWiring->ensure(deps);
    m_dialogCoordinator = m_dialogCoordinatorWiring->coordinator();
}
```

**After:**
```cpp
void MainWindow::ensureDialogCoordinator()
{
    if (m_dialogCoordinator) return;
    auto refs = buildRuntimeRefs();
    m_compositionRoot->ensureDialogCoordinator(refs, /* callbacks */,
        m_dialogCoordinatorWiring, m_dialogCoordinator);
}
```

### 4. MainWindow に CompositionRoot メンバを追加

- `MainWindowCompositionRoot* m_compositionRoot` を `MainWindow` のメンバに追加
- コンストラクタの早い段階で生成（他の ensure よりも前）

### 5. CMakeLists.txt 更新

新規ファイルを追加。

## 制約

- 外部から見た挙動は一切変更しない
- 生成順序は既存の `ensure*()` 呼び出し順をそのまま維持
- CompositionRoot は所有権を持たない（`new` で生成した結果は `MainWindow` のメンバに格納）
- `MainWindowDepsFactory` は CompositionRoot 内部で使う（MainWindow から直接呼ばなくなる）

## 回帰確認

ビルド成功後、以下を手動確認:
- アプリ起動直後のメニュー操作
- 棋譜ファイル読み込み/保存/エクスポート
- 棋譜行クリックでの盤面追従

## 完了条件

- `MainWindowCompositionRoot` が存在し、3つの `ensure*` を提供
- `MainWindow` の各 `ensure*()` は CompositionRoot への委譲のみ（生成ロジックなし）
- `MainWindowDepsFactory` は CompositionRoot からのみ呼ばれる
- `cmake -B build -S . && cmake --build build` が成功する
- コミットメッセージ: 「MainWindowCompositionRoot 導入: ensure* の生成ロジックを集約」
