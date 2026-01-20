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

# Update translations
lupdate src include ui -ts resources/translations/ShogiBoardQ_ja_JP.ts resources/translations/ShogiBoardQ_en.ts
```

## Architecture

### Source Organization (`src/`)

- **app/**: Application-level coordinators and presenters that wire together UI and business logic
  - `main.cpp` - Entry point
  - Coordinators (e.g., `gamestartcoordinator`, `analysiscoordinator`) - orchestrate complex flows
  - Presenters (e.g., `evalgraphpresenter`, `recordpresenter`) - bridge models to views
  - Wiring classes (e.g., `csagamewiring`, `analysistabwiring`) - connect signals/slots

- **core/**: Core game logic and data structures
  - `shogiboard.cpp` - Board state and piece management
  - `shogigamecontroller.cpp` - Game flow control
  - `movevalidator.cpp` - Move legality validation
  - `gamerecordmodel.cpp` - Kifu data model
  - Format converters: `kiftosfenconverter`, `ki2tosfenconverter`, `csatosfenconverter`, `usitosfenconverter`, `jkftosfenconverter`

- **engine/**: USI (Universal Shogi Interface) engine integration
  - `usi.cpp` - Main engine interface
  - `usiprotocolhandler.cpp` - USI protocol implementation
  - `engineprocessmanager.cpp` - Engine process lifecycle
  - `shogienginethinkingmodel.cpp` - Analysis data model

- **controllers/**: UI state controllers
- **views/**: Qt Graphics View for board rendering (`shogiview.cpp`)
- **widgets/**: Custom Qt widgets
- **dialogs/**: Dialog implementations
- **services/**: Business logic services (kifu I/O, clipboard, time keeping)
- **network/**: CSA client for network play
- **models/**: Qt models for lists and data binding

### Headers (`include/`)

All header files are in a flat structure under `include/`. Use `#include "filename.h"` format.

### UI Files (`ui/`)

Qt Designer `.ui` files for dialogs and main window. Auto-processed by CMake's AUTOUIC.

### Resources (`resources/`)

- `shogiboardq.qrc` - Qt resource file
- `translations/` - Translation files (Japanese `_ja_JP.ts`, English `_en.ts`)
- Icons and images

## Key Technologies

- **Qt 6** (Qt 5 fallback supported): Widgets, Charts, Network, LinguistTools
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
