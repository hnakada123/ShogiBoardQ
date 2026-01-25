# Repository Guidelines

## Project Structure & Module Organization
- `src/` contains feature modules grouped by responsibility (e.g., `core/`, `game/`, `kifu/`, `analysis/`, `engine/`, `network/`, `ui/`, `dialogs/`).
- Qt Designer UI files live alongside their related code in `src/app/` and `src/dialogs/` (e.g., `src/app/mainwindow.ui`).
- `resources/` holds assets, icons, translations, and `shogiboardq.qrc`.
- `build/` is the default out-of-tree build output.
- Key protocols: USI (engine integration) and CSA (network play).

## Build, Test, and Development Commands
- `cmake -B build -S .` configures the project (CMake 3.16+, C++17).
- `cmake --build build` builds the Qt6 application.
- `./build/ShogiBoardQ` runs the app after a successful build.
- `cmake -B build -S . -DENABLE_CLANG_TIDY=ON` enables clang-tidy checks.
- `cmake -B build -S . -DENABLE_CPPCHECK=ON` enables cppcheck for unused code.
- `lupdate src -ts resources/translations/ShogiBoardQ_ja_JP.ts resources/translations/ShogiBoardQ_en.ts` updates translation sources when `tr()` strings change.

## Coding Style & Naming Conventions
- C++17; headers (`.h`) live next to their `.cpp` counterparts.
- Prefer small, focused classes; keep `MainWindow` lean and push behavior into coordinators/controllers/services.
- Use Qt signal/slot member function pointer syntax; avoid lambda `connect()`.
- Follow existing naming patterns: lower_snake for files, CamelCase for classes.
- Compiler warnings are strict (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`); keep code warning-clean.

## Testing Guidelines
- No automated test suite is currently present. If you add tests, document the framework and invocation in this file and keep tests near related modules (e.g., `src/core/` or a new `tests/` directory).

## Commit & Pull Request Guidelines
- Commit messages are short, imperative, and typically written in Japanese (e.g., "バグ修正", "UI改善").
- PRs should include: a brief summary, the user-visible impact, and screenshots for UI changes. Link related issues if available.

## Internationalization & Settings
- When you add or change UI text wrapped in `tr()`, update `.ts` files via `lupdate` and provide translations.
- When adding/modifying dialogs, register size/state persistence in `SettingsService` so user preferences survive restarts.
- Typical settings to persist: dialog window sizes, font size controls, column widths, and last-selected combo items.

## Development Environment
- Qt Creator is the primary IDE used for this project; ensure Qt6 Widgets/Charts/Network are available.

## Agent-Specific Instructions
- Always respond in Japanese.
- Run a build after changes, and fix any build warnings automatically when feasible.
