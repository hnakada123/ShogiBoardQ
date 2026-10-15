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

# Command line tool used by the MCP server (no GUI)
./build/shogiboardq-cli version

# Python MCP server tests (also registered in ctest as tst_mcp_python)
python3 -m pytest -q mcp/tests

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
  - `mainwindowcomposer.cpp/.h` - 合成ルート。起動時に全部品を依存順に作り、依存を注入し、シグナルをつなぐ（実装は `mainwindowcomposer_*.cpp` に段階ごとに分割）。設計: `docs/dev/composition-root.md`
  - `mainwindowcomponents.h` - 合成ルートが作る部品の一覧。状態は `src/common/appstate.h` の `AppState` に1つだけ置く（部品には `AppState*` を渡し、個々のフィールドへのポインタは渡さない）
  - `mainwindowactions.cpp/.h` - メニュー・盤・棋譜欄からの利用者操作の振り分け先
  - `gamesession.cpp/.h` - 対局の開始・終局・再開と、対局の手順の棋譜への追記（`MatchRecordSink`）
  - `humanturnpolicy.cpp/.h` - 盤を人間が操作できる手番か・対局が進行中かの判定
  - `turnstatesyncservice.cpp/.h` - 手番同期（TurnManager/GC/View/時計）
  - `boardloadservice.cpp/.h` - SFEN盤面適用・ハイライト同期
  - `kifunavigationcoordinator.cpp/.h` - 棋譜ナビゲーション同期
  - `uiactionswiring` と `file/game/edit/viewactionswiring` - メニュー項目と処理の接続

- **core/**: 純粋なゲームロジック（Qt GUI非依存）
  - `shogiboard.cpp/.h` - Board state and piece management（指し手の適用は `applyUsiMove` だけ）
  - `shogimove.cpp/.h`・`usimove.h` - Move representation
  - `enginemovevalidator.cpp/.h` - Move legality validation（`fmv::Converter` を境界に `fmvlegalcore` のビットボード合法手生成を使う）
  - `sfenreplay.cpp/.h` - 指し手列から局面列を作る
  - `matchplayers.h` - 対局者（先手・後手を人間とエンジンのどちらが指すかと名前）
  - `shogiclock.cpp/.h` - Game clock
  - `shoginotation.h` - 筋・段・駒の表記（漢字・全角・CSA）

- **game/**: ゲーム進行・対局管理（GUI に依存しない）
  - `matchcoordinator.cpp/.h` - 対局の司令塔（開始・着手・終局・待った・中断と再開）。手順は `record()`、対局者は `players()`
  - `humanvshumanstrategy` / `humanvsenginestrategy` / `enginevsenginestrategy` - 対局者の組ごとの進め方（`MatchStrategyHost` 経由）
  - `enginelifecyclemanager.cpp/.h` - エンジンの枠 E1・E2（対局と検討で共有）
  - `shogigamecontroller.cpp/.h` - Game flow control
  - `gamestartcoordinator.cpp/.h`・`gamestartoptionsbuilder` - 対局開始ダイアログの値の解釈と開始
  - `turnmanager.cpp/.h` - Turn management

- **kifu/**: 棋譜関連（GUI に依存しない）
  - `gamerecordmodel.cpp/.h` - Kifu data model
  - `recordline.h`・`recordentry.h` - 1本の手順と1手の記録
  - `kifuioservice.cpp/.h`・`kifusavefile.cpp/.h` - Kifu file I/O
  - **formats/**: Format converters
    - `kiftosfenconverter`, `ki2tosfenconverter`, `csatosfenconverter`, etc.

- **analysis/**: 解析機能（GUI に依存しない）
  - `analysissession.cpp/.h` - 検討・詰み探索のセッション
  - `analysiscoordinator.cpp/.h`・`analysisjob` - 棋譜解析
  - `sfenvalidationservice`・`tsumeshogigenerator` など

- **engine/**: USI (Universal Shogi Interface) エンジン連携（対局の仕組みに依存しない）
  - `usi.cpp/.h` - GUI から使うエンジン1台の窓口
  - `usienginesession.cpp/.h` - 起動と初期化（usi〜readyok）。GUI と CLI の実行器が共有する
  - `usiprotocolhandler.cpp/.h` - USI protocol implementation
  - `engineprocessmanager.cpp/.h` - Engine process lifecycle
  - `usimatchhandler.cpp/.h` - 対局・解析での USI のやり取り（position/go/bestmove）

- **network/**: CSA通信対局
  - `csaclient.cpp/.h` - CSA protocol client（受信の解釈は `csaclient_parser.cpp`、送信は `csaclient_commands.cpp`）
  - `csagamecoordinator.cpp/.h` - CSA game coordination（手順と持ち時間は `CsaGameProgress`）

- **navigation/**: ナビゲーション機能
  - `kifunavigationcontroller.cpp/.h` - 棋譜ナビゲーション（ボタン・分岐）

- **board/**: 盤面編集・操作
  - `boardinteractioncontroller.cpp/.h` - Board interaction
  - `positioneditcontroller.cpp/.h` - Position editing
  - `boardimageexporter.cpp/.h` - Board image export

- **ui/**: UI層
  - **presenters/**: 表示への反映
    - `playerinfopresenter`（対局者名・対局情報）, `gamerecordpresenter`, `boardsyncpresenter`, etc.
  - **controllers/**: UI event handlers
    - `boardsetupcontroller`, `considerationuicontroller`, `josekiwindowcontroller`, `menuwindowcontroller`, `replaycontroller`, etc.
  - **wiring/**: 部品どうしのシグナル接続
    - `csagamewiring`, `analysistabwiring`, `branchnavigationwiring`, etc.
  - **coordinators/**: 複数の部品にまたがる UI の流れ
    - `dialogcoordinator`, `dialoglauncher`, `kifuloadcoordinator`, `kifusavecoordinator`, `analysisflowcontroller`, `recordnavigationhandler`, `positioneditcoordinator`, etc.
  - `uistatepolicymanager` - 動作状態ごとのメニュー・ボタンの有効/無効（コンパイル時の表）

- **views/**: Qt Graphics View for board rendering (`shogiview.cpp/.h`)
- **automation/**: 自動化 API（`ShogiBoardQ --automation`）と、CLI・MCP サーバーが共有する GUI 非依存サービス（設計: `docs/dev/mcp-server.md`）
  - `automationserver.cpp/.h` - QLocalServer で改行区切り JSON-RPC 2.0 を待ち受け
  - `automationdispatcher.cpp/.h` - JSON-RPC 解析とメソッド表（GUI 非依存）
  - `automationcommands_*.cpp` - app / position / kifu / ui の各メソッド、`automationactionpolicy` は QAction 許可リスト
  - `kifuconversionservice`, `boardimagerenderer`, `engineanalysisrunner`, `matesearchrunner`, `tsumeverificationrunner` - CLI と共有するサービス（局面の検証は `analysis/sfenvalidationservice`）
- **cli/**: `shogiboardq-cli`（GUI を起動しない JSON 出力コマンド。`mcp/` の MCP サーバーが呼び出す）
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

### Settings Update Guidelines

**ダイアログに関するソースコードを修正した場合は、設定の宣言の表も更新すること。**

ダイアログやウィンドウを追加・修正する際は、以下の設定を永続化するか検討する:

1. **ウィンドウサイズ**: ユーザーがリサイズ可能なダイアログは、サイズを保存・復元する
2. **フォントサイズ**: フォントサイズ調整機能（A-/A+ボタン）がある場合は、サイズを保存
3. **列幅**: テーブルやリストビューの列幅を保存
4. **最後に選択した項目**: コンボボックスの選択状態など

設定はドメイン別ヘッダー（`src/services/appsettings.h`・`gamesettings.h`・`analysissettings.h`・`networksettings.h`・`josekisettings.h`・`tsumeshogisettings.h`・`enginedialogsettings.h`・`docksettings.h`）に、`Setting<T>`（`src/services/setting.h`）で**キーと既定値を1行で宣言する**。getter と setter の組やキー定数は手で書かない。

**追加手順:**
1. ドメイン別ヘッダーに `inline const Setting<T> name{"Group/key", 既定値};` を1行足す
2. 読むときは `NS::name()`、書くときは `NS::name.set(value)`、既定値は `NS::name.defaultValue()`。既定値が翻訳文や実行時の場所で決まる項目は、宣言の既定値を空にして `NS::name.valueOr(tr("..."))` で読む
3. ダイアログのコンストラクタで読み込み、デストラクタ・`done()`・`closeEvent` で保存する。ウィンドウサイズは `DialogUtils::restoreDialogSize` / `saveDialogSize`、A-/A+ は `FontSizeHelper` に宣言をそのまま渡す
4. 値の範囲を絞る、複数のキーを構造体にまとめる、配列で保存する、といった処理が要る項目だけを関数として `.cpp` に書く。その場合も `SettingsCommon::openSettings()` を使う（設定ファイルを `QSettings` で直接開くのは、`beginReadArray` などでグループの状態を変える配列データだけ）

**実装例:**
```cpp
// gamesettings.h
/// 〇〇ダイアログのウィンドウサイズ（デフォルト: 800x600）
inline const Setting<QSize> myDialogSize{"SizeRelated/myDialogSize", QSize(800, 600)};
/// 〇〇ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> myDialogFontSize{"FontSize/myDialog", 10};

// mydialog.cpp
MyDialog::MyDialog(QWidget* parent)
    : QDialog(parent)
    , m_fontHelper(GameSettings::myDialogFontSize, 8, 24, 1)  // 変更のたびに保存する
{
    DialogUtils::restoreDialogSize(this, GameSettings::myDialogSize);
}

MyDialog::~MyDialog()
{
    DialogUtils::saveDialogSize(this, GameSettings::myDialogSize);
}
```

## Development Environment

- **Qt Creator** is used for development

## Design Principles

- **MainWindow should stay lean**: Delegate logic to existing or new classes (coordinators, controllers, services). MainWindow should only handle MainWindow-specific responsibilities.
- When adding new features, prefer creating new classes in appropriate directories (`app/`, `controllers/`, `services/`, etc.) rather than adding code to MainWindow.
- **Deps/Hooks/Refs パターン**: `docs/dev/done/deps-hooks-refs-pattern.md` に従う。依存注入に3種の構造体パターン（Deps=外部依存注入、Hooks=上位層へのコールバック、Refs=親状態への参照）を使い分ける
- **スレッド安全性**: `docs/dev/threading-guidelines.md` に従う。UIアクセスはメインスレッド限定、ワーカーには値オブジェクトのみ渡す
- **層の依存**: 下の層は上の層を include しない。順に core・common → services → kifu → engine → game → analysis・network・navigation → models → views・widgets → board → dialogs → ui → app。`automation`・`cli` は GUI に依存しない（規則は `tests/tst_layer_dependencies.cpp` が検査する）
- **クラス名の役割**: `*Wiring` はシグナルの接続だけ、`*Presenter` は表示への反映、`*Controller` は利用者操作の処理、`*Coordinator` は複数の部品にまたがる流れ。役割が変わったら名前も変える
- **ファイルの分割**: 1ファイル1000行の上限（`tests/tst_structural_kpi.cpp`）に近づいたら、行数で `_ui`・`_draw` などに機械的に分けず、責務を別のクラスに切り出す

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
