# Task 01: ServiceRegistry サブレジストリ化と ensure* 削減

## 概要

`MainWindowServiceRegistry` の `ensure*` メソッドが閾値 35 に張り付いており、成長余白がない。
ドメインカテゴリごとにサブレジストリクラスを抽出し、`ensure*` 数を 35 → 28 以下に削減する。

## 前提条件

- なし（最初に着手するタスク）

## 現状

- `src/app/mainwindowserviceregistry.h`: 35 ensure* メソッド（KPI上限 35）
- 実装は 6 .cpp ファイルに分割済み:
  - `mainwindowserviceregistry.cpp`（コンストラクタ）
  - `mainwindowanalysisregistry.cpp`（Analysis系）
  - `mainwindowboardregistry.cpp`（Board/共通系）
  - `mainwindowgameregistry.cpp`（Game系 — 最大 576行）
  - `mainwindowkifuregistry.cpp`（Kifu系）
  - `mainwindowuiregistry.cpp`（UI系）
- カテゴリ別内訳:
  - Game系: 16 ensure* （最多）
  - Kifu系: 8 ensure*
  - Analysis系: 4 ensure*
  - UI系: 4 ensure*
  - Board/共通系: 3 ensure*

## 実施内容

### Step 1: サブレジストリクラスの設計

各カテゴリの ensure* を独立クラスに抽出する設計を行う。

候補:
- `GameSubRegistry` — Game系 16 ensure* を移管
- `KifuSubRegistry` — Kifu系 8 ensure* を移管

この2つで 24 ensure* が移管でき、ServiceRegistry 本体は 11 に削減される。

### Step 2: Game系サブレジストリの抽出

1. `src/app/gamesubregistry.h/.cpp` を新規作成
2. Game系 ensure* 16メソッドと関連ヘルパーを移動
3. `mainwindowgameregistry.cpp` の実装を新クラスに移管
4. `MainWindowServiceRegistry` に `GameSubRegistry*` メンバを追加し、委譲メソッドまたは直接アクセスに変更
5. 全呼び出し元（MainWindow, Wiring, Coordinator 等）を更新

### Step 3: Kifu系サブレジストリの抽出

1. `src/app/kifusubregistry.h/.cpp` を新規作成
2. Kifu系 ensure* 8メソッドと関連ヘルパーを移動
3. `mainwindowkifuregistry.cpp` の実装を新クラスに移管
4. 呼び出し元を更新

### Step 4: KPI閾値更新

1. `tests/tst_structural_kpi.cpp` の `maxEnsureMethods` を 35 → 新しい実測値に更新
2. CMakeLists.txt に新規ファイルを追加

## 完了条件

- `sr_ensure_methods <= 28`（KPIテスト通過）
- ビルド成功（`cmake --build build`）
- 全テスト通過（`ctest --test-dir build`）
- 翻訳更新不要（UIテキスト変更なし）

## 注意事項

- `ensure*` メソッドの呼び出しチェーンを慎重に追跡すること。特に `MainWindowDepsFactory` からの参照
- サブレジストリは `QObject` 継承とし、`MainWindowServiceRegistry` を parent にする（所有権ルール準拠）
- `connect()` にラムダを使わないこと（CLAUDE.md 準拠）
- `MainWindowServiceRegistry` への friend class 追加は避ける
