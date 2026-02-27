# Task 01: CTest の headless 実行を全テストに一括適用

## フェーズ

Phase 0（即時）- P0-1 対応

## 背景

`ctest --test-dir build` では 31 件中 26 件が `Subprocess aborted` になる。
`QT_QPA_PLATFORM=offscreen` を付与すると全 pass するが、現在この設定は 3 テストにしか適用されていない。

## 実施内容

1. `tests/CMakeLists.txt` を修正し、**全テスト**に `QT_QPA_PLATFORM=offscreen` 環境変数を設定する
   - 個別の `set_tests_properties` で設定するのではなく、一括で適用する方法を採用する（例: ループ、`CMAKE_CROSSCOMPILING_EMULATOR`、グローバル `set_tests_properties` など最もシンプルな方法）
   - 既存の個別 `set_tests_properties`（3件: `tst_ui_display_consistency`, `tst_analysisflow`, `tst_game_start_flow`）の重複を整理する

2. ビルド・テスト確認
   - `cmake -B build -S .` でコンフィグが通ること
   - `cmake --build build` でビルドが通ること
   - `ctest --test-dir build --output-on-failure` で全テスト pass すること

## 完了条件

- `ctest --test-dir build --output-on-failure` が headless 環境で全 31 件 pass
- 個別の `QT_QPA_PLATFORM` 設定の重複がない
- 既存テストの挙動が変わらない

## 注意事項

- テストの中身は変更しない（CMakeLists.txt のみ修正）
- 他のテスト環境変数（`ENVIRONMENT` プロパティ）が既に設定されているテストがあれば、上書きではなく追記する
