# Task 20260303-05: KifuLoadCoordinator 再生成時の deleteLater() 境界明確化

## 概要
`KifuSubRegistry::createAndWireKifuLoadCoordinator()` で旧 coordinator を `deleteLater()` した直後に新 coordinator を生成している。旧インスタンスからの遅延シグナルが新インスタンスと混在するリスクを排除するため、明示的な切断処理を追加する。

## 優先度
Medium

## 背景
- `deleteLater()` は次のイベントループで実際の削除が行われる
- その間に旧インスタンスに積まれた queued signal が飛ぶ可能性がある
- 新旧 coordinator のイベントが混在すると予期しない動作になる

## 対象ファイル

### 修正対象
1. `src/app/kifusubregistry.cpp` - `createAndWireKifuLoadCoordinator()` に明示切断追加

### 参照（変更不要だが理解が必要）
- `src/kifu/kifuloadcoordinator.h` - KifuLoadCoordinator のシグナル・スロット確認
- `src/app/kifusubregistry.h` - クラス構造確認

## 実装手順

### Step 1: 旧 coordinator の明示切断を追加

`src/app/kifusubregistry.cpp` の `createAndWireKifuLoadCoordinator()` を修正:

**Before (339-345行目):**
```cpp
void KifuSubRegistry::createAndWireKifuLoadCoordinator()
{
    // 既存があれば遅延破棄
    if (m_mw.m_kifuLoadCoordinator) {
        m_mw.m_kifuLoadCoordinator->deleteLater();
        m_mw.m_kifuLoadCoordinator = nullptr;
    }
```

**After:**
```cpp
void KifuSubRegistry::createAndWireKifuLoadCoordinator()
{
    // 既存があれば全シグナル切断後に遅延破棄
    if (m_mw.m_kifuLoadCoordinator) {
        m_mw.m_kifuLoadCoordinator->disconnect();
        m_mw.m_kifuLoadCoordinator->deleteLater();
        m_mw.m_kifuLoadCoordinator = nullptr;
    }
```

`disconnect()` は引数なしで呼ぶと、そのオブジェクトの全シグナル・全スロット接続を切断する。これにより `deleteLater()` 後のイベントループで旧インスタンスに積まれた queued signal が配送されても、受信側への到達を防ぐ。

### Step 2: ビルド確認
```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- `deleteLater()` 前に `disconnect()` が呼ばれる
- ビルド成功（warning なし）
- 既存テスト全件成功

## 設計判断メモ
- `QObject::disconnect(old, nullptr, nullptr, nullptr)` と `old->disconnect()` は等価。後者の方が簡潔
- `QPointer` での無効化監視は現状では不要（ポインタを即 null 化しているため）
- 生成/配線順: 旧切断 → 旧破棄予約 → 旧ポインタ null 化 → 新生成（既存の流れを維持）
