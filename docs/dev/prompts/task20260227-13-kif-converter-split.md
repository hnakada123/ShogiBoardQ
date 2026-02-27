# Task 13: KIF Converter パイプライン分割

## 目的
`kiftosfenconverter.cpp`（1278行）をパイプライン構造に分割し、600行以下にする。

## 前提
- Task 12 の parsecommon 拡充が完了していること

## 作業内容

### 1. KIF Lexer の抽出
`src/kifu/formats/kiflexer.h/.cpp` を新規作成する。

KIF 形式固有の字句解析を担当:
- 行の種類判定（手番行、コメント行、ヘッダー行、分岐行 等）
- トークン化
- KIF 形式固有の表記パース（漢数字、駒名 等）

### 2. KIF Parser の分離
`kiftosfenconverter` に残す構文解析部分を整理する:
- Lexer のトークンを受け取り、指し手構造体に変換する
- ヘッダー情報の解析
- 分岐の処理

### 3. 共通 MoveApplier/Formatter の利用
Task 12 で `parsecommon` に集約した共通処理を活用する:
- 指し手の盤面適用は共通関数を使用
- SFEN 生成は共通関数を使用

### 4. CMakeLists.txt 更新
新規ファイルを追加する。

### テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] `kiftosfenconverter.cpp` が 600行以下
- [ ] `kiflexer.cpp` が 600行以下
- [ ] `tst_kifconverter.cpp` が全件 PASS する
- [ ] 全テストが PASS する

## 注意事項
- KIF フォーマットは最も複雑（漢数字、分岐、変化手順 等）なので慎重に進める
- `KifToSfenConverter` の公開API（`convert` メソッド等）は変更しない
- 内部実装の分割のみ行う
