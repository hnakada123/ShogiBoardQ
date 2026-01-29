# GEMINI In-Context Command Guide

This document provides a high-level overview of the ShogiBoardQ project, its structure, and development conventions to be used as context for an AI assistant.

## Project Overview

ShogiBoardQ is a desktop application for playing, analyzing, and managing Shogi (Japanese chess) games. It is written in C++17 and utilizes the Qt 6 framework for its graphical user interface.

The application provides a feature-rich environment for Shogi enthusiasts, including:
- A graphical shogi board for gameplay and analysis.
- The ability to play against other humans or AI engines.
- Support for the Universal Shogi Interface (USI) protocol to integrate with various Shogi engines.
- Kifu (game record) management, including reading, saving, and parsing various formats.
- Game analysis features, including engine-based evaluation and variation exploration.
- A flexible, dock-based UI for customizing the layout of different information panels (e.g., engine analysis, game records, evaluation graphs).

## Architecture

The project is built using CMake and follows a modular architecture with source code organized under the `src/` directory. Key modules include:

- `src/app`: The main application entry point (`main.cpp`) and the main window (`MainWindow`). `MainWindow` acts as the central coordinator for most of the application's functionality, though it is undergoing refactoring to delegate responsibilities to more specialized controllers.
- `src/core`: Contains the fundamental game logic for Shogi, including the `ShogiBoard` class (representing the board state), move validation, and SFEN parsing.
- `src/game`: Manages the game flow, turns, and coordination between players (human or engine).
- `src/engine`: Handles communication with external Shogi engines using the USI protocol.
- `src/kifu`: Responsible for reading, writing, and managing Shogi game records (kifu).
- `src/analysis`: Contains logic for game analysis, including engine-based consideration and Tsume (checkmate) puzzles.
- `src/ui/`, `src/views/`, `src/widgets/`, `src/dialogs/`: Contain various Qt UI components, views, and dialogs that make up the user interface.
- Numerous controller and coordinator classes that manage specific application features (e.g., `GameStartCoordinator`, `AnalysisCoordinator`, `KifuLoadCoordinator`).

## Building and Running

The project uses CMake as its build system. To build the project from the command line, use the standard CMake workflow:

```bash
# 1. Create a build directory
mkdir build
cd build

# 2. Configure the project
cmake ..

# 3. Build the executable
# On Linux/macOS
make
# On Windows (with MSVC)
cmake --build .
```

After a successful build, the executable `ShogiBoardQ` will be located in the build directory.

## Development Conventions

- **Language:** C++17.
- **Framework:** Qt 6.
- **UI:** The UI is built with Qt Widgets. UI layout files (`.ui`) are located alongside their corresponding source files.
- **Code Style & Quality:** The project uses `clang-tidy` and `cppcheck` for static analysis, indicating a focus on code quality and modern C++ practices. Compiler warnings are enabled to be strict.
- **Modularity:** Functionality is separated into distinct modules/directories (e.g., `core`, `engine`, `kifu`). New functionality should follow this pattern.
- **Coordination:** The application heavily uses coordinator and controller classes to manage complex workflows and interactions between different UI components and backend logic. This pattern should be maintained when adding new features.
- **Internationalization:** The application supports translations (English and Japanese are present) using Qt's translation tools (`.ts`, `.qm` files).
- **Logging:** A simple file-based logger is implemented in `main.cpp`, writing debug output to `debug.log`.
