# Task 18: 異常系テーブル駆動テスト拡充（テーマF: 棋譜/通信仕様境界整理）

## 目的

棋譜変換器・USI/CSAプロトコルの異常系テストをテーブル駆動形式で最低30件追加する。

## 背景

- 正常系テストは存在するが、異常入力（欠損・不正手・境界値）のカバレッジが不足
- フォーマット追加・変更時の回帰リスクが高い
- テーマF（棋譜/通信仕様境界整理）の品質保証作業

## 対象テストファイル

既存:
- `tests/tst_csaconverter.cpp`
- `tests/tst_kifconverter.cpp`
- `tests/tst_ki2converter.cpp`
- `tests/tst_jkfconverter.cpp`
- `tests/tst_usenconverter.cpp`
- `tests/tst_usiconverter.cpp`
- `tests/tst_usiprotocolhandler.cpp`
- `tests/tst_csaprotocol.cpp`
- `tests/tst_parsecommon.cpp`

## 事前調査

### Step 1: 既存テストのカバレッジ確認

```bash
# 各テストのテストケース数
for f in tests/tst_*converter*.cpp tests/tst_usiprotocolhandler.cpp tests/tst_csaprotocol.cpp tests/tst_parsecommon.cpp; do
    echo "=== $(basename $f) ==="
    rg "void.*::" "$f" | grep -v "^//" | wc -l
done

# 異常系テストの有無
rg "invalid|error|empty|malformed|corrupt|broken|bad" tests/tst_*converter*.cpp tests/tst_usiprotocolhandler.cpp tests/tst_csaprotocol.cpp -l
```

### Step 2: 異常系カテゴリの洗い出し

1. **欠損系**: 必須フィールドの欠落、空入力
2. **不正手系**: 存在しない駒の移動、二歩、打ち歩詰め等
3. **文字コード/フォーマット系**: 不正なエンコーディング、改行混在
4. **境界値系**: 最大手数、盤面外座標、ゼロ除算的な値
5. **プロトコル系**: 不正なUSI/CSAコマンド、順序違反

## 実装手順

### Step 3: テーブル駆動テストの追加

Qt Testの `_data()` パターンを使用:

```cpp
void TestClass::testInvalidInput_data()
{
    QTest::addColumn<QString>("input");
    QTest::addColumn<QString>("expectedError");

    QTest::newRow("empty input") << "" << "Empty input";
    QTest::newRow("missing header") << "..." << "Missing header";
    // ... 30件以上
}

void TestClass::testInvalidInput()
{
    QFETCH(QString, input);
    QFETCH(QString, expectedError);
    // テスト実装
}
```

### Step 4: 異常系テストの配分（最低30件）

| カテゴリ | テスト数 | 対象テストファイル |
|---|---:|---|
| 棋譜変換 - 欠損系 | 6 | tst_*converter |
| 棋譜変換 - 不正手系 | 6 | tst_*converter |
| 棋譜変換 - フォーマット系 | 4 | tst_*converter, tst_parsecommon |
| 棋譜変換 - 境界値系 | 4 | tst_*converter |
| USIプロトコル - 不正コマンド | 5 | tst_usiprotocolhandler |
| CSAプロトコル - 不正メッセージ | 5 | tst_csaprotocol |

### Step 5: 各テストファイルに追加

既存のテストファイルに `_data()` パターンで異常系テストを追加。
新しいテストファイルは作成せず、既存ファイルを拡張する。

### Step 6: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure

# 個別テスト確認
ctest --test-dir build -R converter --output-on-failure
ctest --test-dir build -R usiprotocol --output-on-failure
ctest --test-dir build -R csaprotocol --output-on-failure
```

## 完了条件

- [ ] 異常系テストが最低30件追加されている
- [ ] テーブル駆動形式（`_data()` パターン）で実装されている
- [ ] 全テスト通過
- [ ] 各異常系カテゴリ（欠損/不正手/フォーマット/境界値/プロトコル）がカバーされている

## KPI変化目標

- 仕様境界の異常系ケース: +30件以上

## 注意事項

- 異常系テストでクラッシュしないこと（適切なエラーハンドリングの確認）
- テストデータは可読性を重視し、各ケースにわかりやすい名前をつける
- 既存テストの構造を壊さない（追加のみ）
- Qt Testの `_data()` / `QFETCH` パターンに従う
