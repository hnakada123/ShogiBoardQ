# Task 21: MatchCoordinator 削減実装

## 目的
Task 20 の分析結果に基づき、`MatchCoordinator` を段階的に削減する。

## 背景
Task 20 で策定した計画を実装する。
`matchcoordinator.cpp`（1444行）を 800行以下に削減することを目標とする。

## 前提
- Task 20 の分析結果（`docs/dev/done/matchcoordinator-reduction-analysis.md`）が完了していること

## 作業内容

### Phase 1: 最大カテゴリの抽出
Task 20 で特定された最大の抽出候補を分離する:
1. 新クラスを `src/game/` ディレクトリに作成する
2. 既存の Refs/Hooks パターンに合わせて設計する
3. `MatchCoordinator` からの委譲に書き換える
4. ビルド確認・動作確認

### Phase 2: 次の抽出候補の分離
1. 次に大きい抽出候補を分離する
2. ビルド確認・動作確認

### Phase 3: ヘッダー整理
1. `matchcoordinator.h` の不要になったメソッド宣言・メンバ変数を削除する
2. forward declaration の整理
3. シグナル・スロットの整理（不要なものの削除、転送の簡素化）

### Phase 4: CMakeLists.txt と翻訳の更新
1. 新規ファイルを `CMakeLists.txt` に追加する
2. 翻訳ファイルを更新する

## 完了条件
- [ ] `matchcoordinator.cpp` が 800行以下になっている
- [ ] 抽出した各クラスが 600行以下である
- [ ] ビルドが通る（警告なし）
- [ ] 対局の開始・進行・終了が正常に動作する（人対人、人対エンジン、エンジン対エンジン）
- [ ] CSA通信対局が正常に動作する
- [ ] 翻訳ファイルが更新されている

## 注意事項
- Task 20 の分析で決定した分割境界に従う
- `MatchCoordinator` のシグナルは多数の外部クラスが接続しているため、シグナル削除は慎重に行う
- `GameEndHandler` / `GameStartOrchestrator` で確立された Refs/Hooks パターンを踏襲する
- CLAUDE.md のコードスタイルに従う
