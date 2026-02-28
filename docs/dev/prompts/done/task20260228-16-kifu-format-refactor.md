# Task 16: 変換層 Parse/Normalize/Apply 分離（テーマF: 棋譜/通信仕様境界整理）

## 目的

`src/kifu/formats/` の変換器群を `Parse → Normalize → Apply` の3層に明示分離し、重複処理を削減する。

## 背景

- 棋譜変換器（KIF/KI2/CSA/JKF/USEN/SFEN）で入力検証・変換・副作用が同居
- フォーマット追加時に横断影響が出やすい
- テーマF（棋譜/通信仕様境界整理）の主作業

## 事前調査

### Step 1: 変換器の一覧と行数

```bash
wc -l src/kifu/formats/*.cpp src/kifu/formats/*.h | sort -nr | head -30
ls src/kifu/formats/
```

### Step 2: 共通パターンの分析

```bash
# 共通パース処理
cat src/kifu/formats/parsecommon.h
cat src/kifu/formats/parsecommon.cpp | head -100

# 各変換器の構造比較
rg "class.*Converter" src/kifu/formats/ --type-add 'header:*.h' --type header
rg "parse|normalize|apply|convert" src/kifu/formats/ --type-add 'header:*.h' --type header -n | head -40
```

### Step 3: 重複処理の特定

```bash
# 各変換器の公開メソッド比較
for f in src/kifu/formats/*converter*.h; do
    echo "=== $(basename $f) ==="
    rg "static|public:" "$f" -A 3 | head -20
done
```

## 実装手順

### Step 4: 3層の設計

1. **Parse層**: 文字列 → 中間表現（ASTまたはトークン列）
   - 各フォーマット固有のパース処理
   - エラー位置の記録

2. **Normalize層**: 中間表現 → 共通内部表現
   - 駒名・座標の正規化
   - 省略記法の展開
   - 共通の `parsecommon.cpp` を活用

3. **Apply層**: 共通内部表現 → ShogiBoard/GameRecordModel への適用
   - 盤面状態の更新
   - 棋譜モデルへの追記

### Step 5: 共通中間表現の整理

既存の共通処理（`parsecommon.cpp` 640行）を整理:
- Parse/Normalize/Applyのどの層に属するか分類
- 重複する変換ロジックを共通関数に統合

### Step 6: 段階的リファクタリング

1つの変換器から着手（例: `csatosfenconverter`）:
1. Parse/Normalize/Applyの境界をメソッド分割で明確化
2. 動作を変えずにリファクタリング
3. テストで回帰なしを確認
4. 成功パターンを他の変換器に適用

### Step 7: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure

# 個別変換器テスト
ctest --test-dir build -R converter --output-on-failure
```

## 完了条件

- [ ] 少なくとも1つの変換器でParse/Normalize/Apply分離が完了
- [ ] 重複処理が20%以上削減されている
- [ ] 全変換器テスト通過（`tst_*converter*`）
- [ ] `parsecommon.cpp` の責務が明確化されている

## KPI変化目標

- `kifu/formats` の重複関数/分岐を20%以上削減
- 変換器の構造が統一パターンに近づく

## 注意事項

- 動作仕様は変更しない（リファクタリングのみ）
- 各フォーマットのエッジケース（文字コード、改行コード等）を壊さない
- 既存テスト（`tst_csaconverter`, `tst_kifconverter`, `tst_ki2converter`, `tst_jkfconverter`, `tst_usenconverter`, `tst_usiconverter`）を全て通過させる
