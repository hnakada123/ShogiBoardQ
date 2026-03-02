# Task 13: 棋譜変換プロパティテスト導入（ISSUE-021 / P2）

## 概要

棋譜フォーマット変換器にランダム入力ベースの検証を段階導入し、境界値入力への耐性を高める。

## 現状

対象テストファイル:
- `tests/tst_kifconverter.cpp`
- `tests/tst_ki2converter.cpp`
- `tests/tst_csaconverter.cpp`
- `tests/tst_jkfconverter.cpp`
- `tests/tst_usiconverter.cpp`
- `tests/tst_usenconverter.cpp`

現在のテストは定型入力に対する期待値検証が中心。

## 手順

### Step 1: 境界値テストケースの設計

以下のカテゴリの入力を生成する:

1. **空入力**: 空文字列、空行のみ、ホワイトスペースのみ
2. **長大行**: 極端に長い行（10KB以上）
3. **記号混在**: UTF-8マルチバイト文字、制御文字、BOM
4. **不完全な棋譜**: 途中で切れた棋譜、ヘッダのみ、手数0
5. **フォーマット混合**: KIF ファイルに CSA 形式が混在
6. **極端な手数**: 非常に多い手数（1000手以上）

### Step 2: テストヘルパーの作成

1. テスト用のランダム入力生成ヘルパーを作成する:
   ```cpp
   namespace KifuTestHelper {
   QString generateRandomKifuLine(int maxLength);
   QString generateCorruptedKifu(const QString& validKifu, int corruptionType);
   QStringList generateBoundaryInputs();
   }
   ```
2. 共通テストヘルパーファイル（`tests/kifu_test_helper.h`）に配置する

### Step 3: 各変換器にテスト追加

各変換器に以下のテストカテゴリを追加:

```cpp
void tst_KifConverter::emptyInput_doesNotCrash()
void tst_KifConverter::longLine_doesNotCrash()
void tst_KifConverter::invalidUtf8_doesNotCrash()
void tst_KifConverter::truncatedInput_doesNotCrash()
```

期待値は「クラッシュしない」「例外を投げない」「不正入力を適切にスキップまたはエラー報告する」こと。

### Step 4: ビルド・テスト

1. `cmake --build build` でビルド確認
2. テスト実行:
   ```bash
   ctest --test-dir build -R "tst_kifconverter|tst_ki2converter|tst_csaconverter|tst_jkfconverter|tst_usiconverter|tst_usenconverter" --output-on-failure
   ```

## 完了条件

- 既存変換器に異常入力カバレッジが追加される
- 全テストパス

## 注意事項

- 真のプロパティテスト（QuickCheck 等）は C++ での導入コストが高い。まずは手動の境界値テストから始める
- クラッシュ検出は ASAN（`sanitize` CI ジョブ）と組み合わせて効果を発揮する
- ISSUE-020 完了後に着手する
