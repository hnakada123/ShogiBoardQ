# Task 26: パーサーの複雑さ低減

## 目的

KIF/KI2 パーサー等の深くネストした条件分岐を extract-method で分解し、可読性を向上させる。

## 背景

KIF/KI2 パーサーなどで正規表現と条件分岐が深くネストしている箇所がある。将棋の棋譜フォーマット自体の複雑さを考えると仕方ない面もあるが、意味のある名前のメソッドに分割することで可読性が向上する。

## 対象ファイル

- `src/kifu/formats/kiftosfenconverter.cpp`
- `src/kifu/formats/ki2tosfenconverter.cpp`
- その他、ネストが深いパーサー

## 作業

### Step 1: 複雑さの計測

対象ファイルを読み、以下を特定する:
- ネストが4段以上の箇所
- 50行以上の関数
- 複数の責務を持つ関数

### Step 2: extract-method による分解

深くネストした条件分岐を意味のある名前の private static メソッドに抽出する。

例:
```cpp
// Before: 1つの巨大関数内で全て処理
QString convertKifToSfen(const QString& kif) {
    // ... 100行の処理 ...
    // 駒の変換
    // 座標の変換
    // 成り/不成りの判定
    // ...
}

// After: 責務ごとにメソッド分割
QString convertKifToSfen(const QString& kif) {
    auto piece = parsePieceNotation(token);
    auto coord = parseCoordinate(token);
    auto promotion = parsePromotionSuffix(token);
    // ...
}
```

### Step 3: 共通ロジックの発見

KIF/KI2/CSA/USI 間で重複する以下のロジックがあれば、共通ユーティリティへの集約を検討する:
- 漢数字→算用数字変換
- 駒名の日本語↔英字変換
- 座標の表記変換

ただし、無理に共通化せず、明確に重複しているものだけを対象とする。

## 制約

- 既存の変換結果が変わらないこと（入出力の同値性を維持）。
- 既存テスト（`tst_kifconverter.cpp` 等）が全て通ること。
- 過度な抽象化は避け、可読性向上に焦点を当てる。

## 受け入れ条件

- [ ] 50行以上の関数が分解されている。
- [ ] ネスト4段以上の箇所が解消されている。
- [ ] ビルド成功、既存テストが通る。
