# Task 11: 棋譜変換器の共通処理分析

## 目的
4つの棋譜変換器（KIF/KI2/CSA/JKF）の共通処理を分析し、パイプライン化と共通処理集約の計画を策定する。

## 背景
現在の変換器ファイルサイズ:
- `kiftosfenconverter.cpp`: 1278行
- `ki2tosfenconverter.cpp`: 1549行
- `csatosfenconverter.cpp`: 1004行
- `jkftosfenconverter.cpp`: 949行
- 合計: 4780行

既存の共通処理:
- `parsecommon.cpp/.h`: 共通パースユーティリティ
- `notationutils.cpp/.h`: 表記ユーティリティ

## 作業内容

### 1. 各変換器の構造分析
各変換器の処理フローを以下の段階に分解する:
1. **字句解析（Lexer）**: テキストからトークンへ
2. **構文解析（Parser）**: トークンから構造体へ
3. **指し手適用（MoveApplier）**: 構造体から盤面操作へ
4. **SFEN生成（Formatter）**: 盤面状態からSFEN文字列へ

### 2. 共通処理の洗い出し
4つの変換器で重複している処理を特定する:
- 終局語の解析（投了、中断、千日手、持将棋 等）
- コメント行の解析
- 時間情報の解析
- 先手/後手の判定
- 指し手の盤面適用
- 駒名の変換
- エラーハンドリング

### 3. 重複コード量の定量化
各共通処理について:
- 関数名/処理内容
- 各変換器での行数
- 統合可能性（完全一致 / 類似 / 異なる）

### 4. パイプライン設計
共通パイプラインの設計案を作成する:
```
Input(text) → Lexer(format-specific) → Parser(format-specific) → MoveApplier(common) → Formatter(common)
```
- 各段階のインターフェース定義
- フォーマット固有部分と共通部分の境界

### 5. 実装計画の策定
以下の順序で段階的に実施する計画を作成する:
1. 共通処理の `parsecommon` への集約
2. MoveApplier / Formatter の共通化
3. 各変換器のリファクタリング（KIF → KI2 → CSA → JKF）

## 出力
`docs/dev/done/converter-pipeline-analysis.md` に分析結果と計画を記載する。

## 完了条件
- [ ] 各変換器の処理フローが段階別に分解されている
- [ ] 重複コードが定量的に洗い出されている
- [ ] パイプライン設計案がある
- [ ] 段階的な実装計画がある

## 注意事項
- この段階ではコード変更は行わない（分析のみ）
- 各変換器にはフォーマット固有の複雑なエッジケースがある。完全な共通化は不可能な場合もある
- 既存テスト（`tst_kifconverter.cpp`, `tst_ki2converter.cpp`, `tst_csaconverter.cpp`, `tst_jkfconverter.cpp`）のカバレッジも確認する
