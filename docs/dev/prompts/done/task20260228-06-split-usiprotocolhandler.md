# Task 06: usiprotocolhandler.cpp 分割（テーマA: 巨大ファイル分割）

## 目的

`src/engine/usiprotocolhandler.cpp`（902行）を責務ごとに分割し、600行以下にする。

## 背景

- usiprotocolhandler.cpp はUSIプロトコルのメッセージパース・応答処理で902行
- info行パース・オプション解析・コマンドディスパッチが混在
- テーマA（巨大ファイル分割）の第6優先対象
- Task 02（usi.cpp分割）と密接に関連するため、Task 02完了後に着手することを推奨

## 事前調査

### Step 1: 現状の責務分析

```bash
# メソッド一覧
rg -n "^void UsiProtocolHandler::|^bool UsiProtocolHandler::|^QString UsiProtocolHandler::" src/engine/usiprotocolhandler.cpp

# ヘッダの構造
cat src/engine/usiprotocolhandler.h

# info行パース関連
rg "parseInfo|info " src/engine/usiprotocolhandler.cpp -n | head -20

# option解析関連
rg "parseOption|option" src/engine/usiprotocolhandler.cpp -n | head -20

# テスト
head -30 tests/tst_usiprotocolhandler.cpp
```

### Step 2: 責務の3分類

1. **info行パース**: `info depth ... score ...` 等のパース処理
2. **オプション解析**: `option name ... type ...` のパース・格納
3. **コマンドディスパッチ**: 受信メッセージの振り分け・状態更新

## 実装手順

### Step 3: 抽出先クラスの設計

- `src/engine/usiinfoparser.h/.cpp` — info行の詳細パース
- `src/engine/usioptionparser.h/.cpp` — optionメッセージの解析

既存の `shogiengineinfoparser.cpp`（689行）との関係を確認。重複があれば統合を検討。

### Step 4-8: （Task 01と同じ手順）

ヘッダ作成 → メソッド移動 → CMakeLists.txt更新 → ビルド・テスト → 構造KPI更新

## 完了条件

- [ ] `usiprotocolhandler.cpp` が分割前の902行から20%以上減少（目標: 700行以下）
- [ ] 全テスト通過（特に `tst_usiprotocolhandler`）
- [ ] 構造KPI例外値が最新化されている

## KPI変化目標

- 600行超ファイル数: 減少方向
- `usiprotocolhandler.cpp`: 902 → 700以下

## 注意事項

- Task 02（usi.cpp分割）との整合性を確認する
- USIプロトコルのメッセージ順序依存を壊さない
- `tst_usiprotocolhandler` のテストカバレッジを維持する
