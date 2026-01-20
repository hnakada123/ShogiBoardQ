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

# Update translations (.ui files are in src/)
lupdate src -ts resources/translations/ShogiBoardQ_ja_JP.ts resources/translations/ShogiBoardQ_en.ts
```

## Architecture

### Source Organization (`src/`)

ヘッダーファイル（.h）は対応するソースファイル（.cpp）と同じディレクトリに配置されています。

- **app/**: アプリケーションエントリーポイント
  - `main.cpp` - Entry point
  - `mainwindow.cpp/.h/.ui` - Main window

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

## Development Environment

- **Qt Creator** is used for development

## Design Principles

- **MainWindow should stay lean**: Delegate logic to existing or new classes (coordinators, controllers, services). MainWindow should only handle MainWindow-specific responsibilities.
- When adding new features, prefer creating new classes in appropriate directories (`app/`, `controllers/`, `services/`, etc.) rather than adding code to MainWindow.

## Code Style

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
