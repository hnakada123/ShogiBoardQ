# Phase 8: 仕上げ — 不要コード削除とドキュメント同期

## 背景

`docs/dev/mainwindow-responsibility-delegation-plan.md` のフェーズ8（最終フェーズ）。
全フェーズ完了後の仕上げ作業。

## タスク

1. **不要メンバ・不要 include 削除**
   - `mainwindow.h` / `mainwindow.cpp` を読み、以下を削除:
     - 使われなくなったメンバ変数
     - 使われなくなった private メソッド宣言
     - 不要になった `#include`
   - clang-tidy / cppcheck で未使用コードを検出:
     ```bash
     cmake -B build -S . -DENABLE_CLANG_TIDY=ON -G Ninja && ninja -C build
     ```

2. **翻訳ファイル更新**
   - `tr()` 文字列に変更がある場合:
     ```bash
     lupdate src -ts resources/translations/ShogiBoardQ_ja_JP.ts resources/translations/ShogiBoardQ_en.ts
     ```

3. **最終計測**
   - `mainwindow.cpp` の行数（目標: <= 2200行）
   - `MainWindow::` メソッド数（目標: <= 120）
   - 追加/拡張したロジッククラス数（目標: >= 6）
   - 結果を `docs/dev/mainwindow-responsibility-delegation-plan.md` の末尾に追記

4. **ドキュメント同期**
   - CLAUDE.md の Architecture セクションに新規クラスの情報を追加
   - MEMORY.md のリファクタリング状況を更新

5. **最終ビルド確認**
   ```bash
   cmake -B build -S . -G Ninja && ninja -C build
   ```

## 完了条件（Definition of Done）

機能面:
1. 既存の主要操作が回帰しない
2. 既存の保存形式・表示仕様を維持

構造面:
1. `MainWindow` は「受信 + 委譲」中心
2. 長大メソッド（40行超）を大幅削減
3. `PlayMode` 判定 `switch` を `MainWindow` から除去

定量目標:
1. `mainwindow.cpp` <= 2200 行
2. `MainWindow::` メソッド数 <= 120
3. 主要ロジッククラスを最低6個追加/拡張

## 注意

- コードの変更は最小限（削除とコメント整備のみ）
- 機能追加は行わない
