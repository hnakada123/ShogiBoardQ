# Task 2: USI プロトコルハンドラのテスト追加

`src/engine/usiprotocolhandler.h/.cpp` の USI プロトコル解析ロジックに対するユニットテストを作成してください。

## 対象ファイル

- テスト対象: `src/engine/usiprotocolhandler.h/.cpp`
- 新規作成: `tests/tst_usiprotocolhandler.cpp`
- テスト登録: `tests/CMakeLists.txt` に `add_shogi_test` マクロで追加

## テストすべき項目

### 1. info 行の解析

USI エンジンが出力する `info` 行のパースをテストする。以下のパターンを網羅すること:

- `info depth 20 seldepth 30 score cp 150 nodes 1000000 nps 500000 pv 7g7f 3c3d 2g2f`
- `info depth 15 score mate 5 pv 5e5d 4a3b 5d5c`（詰みスコア）
- `info depth 10 multipv 1 score cp 100 pv ...`（MultiPV）
- `info depth 10 multipv 2 score cp -50 pv ...`（MultiPV 2番手）
- `info string ...`（文字列情報、無視されること）

### 2. bestmove 行の解析

- `bestmove 7g7f`（通常の最善手）
- `bestmove 7g7f ponder 3c3d`（ponder 付き）
- `bestmove resign`（投了）
- `bestmove win`（勝ち宣言）

### 3. 不正入力への耐性

- 空行
- `bestmove` の後にトークンがない行
- 未知のコマンド行
- `info` に `score` がない行

## 設計方針

- `UsiProtocolHandler` の解析メソッドをテスト可能にするため、必要に応じてメソッドのアクセス修飾子の変更やテスト用フレンドクラスの導入を検討
- テスト用にエンジンプロセスの起動は不要。プロトコル解析ロジックのみをテストする
- `QSignalSpy` を使い、シグナル経由で解析結果を検証するアプローチを推奨
- テストに必要なスタブは `tests/test_stubs.cpp` のパターンに従う

## 制約

- `connect()` にラムダを使わない（CLAUDE.md 準拠）
- `std::as_const()` でレンジループの detach を防ぐ
- Qt Test フレームワーク（`QTest`）を使用
- 実装後、`cmake -B build -S . -DBUILD_TESTING=ON && cmake --build build && cd build && ctest --output-on-failure` でテストが通ることを確認
