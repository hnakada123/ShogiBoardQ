# Task 21: コード品質改善 改修依頼書（AI向け）

## 目的
ShogiBoardQ の保守性と設計一貫性を改善する。  
機能追加ではなく、既存挙動を維持したまま品質を上げることが目的。

## 背景（現状）
- `MainWindow` の肥大化が継続している。
  - `src/app/mainwindow.cpp`: 約 4,569 行
  - `src/app/mainwindow.h`: 約 825 行
- コア司令塔も大きい。
  - `src/game/matchcoordinator.cpp`: 約 2,598 行
  - `src/game/matchcoordinator.h`: 約 611 行
- 設定サービスが巨大化している。
  - `src/services/settingsservice.cpp`: 約 1,608 行
- クリーンビルド時、テストコードで警告あり（`nodiscard` 戻り値未使用）。
  - `tests/tst_josekiwindow.cpp:175`
  - `tests/tst_josekiwindow.cpp:198`
  - `tests/tst_josekiwindow.cpp:209`
  - `tests/tst_josekiwindow.cpp:322`
- `ctest` は GUI 環境依存で abort しやすく、`QT_QPA_PLATFORM=offscreen` では通る。

## 改修スコープ（優先順）

### P0: ビルド警告ゼロ化（必須）
対象:
- `tests/tst_josekiwindow.cpp`

作業:
1. `QTest::qWaitForWindowExposed(...)` の戻り値を検証する。
2. 同様の呼び出しが他テストにあれば横展開で修正する。

修正方針例:
- `QVERIFY(QTest::qWaitForWindowExposed(&window));`
- もしくは小さなヘルパー関数を作って `QVERIFY2` で失敗理由を明示する。

受け入れ条件:
- `cmake --build build --clean-first -j4` で警告が増えない。
- 少なくとも上記 `nodiscard` 警告が消えている。

---

### P1: 所有権の明確化（必須）
対象候補:
- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- 関連する `ensureXxx()` 実装

作業:
1. 所有権が曖昧なメンバを整理する。
   - 例: `QList<KifuDisplay *>* m_moveRecords` は値メンバ化または `std::unique_ptr` 化。
   - 例: `GameSessionFacade* m_gameSessionFacade` は `std::unique_ptr` 化を検討。
2. `QObject` 親を持つオブジェクトは「非所有」コメントを見直し、実態に合わせる。
3. コメント上の所有権表現と実コードを一致させる。

注意:
- 挙動変更は避ける（寿命管理の明確化のみ）。
- 既存シグナル/スロット接続順序を壊さない。

受け入れ条件:
- メモリリーク/二重解放のリスクを下げる修正になっている。
- ビルドと既存テストが通る。

---

### P2: MainWindow 責務分割（必須）
対象:
- `src/app/mainwindow.cpp`
- `src/app/mainwindow.h`
- 必要に応じて `src/app/` または `src/ui/coordinators/` に新規クラス追加

作業:
1. `MainWindow` から少なくとも 1 つの責務群を外部クラスへ抽出する。  
   優先候補:
   - 初期化シーケンス（`setup*/build*/connect*` 系）
   - リセット処理（`resetEngineState/resetGameState/resetModels/resetUiState`）
   - ダイアログ起動処理群（`display*`）
2. `MainWindow` の public/private slot の数とメンバ依存を減らす。
3. 挙動維持を最優先し、段階的に移行する。

受け入れ条件:
- `MainWindow` の行数が減る（目安: 300 行以上削減 or 1責務群の完全外出し）。
- 呼び出し元の可読性が向上し、初期化順序が崩れない。

---

### P3: `connect` 方針の整合（推奨）
対象例:
- `src/ui/coordinators/docklayoutmanager.cpp`
- `src/engine/usi.cpp`

作業:
1. 方針「`connect` はメンバ関数ポインタ構文、ラムダを避ける」に合わせる。
2. ラムダが不可避な箇所は、理由を短いコメントで明示する。
3. 可能なら `QAction::data` + 単一スロットで置換する。

受け入れ条件:
- ラムダ `connect` の件数が減る。
- 可読性・デバッグ容易性が下がらない。

## テスト/検証手順（必須）
1. `cmake --build build --clean-first -j4`
2. `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4`

任意（推奨）:
- `cmake -B build -S . -DENABLE_CLANG_TIDY=ON`
- `cmake -B build -S . -DENABLE_CPPCHECK=ON`

## 実装ルール
- 既存の機能仕様は変えない。
- リファクタは小さめのコミット単位で進める。
- 命名規約（ファイル: snake_case、クラス: CamelCase）を守る。
- `MainWindow` にロジックを戻さない。

## 完了定義（Definition of Done）
- [ ] P0 修正が完了し、該当警告が解消されている。
- [ ] P1 で所有権がコードとコメントで一致している。
- [ ] P2 で MainWindow の責務分割が 1 つ以上完了している。
- [ ] ビルド成功。
- [ ] `QT_QPA_PLATFORM=offscreen` でテスト成功。
- [ ] 変更内容が短く要約され、影響範囲が説明されている。

## AIへの依頼テンプレート（そのまま使用可）
以下を満たして改修してください:
1. まず P0（警告解消）を完了し、ビルドログで確認する。
2. 次に P1（所有権整理）を実施し、コメントと実装の不一致を解消する。
3. 続いて P2（MainWindow責務分割）を 1 ブロック以上実施する。
4. 可能なら P3（ラムダconnect整理）を進める。
5. 最後に `cmake --build build --clean-first -j4` と
   `QT_QPA_PLATFORM=offscreen ctest --test-dir build --output-on-failure -j4`
   を実行し、結果を報告する。
