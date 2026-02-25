# Task 41: MainWindow ensure* メソッドの棚卸し・分類調査

## Workstream D-1: MainWindow ensure* 群の用途別分離（調査）

## 目的

`MainWindow` の全 `ensure*` メソッドを棚卸しし、「初期化」「依存注入」「シグナル配線」の 3 分類で整理する。後続タスク（配線系の分離）の入力資料を作成する。

## 背景

- `MainWindow` には 36 個以上の `ensure*` メソッドが集中している
- 1 つの `ensure*` 内にオブジェクト生成・依存注入・シグナル配線が混在しているケースがある
- どのメソッドを分離対象とすべきか、現状の棚卸しが必要

## 対象ファイル（読み取りのみ）

- `src/app/mainwindow.h`
- `src/app/mainwindow.cpp`
- `src/ui/wiring/` 配下の既存 wiring クラス群

## 調査内容

### 1. 全 ensure* メソッドの一覧化

各メソッドについて以下を記録する:
- **メソッド名**
- **行数**（おおよそ）
- **責務分類**: 以下の組み合わせで記載
  - `create`: オブジェクトのインスタンス化
  - `bind`: Deps 構造体の設定、updateDeps() 呼び出し
  - `wire`: connect() によるシグナル/スロット接続
- **connect() の数**: 配線の規模感
- **既存 wiring クラスとの関係**: 既に wiring クラスに一部移行済みか

### 2. 分離候補の優先順位付け

以下の基準で分離の優先度を評価する:
- **配線量が多い**（connect() が 5 個以上）
- **責務が混在している**（create + bind + wire が 1 メソッドに同居）
- **既存 wiring クラスの拡張で対応可能**

### 3. 推奨分割単位の定義

以下の単位で分離先クラスを整理する:
- `MatchCoordinator` 関連配線 → 新規 or 既存 wiring クラス
- `DialogCoordinator` 関連配線 → 新規 or 既存 wiring クラス
- `RecordNavigationHandler` 関連配線 → 新規 or 既存 wiring クラス
- `ConsiderationWiring` 関連配線 → 既存 `ConsiderationWiring` の拡張
- その他のまとまり

## 制約

- コード変更は行わない（調査のみ）
- 出力はマークダウン形式の分類表とする

## 出力

以下の内容を含む調査結果を出力する:

1. **ensure* メソッド分類表**（メソッド名 / 行数 / 責務 / connect数 / 備考）
2. **分離候補の優先順位リスト**
3. **推奨分割単位と対応する ensure* メソッドのマッピング**
4. **依存関係の注意点**（初期化順序の前提、循環依存のリスク等）
5. **後続タスクへの推奨実施順序**

調査結果は `docs/dev/ensure-inventory.md` に出力する。
