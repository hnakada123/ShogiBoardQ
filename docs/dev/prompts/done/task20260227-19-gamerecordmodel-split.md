# Task 19: GameRecordModel 分割実装

## 目的
Task 18 の分析結果に基づき、`GameRecordModel` を段階的に分割する。

## 背景
Task 18 で策定した分割計画を実装する。
`gamerecordmodel.cpp`（1729行）を複数の責務別クラスに分割し、各ファイル 600行以下を目標とする。

## 前提
- Task 18 の分析結果（`docs/dev/done/gamerecordmodel-split-analysis.md`）が完了していること

## 作業内容

### Phase 1: MoveTreeManager の抽出
1. 分岐ツリー管理のメソッド群を `src/kifu/movetreemanager.h/.cpp` に抽出する
2. `GameRecordModel` から `MoveTreeManager` への委譲に書き換える
3. ビルド確認・動作確認

### Phase 2: GameAnnotationStore の抽出
1. コメント・評価値・時間情報を `src/kifu/gameannotationstore.h/.cpp` に抽出する
2. `GameRecordModel` から `GameAnnotationStore` への委譲に書き換える
3. ビルド確認・動作確認

### Phase 3: API互換性の確認と整理
1. `GameRecordModel` のファサードAPIが既存利用者と互換であることを確認する
2. 不要になった中間メソッドを削除する
3. ヘッダーの整理（forward declaration の更新）

### Phase 4: CMakeLists.txt と翻訳の更新
1. 新規ファイルを `CMakeLists.txt` に追加する
2. 翻訳ファイルを更新する（`tr()` 呼び出しが移動した場合）

## 完了条件
- [ ] `gamerecordmodel.cpp` が 600行以下になっている
- [ ] 抽出した各クラスが 600行以下である
- [ ] ビルドが通る（警告なし）
- [ ] 棋譜の読み込み・表示・分岐操作・コメント編集が正常に動作する
- [ ] 翻訳ファイルが更新されている

## 注意事項
- Task 18 の分析で決定したクラス名・分割境界に従う（分析結果と異なる場合は理由を記載）
- `GameRecordModel` は多数のクラスから参照されるため、既存のシグナル・スロットの互換性を最優先する
- CLAUDE.md のコードスタイル（connect でラムダ不使用、std::as_const 等）に従う
