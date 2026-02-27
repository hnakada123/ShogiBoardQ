# Task 19: kifu converter 共通パース処理の再集約

## フェーズ

Phase 4（中長期）- P0-4 対応

## 背景

`src/kifu/formats/` の各 `*tosfenconverter.cpp` に共通パース処理の重複がある。

## 実施内容

1. `src/kifu/formats/` 以下の全コンバーターファイルを読み、共通処理を特定する：
   - `kiftosfenconverter.cpp`
   - `ki2tosfenconverter.cpp`
   - `csatosfenconverter.cpp`
   - その他の converter

2. 共通処理を抽出する候補を洗い出す：
   - 棋譜ヘッダーのパース
   - 指し手文字列の正規化
   - 座標変換
   - エラーハンドリングパターン

3. 共通処理を `parsecommon.h/.cpp`（または適切な名前）に集約する
4. 各コンバーターから共通処理を呼び出すよう修正する
5. `CMakeLists.txt` を更新する
6. ビルド確認: `cmake --build build`
7. テスト確認: 棋譜パース関連のテストが pass すること

## 完了条件

- 共通パース処理が一箇所に集約されている
- 各コンバーターの重複コードが削減されている
- ビルドが通る
- 既存テストが pass する

## 注意事項

- 各フォーマット固有のパースロジックは各コンバーターに残す
- 共通化しすぎて可読性が下がらないよう注意
