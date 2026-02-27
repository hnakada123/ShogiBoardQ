# Task 14: KI2 Converter パイプライン分割

## 目的
`ki2tosfenconverter.cpp`（1549行、プロジェクト最大の変換器）をパイプライン構造に分割し、600行以下にする。

## 前提
- Task 12 の parsecommon 拡充が完了していること
- Task 13 の KIF Converter 分割が完了していること（パターンを参考にする）

## 作業内容

### 1. KI2 Lexer の抽出
`src/kifu/formats/ki2lexer.h/.cpp` を新規作成する。

KI2 形式固有の字句解析を担当:
- 行の種類判定
- KI2 固有の表記パース（「同」「成」「打」「引」「寄」「上」等の修飾語）
- 曖昧指し手の解析

### 2. KI2 Parser の分離
構文解析部分を整理する:
- 曖昧指し手の解決ロジック（盤面参照が必要）
- ヘッダー情報の解析
- コメント・分岐の処理

### 3. 共通処理の活用
`parsecommon` の共通関数を最大限活用する。

### 4. CMakeLists.txt 更新

### テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] `ki2tosfenconverter.cpp` が 600行以下
- [ ] `ki2lexer.cpp` が 600行以下
- [ ] `tst_ki2converter.cpp` が全件 PASS する
- [ ] 全テストが PASS する

## 注意事項
- KI2 は曖昧指し手の解決が複雑（複数の駒が同じマスに移動可能な場合の記法）
- KIF と KI2 で共通の漢数字パース等があれば、さらに共通化を検討する
- Task 13 と同じ分割パターンを踏襲する
