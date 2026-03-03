# Task 20260303-06: USEN デコード失敗時の扱いを検知可能な失敗へ変更

## 概要
USEN デコード失敗時に `?1` などのプレースホルダを入れて処理継続しているのを、エラー情報を返す厳密 API に変更する。異常データの正常経路への混入を防ぐ。

## 優先度
Medium

## 背景
- 現状: デコード失敗時にプレースホルダ文字列（`?1`, `?2` 等）を moves リストに追加して処理継続
- 問題: 上位で失敗検知しにくく、品質低下と診断困難を招く
- 既存公開 API との互換性は維持しつつ、内部で厳密な結果を扱えるようにする

## 対象ファイル

### 修正対象
1. `src/kifu/formats/usentosfenconverter.h` - `DecodeResult` 構造体と strict API 宣言追加
2. `src/kifu/formats/usentosfenconverter_decode.cpp` - strict API 実装、既存 API をラッパに変更

### 参照（変更不要だが理解が必要）
- `src/kifu/formats/usentosfenconverter.cpp` - `parseWithVariations()` / `convertFile()` の呼び出し確認
- `tests/tst_usenconverter.cpp` - 既存テスト確認

### テスト修正
3. `tests/tst_usenconverter.cpp` - 不正入力テスト追加

## 実装手順

### Step 1: DecodeResult 構造体を定義

`src/kifu/formats/usentosfenconverter.h` に構造体を追加:

```cpp
struct UsenDecodeResult {
    QStringList moves;       // デコードされた USI 手順
    int invalidCount = 0;    // デコード失敗した手数
    QString firstError;      // 最初のエラー内容（失敗がなければ空）
};
```

クラス宣言のスコープ外（ヘッダのトップレベル）か、`UsenToSfenConverter` クラス内の public に配置する。MOC の制約に注意（signals/slots のパラメータには使わないため問題なし）。

### Step 2: 厳密 API を追加

`src/kifu/formats/usentosfenconverter.h` に宣言:

```cpp
static UsenDecodeResult decodeUsenMovesStrict(const QString& usenStr, QString* terminalOut = nullptr);
```

### Step 3: 既存 decodeUsenMoves を strict のラッパに変更

`src/kifu/formats/usentosfenconverter_decode.cpp` で:

1. 現在の `decodeUsenMoves()` の実装を `decodeUsenMovesStrict()` にリネーム
2. プレースホルダ追加部分を `invalidCount` / `firstError` のセットに変更
3. 新しい `decodeUsenMoves()` は `decodeUsenMovesStrict()` のラッパとして実装

**decodeUsenMovesStrict 内のエラー処理部分:**

```cpp
} else {
    // デコードできなかった場合はエラー情報を記録しプレースホルダを追加
    const QString placeholder = QStringLiteral("?%1").arg(moveCount + 1);
    qCDebug(lcKifu) << "Move" << (moveCount + 1) << "'" << threeChars
             << "' could not be decoded, using placeholder";
    usiMoves.append(placeholder);
    ++result.invalidCount;
    if (result.firstError.isEmpty()) {
        result.firstError = QStringLiteral("Move %1 '%2' could not be decoded")
            .arg(moveCount + 1).arg(threeChars);
    }
}
```

**既存 API のラッパ:**

```cpp
QStringList UsenToSfenConverter::decodeUsenMoves(const QString& usenStr, QString* terminalOut)
{
    UsenDecodeResult result = decodeUsenMovesStrict(usenStr, terminalOut);
    return result.moves;
}
```

### Step 4: 上位呼び出し元を確認

`parseWithVariations()` や `convertFile()` で `decodeUsenMoves()` を呼んでいる箇所を確認し、必要に応じて `decodeUsenMovesStrict()` への切り替えを検討する。ただし、この Task では既存 API の互換維持を優先し、上位の切り替えは行わない（将来タスク）。

### Step 5: テスト追加

`tests/tst_usenconverter.cpp` に不正入力テストを追加:

```cpp
void TestUsenConverter::testDecodeUsenMovesStrict_invalidInput()
{
    // 不正な3文字シーケンスを含む USEN 文字列
    const QString invalidUsen = QStringLiteral("!!!7g77f7");  // 最初の3文字が不正
    UsenDecodeResult result = UsenToSfenConverter::decodeUsenMovesStrict(invalidUsen);
    QVERIFY(result.invalidCount > 0);
    QVERIFY(!result.firstError.isEmpty());
    // moves にはプレースホルダが含まれる
    QVERIFY(result.moves.contains(QStringLiteral("?1")));
}

void TestUsenConverter::testDecodeUsenMovesStrict_validInput()
{
    // 正常な USEN 文字列（既存テストから流用）
    const QString validUsen = QStringLiteral("7g77f7");  // 実際の有効値に置換
    UsenDecodeResult result = UsenToSfenConverter::decodeUsenMovesStrict(validUsen);
    QCOMPARE(result.invalidCount, 0);
    QVERIFY(result.firstError.isEmpty());
}
```

注意: テストで使う USEN 文字列は実際のエンコード仕様を確認して有効/無効な値を設定すること。既存テストケースの入力値を参考にする。

### Step 6: ビルド・テスト実行
```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- `decodeUsenMovesStrict()` が `invalidCount` と `firstError` を返す
- 既存の `decodeUsenMoves()` は互換維持（戻り値は `QStringList` のまま）
- 不正入力時にエラー情報が取れるテストが追加される
- ビルド成功（warning なし）
- 既存テスト全件成功
