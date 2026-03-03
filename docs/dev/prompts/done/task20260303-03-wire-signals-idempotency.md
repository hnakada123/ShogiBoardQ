# Task 20260303-03: wireSignals() 冪等性強化

## 概要
`KifuDisplayCoordinator::wireSignals()` に冪等性ガードを追加し、複数回呼び出しによる重複接続を防止する。

## 優先度
Medium

## 背景
- `wireSignals()` は複数 `connect` を行うが、配線済みフラグや `Qt::UniqueConnection` を持たない
- 現状は `BranchNavigationWiring::wireSignals()` 側が `KifuDisplayCoordinator` の生成を1回に抑えているため問題は起きていない
- しかし将来の再配線経路追加で重複接続リスクがある
- 1操作でハンドラが2回以上実行される不具合を予防する

## 対象ファイル

### 修正対象
1. `src/ui/coordinators/kifudisplaycoordinator.h` - フラグ追加
2. `src/ui/coordinators/kifudisplaycoordinator.cpp` - 冪等性ガード追加

### 参照（変更不要だが理解が必要）
- `src/ui/wiring/branchnavigationwiring.cpp:87-99` - 呼び出し元の確認

## 実装手順

### Step 1: メンバ変数追加

`src/ui/coordinators/kifudisplaycoordinator.h` の private メンバに追加:

```cpp
bool m_signalsWired = false;
```

既存の `bool m_pendingNavResultCheck = false;` の近くに配置する。

### Step 2: wireSignals() に冪等ガードを追加

`src/ui/coordinators/kifudisplaycoordinator.cpp` の `wireSignals()` 冒頭にガードを追加:

**Before (145-176行目):**
```cpp
void KifuDisplayCoordinator::wireSignals()
{
    if (m_navController != nullptr) {
        connect(m_navController, &KifuNavigationController::navigationCompleted,
                this, &KifuDisplayCoordinator::onNavigationCompleted);
```

**After:**
```cpp
void KifuDisplayCoordinator::wireSignals()
{
    if (m_signalsWired)
        return;
    m_signalsWired = true;

    if (m_navController != nullptr) {
        connect(m_navController, &KifuNavigationController::navigationCompleted,
                this, &KifuDisplayCoordinator::onNavigationCompleted);
```

### Step 3: ビルド確認
```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- `wireSignals()` を2回呼んでも `connect` は1回分しか実行されない
- ビルド成功（warning なし）
- 既存テスト全件成功

## 設計判断メモ
- `Qt::UniqueConnection` 一括付与も選択肢だが、フラグ方式の方が軽量かつ明示的
- フラグ方式なら将来 `unwireSignals()` を追加する際も `m_signalsWired = false` でリセットできる
- `BranchNavigationWiring` 側のガードは既存のまま維持（多層防御）
