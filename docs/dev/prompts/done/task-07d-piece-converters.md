# Task 7d: フォーマット変換器の Piece 対応

棋譜フォーマット変換器（KIF, KI2, CSA, USI, USEN, JKF）を `Piece` enum 対応に更新してください。

## 前提

Task 7a〜7c（Piece enum 定義、ShogiMove/ShogiBoard の Piece 化）が完了していること。

## 作業内容

### Step 1: 各変換器の確認

`src/kifu/formats/` 配下の全変換器を確認し、`QChar` で駒を扱っている箇所を特定:
- `kiftosfenconverter.cpp`
- `ki2tosfenconverter.cpp`
- `csatosfenconverter.cpp`
- `usitosfenconverter.cpp`
- `usentosfenconverter.cpp`
- `jkftosfenconverter.cpp`

### Step 2: 各変換器の更新

- 内部の駒文字マッピング（`QMap<QString, QChar>` 等）を `Piece` enum に更新
- 駒文字の比較処理を `Piece` enum 比較に置換
- SFEN 出力時は `pieceToChar()` を使用
- 外部フォーマット（KIF/KI2/CSA）の駒文字→ `Piece` 変換は各変換器固有のマッピングを維持

### Step 3: テストの確認

- 全既存フォーマット変換テストが通ること
- ラウンドトリップ（読み込み→ SFEN 出力→再読み込み）が正しく動くこと

## 制約

- CLAUDE.md のコードスタイルに準拠
- 各変換器のテストが全て通ること
- 翻訳ファイルの更新は不要（UI 表示に影響しない内部変更のため）
