# コード改善リスト

調査日: 2026-03-03

---

## 1. メモリリーク（高優先度）

### 1-1. JosekiRepository の raw new（メモリリーク）

- **ファイル**: `src/dialogs/josekiwindow.h:141`, `src/dialogs/josekiwindow.cpp:30`
- **問題**: `JosekiRepository` は QObject ではない通常のクラスだが、`new` で生成して raw ポインタ `JosekiRepository* m_repository` で保持している。デストラクタは `= default` のため `delete` されない。メモリリーク。
- **修正方法**:
  ```cpp
  // josekiwindow.h - 変更前
  JosekiRepository *m_repository = nullptr;

  // josekiwindow.h - 変更後
  #include <memory>
  std::unique_ptr<JosekiRepository> m_repository;
  ```
  ```cpp
  // josekiwindow.h - デストラクタ変更
  // 変更前
  ~JosekiWindow() override = default;
  // 変更後（ヘッダーで宣言、cppで定義）
  ~JosekiWindow() override;

  // josekiwindow.cpp に追加
  JosekiWindow::~JosekiWindow() = default;
  ```
  ```cpp
  // josekiwindow.cpp - コンストラクタ変更前
  , m_repository(new JosekiRepository())
  , m_presenter(new JosekiPresenter(m_repository, this))

  // josekiwindow.cpp - コンストラクタ変更後
  , m_repository(std::make_unique<JosekiRepository>())
  , m_presenter(new JosekiPresenter(m_repository.get(), this))
  ```
  - **注意**: `unique_ptr` で前方宣言型を使う場合、デストラクタを `.cpp` で定義する必要がある（完全型が必要）

### 1-2. DebugScreenshotService の raw new

- **ファイル**: `src/services/debugscreenshotwiring.h:31`, `src/services/debugscreenshotwiring.cpp:14`
- **問題**: `DebugScreenshotService` は QObject ではない通常のクラスだが、raw ポインタで保持。`#ifdef QT_DEBUG` 内のデバッグ専用コードだが所有権ルール違反。
- **修正方法**:
  ```cpp
  // debugscreenshotwiring.h - 変更前
  DebugScreenshotService* m_service = nullptr;

  // debugscreenshotwiring.h - 変更後
  #include <memory>
  std::unique_ptr<DebugScreenshotService> m_service;
  ```
  ```cpp
  // debugscreenshotwiring.h - デストラクタ追加
  ~DebugScreenshotWiring() override;

  // debugscreenshotwiring.cpp に追加
  DebugScreenshotWiring::~DebugScreenshotWiring() = default;
  ```
  ```cpp
  // debugscreenshotwiring.cpp - コンストラクタ変更前
  , m_service(new DebugScreenshotService(mainWindow))

  // debugscreenshotwiring.cpp - コンストラクタ変更後
  , m_service(std::make_unique<DebugScreenshotService>(mainWindow))
  ```
  ```cpp
  // service() の戻り値型変更
  DebugScreenshotService* service() const { return m_service.get(); }
  ```

---

## 2. 標準ライブラリヘッダーの不足（中優先度）

推移的インクルードに依存しており、依存先の変更でコンパイルエラーになるリスクがある。

### 2-1. `<functional>` の不足

| ファイル | 使用箇所 | 現在の経路 |
|---------|---------|-----------|
| `src/services/timekeepingservice.h` | `std::function<void(const QString&)>` 等（L64-65） | `<QString>` 経由（不安定） |
| `src/app/sessionlifecycledepsfactory.h` | `std::function` を多数使用（L23-40） | `sessionlifecyclecoordinator.h` 経由 |
| `src/app/mainwindowdepsfactory.h` | Callbacks 構造体内で `std::function` を多数使用 | 各 Wiring ヘッダー経由 |
| `src/app/kifunavigationdepsfactory.h` | `std::function` を使用 | `kifunavigationcoordinator.h` 経由 |

- **修正方法**: 各ファイルの `#include` セクションに `#include <functional>` を追加

### 2-2. `<memory>` の不足

- **ファイル**: `src/common/threadtypes.h`
- **問題**: `std::shared_ptr` と `std::make_shared` を使用しているが `<memory>` を直接インクルードしていない
- **修正方法**:
  ```cpp
  // threadtypes.h - 追加
  #include <memory>
  ```

### 2-3. `<optional>` の不足

- **ファイル**: `src/core/shogiboard.h`
- **問題**: `std::optional<SfenComponents>` 等を使用。`shogitypes.h` が `<optional>` をインクルードしているため動作するが、直接依存していない
- **修正方法**:
  ```cpp
  // shogiboard.h - 追加
  #include <optional>
  ```

---

## 3. `#include <QList>` の重複（低優先度）

18ファイルで `#include <QList>` が重複している。実害はない（インクルードガードで二重読込防止）が、リファクタリング時のコピペミス。重複する片方を削除する。

| ファイル | 重複行 |
|---------|--------|
| `src/app/mainwindowruntimerefs.h` | L7, L9 |
| `src/app/mainwindowstate.h` | L8, L11 |
| `src/app/gamerecordloadservice.h` | L4, L6 |
| `src/app/mainwindowcoreinitcoordinator.h` | L7, L9 |
| `src/kifu/gamerecordmodel.h` | L11, L12 |
| `src/kifu/kifubranchtree.h` | L10, L12 |
| `src/kifu/kifuapplylogger.h` | L7, L10 |
| `src/kifu/kifuapplyservice.h` | L9, L12 |
| `src/kifu/kifparsetypes.h` | L9, L10 |
| `src/kifu/kifucontentbuilder.h` | L10, L11 |
| `src/kifu/formats/kiftosfenconverter.h` | L8, L12 |
| `src/kifu/formats/ki2tosfenconverter.h` | L7, L11 |
| `src/kifu/formats/jkftosfenconverter.h` | L9, L10 |
| `src/kifu/formats/usentosfenconverter.h` | L10, L11 |
| `src/kifu/formats/usitosfenconverter.h` | L10, L11 |
| `src/views/shogiviewhighlighting.h` | L11, L13 |
| `src/ui/coordinators/kifuselectionsync.h` | L9, L10 |
| `src/widgets/branchtreemanager.h` | L8, L9 |

- **修正方法**: 各ファイルの2つ目の `#include <QList>` を削除

---

## 4. 未使用・冗長な `#include`（低優先度）

### 4-1. 未使用 `#include <QThread>`

- **ファイル**: `src/engine/usi.h:12`
- **問題**: `QThread` は `usi.h` でも `usi.cpp` でも参照されていない
- **修正方法**: `#include <QThread>` の行を削除

### 4-2. `#include <QWidget>` → 前方宣言で十分

以下のファイルは `QWidget*` をポインタ型としてのみ使用しているため、`#include <QWidget>` を `class QWidget;` の前方宣言に置き換えられる。

| ファイル | 行 | 使用箇所 |
|---------|-----|---------|
| `src/ui/controllers/jishogiscoredialogcontroller.h` | L9 | `void showDialog(QWidget*, ...)` のパラメータ型のみ |
| `src/ui/controllers/nyugyokudeclarationhandler.h` | L9 | `bool handleDeclaration(QWidget*, ...)` のパラメータ型のみ |

- **修正方法**:
  ```cpp
  // 変更前
  #include <QWidget>

  // 変更後
  class QWidget;
  ```
- **注意**: `src/common/dialogutils.h` も `QWidget*` のみだが、`applyFontToAllChildren` の `const QFont&` 引数には `<QFont>` のインクルードが別途必要。現状 `<QWidget>` が `<QFont>` を推移的にインクルードしているため、変更すると `<QFont>` の追加が必要になる。変更してもよいが効果は小さい。

---

## 5. `std::as_const()` の不足（低優先度）

プロジェクト規約（CLAUDE.md）では Qt コンテナの range-for ループには `std::as_const()` を付けることとしている。以下の箇所で不足している。

### 5-1. 実際にデタッチリスクがある箇所（非const コンテナ）

| ファイル | 行 | コンテナ | 修正方法 |
|---------|-----|---------|---------|
| `src/dialogs/enginesettingsoptionhandler.cpp` | 94 | `QList<EngineOptionCategory> categoryOrder` (非const) | `for (EngineOptionCategory category : std::as_const(categoryOrder))` |
| `src/engine/thinkinginfopresenter.cpp` | 99 | `QStringList batch` (非const) | `for (const QString& line : std::as_const(batch))` |

### 5-2. const コンテナだが規約統一のため修正すべき箇所

| ファイル | 行 | コンテナ | 修正方法 |
|---------|-----|---------|---------|
| `src/dialogs/engineregistrationdialog.cpp` | 246 | `const auto widgets` | `for (QWidget* widget : std::as_const(widgets))` |
| `src/dialogs/startgamedialog.cpp` | 387 | `const QList<QWidget*> widgets` | `for (QWidget* widget : std::as_const(widgets))` |
| `src/dialogs/startgamedialog.cpp` | 395 | `const QList<QComboBox*> comboBoxes` | `for (QComboBox* comboBox : std::as_const(comboBoxes))` |
| `src/dialogs/kifupastedialog.cpp` | 189 | `const auto widgets` | `for (QWidget* widget : std::as_const(widgets))` |
| `src/common/dialogutils.cpp` | 21 | `const auto children` | `for (QWidget* child : std::as_const(children))` |
| `src/ui/controllers/mainwindowappearancecontroller.cpp` | 70 | `const auto toolbars` | `for (QToolBar* tb : std::as_const(toolbars))` |
| `src/ui/controllers/mainwindowappearancecontroller.cpp` | 72 | `const auto buttons` | `for (QToolButton* b : std::as_const(buttons))` |
| `src/ui/wiring/menuwindowwiring.cpp` | 55 | `const auto menuBarActions` | `for (QAction* menuAction : std::as_const(menuBarActions))` |
| `src/ui/wiring/menuwindowwiring.cpp` | 85 | `const auto menuActions` | `for (QAction* action : std::as_const(menuActions))` |
| `src/dialogs/menuwindow.cpp` | 114 | `category.actions`（constref経由） | `for (QAction* action : std::as_const(category.actions))` |
| `src/dialogs/menuwindow.cpp` | 225 | `const QList<QAction*>& actions`（constref） | `for (QAction* action : std::as_const(actions))` |

---

## 修正の優先順位まとめ

| 優先度 | カテゴリ | 件数 | 影響 |
|--------|---------|------|------|
| **高** | メモリリーク（JosekiRepository） | 1 | 定跡ウィンドウ使用時にメモリリーク |
| **中** | 標準ヘッダー不足（`<functional>`, `<memory>`, `<optional>`） | 6 | 推移的インクルード変更時にコンパイルエラー |
| **中** | raw new（DebugScreenshotService） | 1 | 所有権ルール違反 |
| **低** | `#include <QList>` 重複 | 18 | コード品質（実害なし） |
| **低** | 未使用 `#include` | 1 | コンパイル時間微増 |
| **低** | `#include` → 前方宣言 | 2 | コンパイル時間微減 |
| **低** | `std::as_const()` 不足 | 13 | clazy 警告・規約統一 |
