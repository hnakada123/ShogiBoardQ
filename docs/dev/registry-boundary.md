# レジストリ境界仕様

## 概要

部品の生成と依存の更新（`ensure*`）は、MainWindow ではなく3つのレジストリが受け持つ。本文書はレジストリごとの責務と、`ensure*` の数の上限を定義する。上限は `tests/tst_structural_kpi.cpp` が確かめる。

## レイヤ構成

```
MainWindowServiceRegistry（ensure* 24）       ... Game・UI・解析・盤・自動化。実装は下の分割ファイル
├── MainWindowFoundationRegistry（ensure* 15）... 共通基盤（子オブジェクト、foundation() で取得）
└── KifuSubRegistry（ensure* 8）              ... 棋譜・ナビ・I/O（子オブジェクト、kifu() で取得）
```

`MainWindowServiceRegistry` は1つのクラスで、実装を次のファイルに分けている。`gamesubregistry*.cpp` は名前に「subregistry」と付くが、別のクラスではない。

| ファイル | 内容 | ensure* |
|---------|------|--------:|
| `gamesubregistry.cpp` | Game系の状態・コントローラ（時計、リプレイ、ゲーム状態、対局開始）、手番同期・手戻し | 4 |
| `gamesubregistry_session.cpp` | セッション（開始前の整理、ライブセッション、対局の流れ、コアの初期化、ライフサイクル） | 5 |
| `gamesubregistry_wiring.cpp` | 配線（司令塔の配線、CSA 通信対局の配線、連続対局）、司令塔の初期化 | 3 |
| `mainwindowuiregistry.cpp` | UI系（棋譜表示、ダイアログ、ドックのレイアウト、棋譜欄の行変更） | 4 |
| `mainwindowanalysisregistry.cpp` | 解析系（読み筋のクリック、検討の局面、検討の配線、USI コマンド） | 4 |
| `mainwindowboardregistry.cpp` | 盤・局面編集（盤の操作、局面編集、SFEN の盤への適用） | 3 |
| `mainwindowautomationregistry.cpp` | 自動化 API（`--automation`） | 1 |
| `mainwindowuibootstrapper.cpp`・`mainwindowdockbootstrapper.cpp`・`mainwindowlifecyclewiring.cpp`・`mainwindowserviceregistry.cpp` | 起動・終了の手順、ドックの作成、主要な操作 | 0 |

ensure* の数は 2026-10-14 時点の値である。

## 各レイヤの責務

### MainWindowServiceRegistry
- **責務**: Game・UI・解析・盤・自動化の ensure* と、起動・終了などの主要な操作
- **上限**: ensure* ≤ 25（KPIテスト `serviceRegistryEnsureLimit`。`mainwindowserviceregistry.h` の宣言を数える）
- **方針**: 新しい部品は、上の分割ファイルのうち責務の近いものに足す。上限を超える場合は、既存の ensure* をまとめるか、別のサブレジストリ（クラス）に切り出す

### MainWindowFoundationRegistry
- **責務**: 複数ドメインが共有する部品（UI状態ポリシー、対局者情報、棋譜ナビゲーション、コメント、盤面同期、通知、評価値グラフ、メニュー、言語、駒音、ドック生成、局面編集、解析結果、持将棋）の生成
- **上限**: ensure* ≤ 15（KPIテスト `subRegistryEnsureLimits`）
- **特徴**: 生成の途中でほかの ensure* を直接呼ぶのは `ensurePlayerInfoController`（`ensurePlayerInfoWiring` を呼ぶ）だけ。ほかのレジストリの部品が要るときは、Deps に渡すコールバックの中で呼ぶ

### KifuSubRegistry
- **責務**: 棋譜関連（分岐ナビゲーション、棋譜ファイルの読み書き、棋譜のエクスポート、棋譜の追記と読み込み、棋譜読み込み担当、棋譜データ、定跡）
- **上限**: ensure* ≤ 8（KPIテスト `subRegistryEnsureLimits`）

FoundationRegistry と KifuSubRegistry は、2026-10-14 時点で上限に達している。

## ensure* の2段構造パターン

依存更新が必要な ensure* メソッドは、以下の3メソッドに分離する：

```cpp
// 公開API（互換性維持）
void ensureX() {
    if (!m_mw.m_x) {
        createX();           // 生成（private、1回のみ）
    }
    refreshXDeps();          // 依存更新（public、何度でも呼べる）
}

// 生成（private）: オブジェクト作成 + signal/slot 配線
void createX() {
    m_mw.m_x = new X(...);
    connect(...);
}

// 依存更新（public）: Deps 構造体の再構築
void refreshXDeps() {
    if (!m_mw.m_x) return;
    X::Deps deps;
    deps.a = m_mw.m_a;
    // ...
    m_mw.m_x->updateDeps(deps);
}
```

### パターン適用基準

| パターン | 条件 | 例 |
|---------|------|-----|
| **Create-only** | ガード + 生成のみ、updateDeps なし | ensurePositionEditController, ensureDockLayoutManager |
| **Create + RefreshDeps** | ガード付き生成 + 毎回 updateDeps（3メソッドに分ける） | ensureBranchNavigationWiring, ensureMatchCoordinatorWiring, ensureSessionLifecycleCoordinator |
| **Always-call** | 生成は1回だけ。呼ぶたびに依存を設定し直す（分けずに1つの関数で書く） | ensureUiStatePolicyManager, ensureTimeController, ensureRecordPresenter |

Create-only と Always-call は分離不要。依存の設定が長い Create + RefreshDeps パターンだけ分離する。

## 新規 ensure* の追加ルール

1. 新規コンポーネントは、責務の合うレジストリ（ServiceRegistry なら分割ファイル）に追加する
2. 複数ドメインから共有される場合は FoundationRegistry に追加する
3. 依存更新が必要な場合は Create + RefreshDeps パターンを適用する
4. 上限に達しているレジストリに追加する場合は、同時に既存 ensure* の統合・削除を検討する。統合できない場合は、PR 説明に理由を書き、KPI テストの上限値を更新する（レビュー対象）
5. 上限に達し続ける場合は、さらなるサブレジストリ分割を検討する
6. refresh*Deps() は ensure* とは別カウントとし、自由に追加できる

## KPIテスト

`tst_structural_kpi.cpp` の以下のテストで境界を検証する：

- `mainWindowEnsureLimit`: MainWindow ≤ 0（MainWindow には ensure* を置かない）
- `serviceRegistryEnsureLimit`: ServiceRegistry ≤ 25
- `subRegistryEnsureLimits`: FoundationRegistry ≤ 15、KifuSubRegistry ≤ 8

### refresh* の除外

KPI テストの検索パターンは `^\s*void\s+ensure` であり、`refresh*Deps()` メソッドはカウント対象外となる。これにより、依存更新メソッドは ensure* の上限に影響せず自由に追加できる。
