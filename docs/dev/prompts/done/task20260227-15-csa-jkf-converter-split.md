# Task 15: CSA/JKF Converter パイプライン分割

## 目的
`csatosfenconverter.cpp`（1004行）と `jkftosfenconverter.cpp`（949行）をパイプライン構造に分割し、600行以下にする。

## 前提
- Task 12 の parsecommon 拡充が完了していること
- Task 13-14 のパターンを参考にする

## 作業内容

### Phase A: CSA Converter 分割

#### 1. CSA Lexer の抽出
`src/kifu/formats/csalexer.h/.cpp` を新規作成する:
- CSA 形式固有のトークン化（`+7776FU` 形式）
- ヘッダー行（`V2.2`, `N+`, `N-` 等）の解析
- 特殊行（`%TORYO`, `%CHUDAN` 等）の判定

#### 2. CSA Parser の分離
- `csatosfenconverter` をトークン受け取り→構造体変換に限定する
- 共通処理は `parsecommon` を活用する

### Phase B: JKF Converter 分割

#### 1. JKF Parser の分離
JKF は JSON ベースなので Lexer は不要（Qt の JSON パーサーを使用）。
`src/kifu/formats/jkfmoveparser.h/.cpp` を新規作成する:
- JSON 構造体から指し手構造体への変換
- 特殊フィールドの解析

#### 2. JKF Converter のスリム化
- 共通処理は `parsecommon` を活用する
- JSON パース結果の変換ロジックを `jkfmoveparser` に移動する

### CMakeLists.txt 更新

### テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] `csatosfenconverter.cpp` が 600行以下
- [ ] `jkftosfenconverter.cpp` が 600行以下
- [ ] `tst_csaconverter.cpp`, `tst_jkfconverter.cpp` が全件 PASS する
- [ ] 全テストが PASS する
- [ ] 構造KPIテストの例外リストから両ファイルを削除可能

## 注意事項
- CSA は比較的単純なフォーマットなので分割は容易
- JKF は JSON ベースで構造が異なるため、他の変換器と同じ Lexer パターンは不要
- 2つの変換器を同時に作業するが、Phase A → B の順で段階的に進める
