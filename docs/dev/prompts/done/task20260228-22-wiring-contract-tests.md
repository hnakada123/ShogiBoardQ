# Task 22: 配線契約テスト追加（Phase 2: テスト基盤拡充）

## 目的

Wiring層のシグナル転送チェーンを検証する契約テストを追加し、配線変更時の回帰を検知する。

## 背景

- `src/ui/wiring/` に19クラスが存在し、シグナル/スロット接続の責務を担う
- 配線の変更頻度が高く、シグナル転送チェーンが途切れても検知手段がない
- 構造テスト（`tst_structural_kpi`）は接続の存在を検証するが、動作の正しさは未検証
- 包括的改善分析 §3.4 P1, future-improvement テーマD

## 対象テスト

新規作成:
- `tests/tst_wiring_contracts.cpp`

## 事前調査

### Step 1: 変更頻度の高いWiringクラスの特定

```bash
# 最近の変更頻度（コミット数）で優先度を判断
for f in src/ui/wiring/*.cpp; do
    echo "$(git log --oneline -- "$f" | wc -l) $f"
done | sort -rn | head -10
```

### Step 2: Wiringクラスの接続パターン分析

```bash
# 各Wiringクラスの connect() 呼び出し数
for f in src/ui/wiring/*.cpp; do
    echo "$(rg -c 'connect\(' "$f" 2>/dev/null || echo 0) $f"
done | sort -rn
```

### Step 3: 既存テストの確認

```bash
# wiring関連のテスト有無
rg -l "wiring\|Wiring" tests/
```

## 実装手順

### Step 4: テスト対象の選定（優先2件）

変更頻度・接続数・重要度から以下を優先:

1. **MatchCoordinatorWiring** — 対局系の中核配線。GameStartCoordinator/GameEndHandler等との接続
2. **UiActionsWiring** — メニュー/ツールバーのアクション配線。4つのサブWiringを集約

### Step 5: テストスタブの設計

配線テストでは実オブジェクトの代わりにスタブを使用:

```cpp
// テスト用の最小スタブ
class StubSender : public QObject {
    Q_OBJECT
signals:
    void someSignal();
};

class StubReceiver : public QObject {
    Q_OBJECT
public:
    int callCount = 0;
public slots:
    void onSomeSignal() { ++callCount; }
};
```

### Step 6: 契約テストの実装

```cpp
// tst_wiring_contracts.cpp
class TestWiringContracts : public QObject
{
    Q_OBJECT

private slots:
    // MatchCoordinatorWiring の契約テスト
    void matchCoordinatorWiring_gameStartSignalForwarded();
    void matchCoordinatorWiring_gameEndSignalForwarded();

    // UiActionsWiring の契約テスト
    void uiActionsWiring_fileActionsConnected();
    void uiActionsWiring_gameActionsConnected();
};
```

テスト内容:
- `QSignalSpy` で出力シグナルの発火を検証
- スタブの `callCount` でスロット呼び出しを検証
- 接続の存在確認（`QObject::receivers()` または `QSignalSpy::isValid()`）

### Step 7: CMakeLists.txt に追加

```bash
scripts/update-sources.sh
```

テストファイルを `tests/CMakeLists.txt` に追加。

### Step 8: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
ctest --test-dir build -R wiring_contracts --output-on-failure -V
```

## 完了条件

- [ ] `tests/tst_wiring_contracts.cpp` が作成されている
- [ ] 優先2件のWiringクラスの契約テストが実装されている
- [ ] 各Wiringクラスの主要シグナル転送チェーンが検証されている
- [ ] 全テスト通過
- [ ] テストは `QSignalSpy` を活用している

## KPI変化目標

- テスト数: +1（新規テストファイル）
- 配線契約テストケース: 0 → 6件以上

## 注意事項

- Wiring クラスの `Deps` struct には多くの依存があるため、最小限のスタブで構成する
- `connect()` でラムダを使用しない（テストコードでも CLAUDE.md 準拠）
- テストスタブは `tests/stubs/` に配置（既存パターンに従う）
- 実際のUIコンポーネントを生成せず、シグナル/スロットのみをテストする
