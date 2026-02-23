# Task 20: UiActionsWiring の機能別分割

`src/ui/wiring/uiactionswiring.h/.cpp` を機能ドメイン別のクラスに分割してください。

## 背景

現在の `UiActionsWiring` は40行に44個の `connect()` が集中しており、非常に密度が高い。メニューアクション全般を1クラスで扱っているが、機能別に分割した方が各ドメインの見通しがよくなる。

## 作業内容

### Step 1: 現状の分析

`uiactionswiring.cpp` の全 `connect()` を読み、以下のカテゴリに分類:

- **ファイル操作**: 新規、開く、保存、エクスポート等
- **編集操作**: コピー、貼り付け、局面編集等
- **対局操作**: 対局開始、投了、中断等
- **表示操作**: 反転、ドック表示切替等
- **その他**: 分類が難しいもの

### Step 2: 分割クラスの作成

カテゴリごとに新クラスを作成:

- `src/ui/wiring/fileactionswiring.h/.cpp` — ファイル操作の connect
- `src/ui/wiring/editactionswiring.h/.cpp` — 編集操作の connect
- `src/ui/wiring/gameactionswiring.h/.cpp` — 対局操作の connect
- `src/ui/wiring/viewactionswiring.h/.cpp` — 表示操作の connect

各クラスは `Deps` 構造体パターン（既存の Wiring クラスと同様）を使用。

### Step 3: UiActionsWiring の整理

分割後の `UiActionsWiring` は以下のいずれか:

**案A**: 4つのサブ Wiring を所有するファサードとして残す
```cpp
class UiActionsWiring {
    FileActionsWiring m_file;
    EditActionsWiring m_edit;
    GameActionsWiring m_game;
    ViewActionsWiring m_view;
public:
    void wireAll(Deps& deps);
};
```

**案B**: `UiActionsWiring` を廃止し、MainWindow が4つのサブ Wiring を直接保持

どちらが既存コードへの影響が小さいかを確認して選択する。

### Step 4: MainWindow の更新

`ensureActionsWiring()` や関連する ensure/update メソッドを新クラスに対応。

## 制約

- CLAUDE.md のコードスタイルに準拠
- `connect()` にラムダを使わない
- 全既存テストが通ること
- 既存の Wiring クラスのパターン（Deps 構造体 + updateDeps）に従う
