# Task 14: wiring 退行検知テスト拡張（ISSUE-022 / P2）

## 概要

signal/slot 配線の抜け漏れを検知するテストを拡充し、wiring 変更時の退行検知精度を向上させる。

## 現状

| テストファイル | テストメソッド数 | 内容 |
|---|---|---|
| `tst_wiring_contracts.cpp` | 17 | MatchCoordinatorWiring のシグナル転送テスト |
| `tst_wiring_slot_coverage.cpp` | 4 | ソース解析型テスト（接続漏れ、重複接続、ensure呼び出し） |

### tst_wiring_slot_coverage の現在のテスト:
1. `allWiringPublicSlots_areConnected` - 全 public slots が接続されているか
2. `noDuplicateConnections_inWiringFiles` - 重複 connect がないか
3. `eachWiringFile_hasConnections` - 各 wiring.cpp に connect があるか
4. `registryEnsureMethods_areInvoked` - ensure* が実際に呼ばれているか

## 手順

### Step 1: 不足検知パターンの洗い出し

以下のパターンを追加検討する:

1. **依存未注入検知**: `updateDeps()` が呼ばれていない Deps 構造体のメンバがないか
2. **シグナル未接続検知**: `signals:` セクションで宣言されたシグナルが `connect()` のソースとして使われているか
3. **スロット引数型ミスマッチ**: connect のシグナル/スロットのパラメータ型が一致しているか（コンパイル時に検出されるが、文字列ベース接続が混在する場合）
4. **接続切断漏れ**: `disconnect()` なしでオブジェクトが破棄されるパターン（Qt parent が適切なら問題ない）

### Step 2: 主要 wiring クラスの契約テスト追加

`tst_wiring_contracts.cpp` に以下の wiring クラスのテストを追加:

```cpp
// 既存: MatchCoordinatorWiring
// 追加候補:
void wireForwarding_analysisTabWiring_...();
void wireForwarding_csaGameWiring_...();
void wireForwarding_considerationWiring_...();
void wireForwarding_playerInfoWiring_...();
```

各 wiring クラスの主要シグナル転送が正しく行われることをテストする。

### Step 3: slot_coverage の検査対象拡大

1. `registryEnsureMethods_areInvoked` の対象を新しいレジストリに拡大:
   - `gamewiringssubregistry.h` 等
2. `allWiringPublicSlots_areConnected` の検査対象に新しい wiring ファイルを追加

### Step 4: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_wiring_contracts|tst_wiring_slot_coverage|tst_wiring_csagame|tst_wiring_analysistab|tst_wiring_consideration|tst_wiring_playerinfo" --output-on-failure
   ```

## 完了条件

- wiring 変更時の退行検知精度が向上する
- 新規テストが全パスする

## 注意事項

- ソース解析型テストはビルド不要でファイル内容だけで検証するため高速
- テストスタブが必要な場合は既存の `test_stubs_*` パターンに従う
- ISSUE-011 完了後に着手する
