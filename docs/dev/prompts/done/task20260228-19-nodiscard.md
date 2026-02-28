# Task 19: `[[nodiscard]]` 一括追加（Phase 1: ガードレール強化）

## 目的

戻り値を持つ主要関数に `[[nodiscard]]` 属性を追加し、戻り値の無視をコンパイラ警告で防止する。

## 背景

- 現在 `[[nodiscard]]` は1件のみ（`usi.cpp`）
- `bool` を返すファイルI/O関数や変換関数の戻り値が無視されると、サイレント失敗の原因になる
- C++17の標準機能であり、追加コストは極めて低い
- 包括的改善分析 §5.2.1, §6.3, §12 P1

## 対象カテゴリ

### カテゴリ1: ファイルI/O関数（`bool` 戻り値）

```bash
# 対象の洗い出し
rg "^\s*bool\s+\w+" src/kifu/kifuioservice.h src/kifu/kifufilecontroller.h --no-line-number
```

主な候補:
- `KifuIOService::writeKifuFile()`
- `KifuIOService::saveKifuToFile()`
- `KifuFileController` の保存系メソッド

### カテゴリ2: バリデーション関数

```bash
rg "^\s*bool\s+\w+" src/core/movevalidator.h --no-line-number
```

- `MoveValidator` の全検証関数

### カテゴリ3: パース・変換関数

```bash
rg "^\s*bool\s+\w+|^\s*static\s+bool\s+\w+" src/kifu/formats/*.h --no-line-number
```

- 全形式コンバータの `parse()` / `convert()` / `parseWithVariations()` 系
- `parsecommon.h` のパースユーティリティ
- `shogiboard.h` の `parseSfen()` 等

### カテゴリ4: その他のエラー報告関数

```bash
rg "^\s*bool\s+\w+" src/engine/usiprotocolhandler.h src/network/csaclient.h --no-line-number
```

## 事前調査

### Step 1: 対象関数の完全リスト作成

```bash
# bool を返す public 関数の一覧
rg "^\s*(static\s+)?bool\s+\w+\(" src/**/*.h | grep -v "private:" | grep -v "//" | sort
```

### Step 2: 戻り値が実際に無視されている箇所の特定

```bash
# 関数名を使って、戻り値を捨てている呼び出しを探す
# 例: writeKifuFile(...); のように変数に受けていない箇所
```

## 実装手順

### Step 3: ヘッダファイルへの `[[nodiscard]]` 追加

対象ヘッダの関数宣言に `[[nodiscard]]` を追加する:

```cpp
// Before
bool writeKifuFile(const QString& filePath, ...);
static bool parseWithVariations(const QString& path, ...);

// After
[[nodiscard]] bool writeKifuFile(const QString& filePath, ...);
[[nodiscard]] static bool parseWithVariations(const QString& path, ...);
```

追加対象: 20〜30関数（調査結果により変動）

### Step 4: コンパイラ警告への対応

`[[nodiscard]]` 追加後にビルドし、新たに発生する警告を修正する:

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build 2>&1 | grep "nodiscard"
```

戻り値を意図的に無視している箇所は以下で対応:
1. 戻り値を変数に受けてチェックする（推奨）
2. やむを得ない場合は `(void)` キャストで明示的に無視

### Step 5: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] 主要関数（20〜30件）に `[[nodiscard]]` が追加されている
- [ ] ビルド時に新規警告が出ない（戻り値の無視箇所を修正済み）
- [ ] 全テスト通過
- [ ] 追加対象が以下のカテゴリを網羅: ファイルI/O, バリデーション, パース/変換, エラー報告

## KPI変化目標

- `[[nodiscard]]` 使用数: 1 → 20以上

## 注意事項

- `void` 戻り値の関数には追加しない
- `bool` 以外でも `std::optional` や Result 型を返す関数は対象
- Qt の `override` 関数（`event()` 等）には追加しない（基底クラスのシグネチャに従う）
- シグナルやスロット宣言には追加しない
- 既存テストが全て通過することを確認する
