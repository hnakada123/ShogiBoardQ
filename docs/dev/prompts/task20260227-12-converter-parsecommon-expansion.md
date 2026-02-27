# Task 12: parsecommon 拡充と共通処理集約

## 目的
Task 11 の分析結果に基づき、4つの棋譜変換器で重複している共通処理を `parsecommon` に集約する。

## 前提
- Task 11 の分析ドキュメントが完成していること

## 作業内容

### 1. parsecommon.h/.cpp の拡充
以下の共通処理を `parsecommon` に追加する（Task 11 の分析結果に基づく）:

想定される共通処理:
- 終局語の解析（`parseGameResult`）
- コメント行の解析（`parseComment`）
- 時間情報の解析（`parseTimeInfo`）
- 先手/後手の判定（`parseTurnIndicator`）
- 指し手の盤面適用（`applyMoveToBoard`）
- エラー生成ヘルパー（`createParseError`）

### 2. 各変換器の重複コード置換
各変換器で `parsecommon` の共通関数を呼び出すように書き換える:
- `kiftosfenconverter.cpp`
- `ki2tosfenconverter.cpp`
- `csatosfenconverter.cpp`
- `jkftosfenconverter.cpp`

### 3. 共通テストの追加
`parsecommon` の新規関数に対するユニットテストを追加する:
- `tests/tst_parsecommon.cpp` を新規作成（または既存テストに追加）

### テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] 重複コードが 50%以上削減されている
- [ ] 各変換器の既存テストが全て PASS する
- [ ] 新規 parsecommon テストが PASS する
- [ ] 各変換器のファイル行数が減少している

## 注意事項
- 各変換器のテストを1つずつ実行しながら進める
- 共通化の際、フォーマット固有の微妙な違い（例: KIF と CSA で終局語の表記が異なる）に注意する
- 引数や戻り値の型は既存の `parsecommon` のスタイルに合わせる
- 共通関数は名前空間 `ParseCommon` 内に配置する（既存パターンに従う）
