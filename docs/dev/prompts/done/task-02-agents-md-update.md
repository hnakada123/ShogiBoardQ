# Task 02: AGENTS.md のテストセクション更新

## フェーズ

Phase 0（即時）- P1-5 対応

## 背景

`AGENTS.md` には「自動テストなし」と記述されているが、実際には `tests/` に 31 テストが存在し `tests/CMakeLists.txt` で `add_test` 登録されている。新規開発者が古い情報に引っ張られるリスクがある。

## 実施内容

1. `AGENTS.md` を読み、現在の Testing セクションの内容を確認する
2. Testing セクションを以下の情報で更新する：
   - テストが `tests/` ディレクトリに存在すること
   - テスト数（31件）
   - ビルド方法: `cmake -B build -S . -DBUILD_TESTING=ON` → `cmake --build build`
   - 実行方法: `ctest --test-dir build --output-on-failure`
   - `QT_QPA_PLATFORM=offscreen` が CMakeLists.txt で自動設定される旨（Task 01 完了前提）
   - headless 環境でのテスト実行が標準であること
3. その他、AGENTS.md 内で明らかに古い記述があれば修正する（ただし Testing 以外は最小限に）

## 完了条件

- AGENTS.md の Testing セクションが現状を正確に反映している
- テスト実行手順が明記されている

## 前提

- Task 01（CTest headless 適用）が完了していること
