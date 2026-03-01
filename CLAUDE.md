# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

ShogiBoardQ is a Japanese chess (Shogi) board application built with Qt6 and C++17. It supports game play, kifu (game record) management, USI engine integration for analysis, and CSA network play.

## Build Commands

```bash
# Configure and build
cmake -B build -S .
cmake --build build

# Or with ninja (faster)
cmake -B build -S . -G Ninja
ninja -C build

# Run the application
./build/ShogiBoardQ

# With static analysis
cmake -B build -S . -DENABLE_CLANG_TIDY=ON
cmake -B build -S . -DENABLE_CPPCHECK=ON

# Update translations (.ts files from source, then rebuild .qm)
cmake --build build --target update_translations
cmake --build build --target translations
```

## Architecture

### Source Organization (`src/`)

ヘッダーファイル（.h）は対応するソースファイル（.cpp）と同じディレクトリに配置されています。

- **app/**: アプリケーションエントリーポイントと委譲先サービス
  - `main.cpp` - Entry point
  - `mainwindow.cpp/.h/.ui` - Main window
  - `mainwindowcompositionroot.cpp/.h` - ensure*生成ロジック集約
  - `mainwindowuibootstrapper.cpp/.h` - UI初期化ステップのオーケストレーション
  - `playmodepolicyservice.cpp/.h` - プレイモード依存の判定ロジック
  - `turnstatesyncservice.cpp/.h` - 手番同期（TurnManager/GC/View/時計）
  - `gamerecordupdateservice.cpp/.h` - 棋譜追記・ライブセッション更新
  - `boardloadservice.cpp/.h` - SFEN盤面適用・ハイライト同期
  - `kifunavigationcoordinator.cpp/.h` - 棋譜ナビゲーション同期

- **core/**: 純粋なゲームロジック（Qt GUI非依存）
  - `shogiboard.cpp/.h` - Board state and piece management
  - `shogimove.cpp/.h` - Move representation
  - `movevalidator.cpp/.h` - Move legality validation
  - `shogiclock.cpp/.h` - Game clock
  - `shogiutils.cpp/.h` - Utilities

- **game/**: ゲーム進行・対局管理
  - `matchcoordinator.cpp/.h` - Match orchestration
  - `shogigamecontroller.cpp/.h` - Game flow control
  - `gamestartcoordinator.cpp/.h` - Game start flow
  - `turnmanager.cpp/.h` - Turn management

- **kifu/**: 棋譜関連
  - `gamerecordmodel.cpp/.h` - Kifu data model
  - `kifuloadcoordinator.cpp/.h` - Kifu loading
  - `kifuioservice.cpp/.h` - Kifu file I/O
  - **formats/**: Format converters
    - `kiftosfenconverter`, `ki2tosfenconverter`, `csatosfenconverter`, etc.

- **analysis/**: 解析機能
  - `analysiscoordinator.cpp/.h` - Analysis orchestration
  - `analysisflowcontroller.cpp/.h` - Analysis flow control
  - `considerationflowcontroller.cpp/.h` - Consideration mode

- **engine/**: USI (Universal Shogi Interface) エンジン連携
  - `usi.cpp/.h` - Main engine interface
  - `usiprotocolhandler.cpp/.h` - USI protocol implementation
  - `engineprocessmanager.cpp/.h` - Engine process lifecycle
  - `enginesettingscoordinator.cpp/.h` - Engine settings

- **network/**: CSA通信対局
  - `csaclient.cpp/.h` - CSA protocol client
  - `csagamecoordinator.cpp/.h` - CSA game coordination

- **navigation/**: ナビゲーション機能
  - `navigationcontroller.cpp/.h` - Navigation control
  - `navigationcontext.cpp/.h` - Navigation context

- **board/**: 盤面編集・操作
  - `boardinteractioncontroller.cpp/.h` - Board interaction
  - `positioneditcontroller.cpp/.h` - Position editing
  - `boardimageexporter.cpp/.h` - Board image export

- **ui/**: UI層
  - **presenters/**: MVP Presenter layer
    - `evalgraphpresenter`, `recordpresenter`, `navigationpresenter`, etc.
  - **controllers/**: UI event handlers
    - `boardsetupcontroller`, `recordnavigationcontroller`, etc.
  - **wiring/**: Signal/Slot connections
    - `uiactionswiring`, `csagamewiring`, `analysistabwiring`, etc.
  - **coordinators/**: UI coordination
    - `dialogcoordinator`, `positioneditcoordinator`, etc.

- **views/**: Qt Graphics View for board rendering (`shogiview.cpp/.h`)
- **widgets/**: Custom Qt widgets
- **dialogs/**: Dialog implementations (with co-located .ui files)
- **models/**: Qt models for lists and data binding
- **services/**: Cross-cutting services (settings, time keeping)
- **common/**: Shared utilities (`errorbus`, `jishogicalculator`)

### UI Files

Qt Designer `.ui` files are co-located with their corresponding source files:
- `src/app/mainwindow.ui` - Main window
- `src/dialogs/*.ui` - Dialog files

Auto-processed by CMake's AUTOUIC.

### Resources (`resources/`)

- `shogiboardq.qrc` - Qt resource file
- `icons/` - Application icons (.icns, .ico, .png)
- `images/`
  - `actions/` - Menu/Toolbar action icons
  - `pieces/` - Shogi piece images
- `platform/` - Platform-specific files (app.rc for Windows)
- `translations/` - Translation files (Japanese `_ja_JP.ts`, English `_en.ts`)

## Key Technologies

- **Qt 6** (Qt6専用、Qt5非対応): Widgets, Charts, Network, LinguistTools
- **C++17** required
- **CMake 3.16+** build system
- **USI Protocol** for engine communication
- **CSA Protocol** for network play

## Internationalization

The app supports Japanese and English. Translations are in `.ts` files under `resources/translations/`. After modifying translatable strings, run `lupdate` to update `.ts` files, then rebuild to generate `.qm` files.

### Translation Update Guidelines

**ソースコードを修正した際は、必ず翻訳ファイルも更新すること。**

1. `tr()` でラップされた文字列を追加・変更した場合:
   ```bash
   cmake --build build --target update_translations
   ```
2. 翻訳ファイル（`.ts`）を開き、`type="unfinished"` となっている新規エントリに翻訳を追加する
3. 日本語が原文の場合は英語翻訳を、英語が原文の場合は必要に応じて日本語翻訳を追加する

### Translation Baseline (CI)

CI の `translation-check` ジョブは、未翻訳数がベースラインから増加すると fail する。
ベースラインは `resources/translations/baseline-unfinished.txt` に記録されている。

翻訳を追加したらベースラインを更新すること:
```bash
# ベースライン更新
grep -c 'type="unfinished"' resources/translations/*.ts | \
  sed 's|resources/translations/||' > resources/translations/baseline-unfinished.txt
```

### SettingsService Update Guidelines

**ダイアログに関するソースコードを修正した場合は、SettingsServiceも更新すること。**

ダイアログやウィンドウを追加・修正する際は、以下の設定をSettingsServiceに登録して永続化を検討する:

1. **ウィンドウサイズ**: ユーザーがリサイズ可能なダイアログは、サイズを保存・復元する
2. **フォントサイズ**: フォントサイズ調整機能（A-/A+ボタン）がある場合は、サイズを保存
3. **列幅**: テーブルやリストビューの列幅を保存
4. **最後に選択した項目**: コンボボックスの選択状態など

**追加手順:**
1. `src/services/settingsservice.h` に getter/setter 関数を宣言
2. `src/services/settingsservice.cpp` に実装を追加（既存の実装パターンに従う）
3. ダイアログのコンストラクタで設定を読み込み、デストラクタまたは `closeEvent` で保存

**実装例:**
```cpp
// settingsservice.h
QSize myDialogSize();
void setMyDialogSize(const QSize& size);

// settingsservice.cpp
QSize myDialogSize()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("MyDialog/size", QSize(800, 600)).toSize();
}

void setMyDialogSize(const QSize& size)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("MyDialog/size", size);
}
```

## Development Environment

- **Qt Creator** is used for development

## Design Principles

- **MainWindow should stay lean**: Delegate logic to existing or new classes (coordinators, controllers, services). MainWindow should only handle MainWindow-specific responsibilities.
- When adding new features, prefer creating new classes in appropriate directories (`app/`, `controllers/`, `services/`, etc.) rather than adding code to MainWindow.
- **Deps/Hooks/Refs パターン**: `docs/dev/deps-hooks-refs-pattern.md` に従う。依存注入に3種の構造体パターン（Deps=外部依存注入、Hooks=上位層へのコールバック、Refs=親状態への参照）を使い分ける
- **スレッド安全性**: `docs/dev/threading-guidelines.md` に従う。UIアクセスはメインスレッド限定、ワーカーには値オブジェクトのみ渡す

## Code Style

- **所有権ルール**: `docs/dev/ownership-guidelines.md` に従う。QObject は parent ownership、非QObject は `std::unique_ptr` を基本とする。裸の `new/delete` は原則禁止
- Compiler warnings enabled: `-Wall -Wextra -Wpedantic -Wshadow -Wconversion` and more
- clang-tidy configured for unused code detection (see `.clang-tidy`)
- **Do not use lambda expressions in `connect()` statements**. Use the member function pointer syntax:
  ```cpp
  // Good
  connect(sender, &Sender::signal, receiver, &Receiver::slot);

  // Avoid
  connect(sender, &Sender::signal, [this]() { ... });
  ```
- **Avoid Qt container detachment in range-loops**: Use `std::as_const()` or `qAsConst()` to prevent detachment warnings:
  ```cpp
  // Good
  for (const auto &item : std::as_const(list)) { ... }

  // Warning: clazy-range-loop-detach
  for (const auto &item : list) { ... }
  ```
- **メンバ変数の初期化**: C++ Core Guidelines (C.45, C.48) に従い、以下のように使い分ける:
  - **ヘッダーでのインクラス初期化（NSDMI）**: デフォルト値が固定の場合に使用
  - **コンストラクタ初期化子リスト**: コンストラクタ引数に依存する値、基底クラス、`this`ポインタが必要な場合に使用
  ```cpp
  // ヘッダー (.h) - 固定のデフォルト値
  int m_count = 0;
  bool m_isReady = false;

  // コンストラクタ (.cpp) - 引数依存・基底クラス・this が必要な値
  MyClass::MyClass(Model* model, QObject* parent)
      : QObject(parent)
      , m_handler(std::make_unique<Handler>(this))
      , m_model(model)
  ```
- **Debug logging**: `docs/dev/debug-logging-guidelines.md` に従う。`qDebug()` はデバッグビルド専用。リリースビルドでは `QT_NO_DEBUG_OUTPUT` により無効化される
- **clazy 警告を出さないコーディング**: clazy level0,level1 の警告が出ないようにコードを書くこと。特に以下に注意:
  - **`clazy-fully-qualified-moc-types`**: `signals:` / `public slots:` の引数型はMOCが正規化できるよう完全修飾する。ネスト型は `ClassName::NestedType` と記述する。Qt6では `QVector` は `QList` に正規化されるため `QList` を使う
    ```cpp
    // Good - ネスト型を完全修飾
    void gameEnded(const MatchCoordinator::GameEndInfo& info);
    void gameStateChanged(CsaGameCoordinator::GameState state);

    // Bad - MOCが正規化できない
    void gameEnded(const GameEndInfo& info);
    void gameStateChanged(GameState state);
    ```
  - **`clazy-const-signal-or-slot`**: `const` メソッドやgetter（引数なし＋戻り値あり）を `signals:` / `public slots:` に置かない。`connect()` で使わないメソッドは `public:` セクションに配置する
  - **`clazy-range-loop-detach`**: 上記の `std::as_const()` を使用する
