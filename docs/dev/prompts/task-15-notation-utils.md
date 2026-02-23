# Task 15: 棋譜変換の共通ユーティリティ化

`src/kifu/formats/` 配下の変換器に散在する共通ロジックを抽出してください。

## 背景

KIF/KI2/CSA/USI/USEN 変換器で、座標変換・数字変換・駒表現変換の類似コードが重複している。バグ修正の水平展開漏れが起きやすい。

## 作業内容

### Step 1: 重複パターンの特定

各変換器を読み、以下の重複パターンを特定:

1. **座標文字 ↔ 数値変換**: 列番号 ↔ '1'-'9', 段番号 ↔ 'a'-'i' or '一'-'九'
2. **駒文字変換**: SFEN駒文字 ↔ 各フォーマット固有の駒文字
3. **SFEN 文字列構築**: 指し手を SFEN 形式にフォーマット
4. **全角・半角変換**: 全角数字 ↔ 半角数字

### Step 2: 共通ユーティリティの作成

`src/kifu/formats/notationutils.h/.cpp`（新規）:

```cpp
namespace NotationUtils {

// USI 座標変換
std::optional<int> usiFileToInt(QChar ch);  // '1'-'9' → 1-9
std::optional<int> usiRankToInt(QChar ch);  // 'a'-'i' → 1-9
QChar intToUsiFile(int file);               // 1-9 → '1'-'9'
QChar intToUsiRank(int rank);               // 1-9 → 'a'-'i'

// 漢数字変換（KIF/KI2 用）
std::optional<int> kanjiDigitToInt(QChar ch);  // '一'〜'九' → 1-9
std::optional<int> zenkakuDigitToInt(QChar ch); // '１'〜'９' → 1-9

// SFEN 指し手フォーマット
QString formatSfenMove(int fromFile, int fromRank, int toFile, int toRank, bool promote);
QString formatSfenDrop(Piece piece, int toFile, int toRank);

} // namespace NotationUtils
```

### Step 3: 各変換器の移行

各変換器のローカル実装を `NotationUtils` の呼び出しに置換。一度に全変換器を変更するのではなく、1つずつ確認しながら移行する。

### Step 4: テスト追加

`tests/tst_coredatastructures.cpp` またはに `NotationUtils` のテストを追加:
- 各変換関数のラウンドトリップ
- 境界値（1, 9, 範囲外）
- 無効入力（std::nullopt 返却の確認）

## 制約

- CLAUDE.md のコードスタイルに準拠
- 全既存テストが通ること
- 既存の変換結果が変わらないこと
