# Task 02: usi.cpp 分割（テーマA: 巨大ファイル分割）

## 目的

`src/engine/usi.cpp`（1084行）を責務ごとに分割し、1000行以下にする。

## 背景

- usi.cpp はUSIプロトコルエンジンの主要インターフェースで1084行
- エンジンのライフサイクル管理・コマンド送受信・状態管理が混在
- ヘッダ（usi.h）も303行と大きい
- テーマA（巨大ファイル分割）の第2優先対象

## 事前調査

### Step 1: 現状の責務分析

```bash
# メソッド一覧
rg -n "^void Usi::|^bool Usi::|^int Usi::|^QString Usi::|^QStringList Usi::" src/engine/usi.cpp

# ヘッダの構造
cat src/engine/usi.h

# 関連ファイル
rg "class Usi" src --type-add 'header:*.h' --type header
rg "#include.*usi\.h" src --type cpp -l

# 既存の分割済みクラス
ls src/engine/

# シグナル/スロット接続
rg "\\bUsi\\b" src/engine/ --type cpp -n | head -40
```

### Step 2: 責務の3分類

ファイルを読み、メソッドを以下のように分類:

1. **エンジンコマンド発行**: position/go/stop等のコマンド構築・送信
2. **エンジン状態管理**: readyok/bestmove等の状態遷移
3. **評価情報処理**: info行パース・評価値更新

注: 既に `usiprotocolhandler.cpp`、`engineprocessmanager.cpp` が存在するので重複を避ける。

## 実装手順

### Step 3: 抽出先クラスの設計

調査結果に基づき、以下のようなクラスを検討:

- `src/engine/usicommandbuilder.h/.cpp` — コマンド文字列構築
- `src/engine/usistatemanager.h/.cpp` — エンジン状態遷移管理

既存の `usiprotocolhandler.cpp`（902行、Task 06で分割予定）との責務境界を明確にする。

### Step 4-8: （Task 01と同じ手順）

ヘッダ作成 → メソッド移動 → CMakeLists.txt更新 → ビルド・テスト → 構造KPI更新

## 完了条件

- [ ] `usi.cpp` が分割前の1084行から20%以上減少（目標: 800行以下）
- [ ] `usi.h` のメンバ数が減少している
- [ ] 全テスト通過
- [ ] 構造KPI例外値が最新化されている
- [ ] `tst_usiprotocolhandler` が通過している

## KPI変化目標

- 1000行超ファイル数: 2 → 1（Task 01完了後の値から）
- `usi.cpp`: 1084 → 800以下

## 注意事項

- USIプロトコルの正確性を維持する（手順の順序依存が多い）
- `usiprotocolhandler.cpp`（902行）との責務重複に注意
- エンジンプロセスのライフサイクルは `engineprocessmanager.cpp` が管理済み
