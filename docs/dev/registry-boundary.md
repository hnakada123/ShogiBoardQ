# レジストリ境界仕様

## 概要

`MainWindowServiceRegistry` を頂点とするレジストリ階層は、遅延初期化 (`ensure*`) メソッドの管理と成長を制御する。本文書はレイヤごとの責務と上限を定義する。

## レイヤ構成

```
MainWindowServiceRegistry (11 ensure*)
├── MainWindowFoundationRegistry (15 ensure*)  ... 共通基盤 (Tier 0/1)
├── GameSubRegistry (6 ensure*)                ... Game系 状態・コントローラ
│   ├── GameSessionSubRegistry (6 ensure*)     ... セッション管理
│   └── GameWiringSubRegistry (4 ensure*)      ... 配線管理
└── KifuSubRegistry (8 ensure*)                ... 棋譜・ナビ・I/O
```

## 各レイヤの責務

### MainWindowServiceRegistry
- **責務**: UI / Analysis / Board 系の ensure* とオーケストレーション
- **上限**: ensure* ≤ 11（KPIテスト `serviceRegistryEnsureLimit` で検証）
- **方針**: 新規 ensure* は追加しない。必要な場合はサブレジストリに委譲する

### MainWindowFoundationRegistry
- **責務**: 複数ドメインが共有する Tier 0/1 コンポーネントの遅延初期化
- **上限**: ensure* ≤ 17
- **特徴**: 他の ensure* を呼ばない（ensurePlayerInfoController のみ内部呼出し）

### GameSubRegistry
- **責務**: Game系の状態・コントローラ管理（TimeController, ReplayController 等）
- **上限**: ensure* ≤ 8
- **サブレジストリ**: GameSessionSubRegistry, GameWiringSubRegistry

### GameSessionSubRegistry
- **責務**: セッションライフサイクル（クリーンアップ・ライブセッション・コア初期化）
- **上限**: ensure* ≤ 8

### GameWiringSubRegistry
- **責務**: 配線管理（MatchCoordinator 配線・CSA 配線・連続対局・ターン同期）
- **上限**: ensure* ≤ 6

### KifuSubRegistry
- **責務**: 棋譜関連（ナビゲーション・ファイルI/O・コメント・定跡）
- **上限**: ensure* ≤ 10

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
| **Create-only** | ガード + 生成のみ、updateDeps なし | ensureTimeController, ensureRecordPresenter |
| **Create + RefreshDeps** | ガード付き生成 + 毎回 updateDeps | ensureTurnStateSyncService, ensureBranchNavigationWiring |
| **Always-call** | ガードなし、毎回全処理実行 | ensureUiStatePolicyManager |

Create-only パターンは分離不要。Create + RefreshDeps パターンのみ分離する。

## 新規 ensure* の追加ルール

1. **ServiceRegistry への直接追加は禁止**（上限到達済み）
2. 新規コンポーネントは適切なサブレジストリに追加する
3. 複数ドメインから共有される場合は FoundationRegistry に追加する
4. サブレジストリの上限に達した場合は、さらなるサブレジストリ分割を検討する
5. 依存更新が必要な場合は Create + RefreshDeps パターンを適用する

## KPIテスト

`tst_structural_kpi.cpp` の以下のテストで境界を検証する：

- `serviceRegistryEnsureLimit`: ServiceRegistry ≤ 11
- `subRegistryEnsureLimits`: 各サブレジストリの上限検証
