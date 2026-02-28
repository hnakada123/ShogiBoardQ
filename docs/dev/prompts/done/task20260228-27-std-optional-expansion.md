# Task 27: `std::optional` 拡大（Phase 3: C++17機能活用）

## 目的

`bool` + out-parameter パターンを `std::optional` に置換し、関数のインターフェースを型安全にする。

## 背景

- 現在 `std::optional` は14箇所で使用（採用率: 低〜中）
- `bool` 戻り値 + `QString*` out-param パターンが変換器やI/O関数に多数存在
- `std::optional` により「値があるかないか」を型で表現でき、誤用を防止
- 包括的改善分析 §5.2.2, §12 P2

## 対象カテゴリ

### カテゴリ1: ShogiUtils のパース関数

```bash
rg "bool\s+parse" src/core/shogiutils.h
```

候補:
```cpp
// Before
bool parseMoveLabel(const QString& moveLabel, int* outFile, int* outRank);

// After
std::optional<std::pair<int, int>> parseMoveLabel(const QString& moveLabel);
```

### カテゴリ2: 変換器群のパース関数

```bash
rg "static\s+bool\s+parse" src/kifu/formats/*.h
```

候補（6ファイル、計~15箇所）:
```cpp
// Before
static bool parseWithVariations(const QString& path, KifParseResult& out,
                                 QString* errorMessage = nullptr);

// After
struct ParseResult { KifParseResult result; QString error; };
static std::optional<ParseResult> parseWithVariations(const QString& path);
```

### カテゴリ3: その他の候補

```bash
rg "bool\s+\w+\(.*\*\s*\w+\s*(=\s*nullptr)?\)" src/**/*.h | head -20
```

## 事前調査

### Step 1: 全候補の洗い出し

```bash
# bool + out-param パターンの検出
rg "bool\s+\w+\([^)]*\*\s*\w+" src/**/*.h | grep -v "const.*\*"
```

### Step 2: 各候補の呼び出し箇所の確認

```bash
# parseMoveLabel の使用箇所
rg "parseMoveLabel" src --type cpp -n
```

### Step 3: 影響範囲の評価

各候補について:
- 呼び出し箇所の数
- テストの有無
- 他の public API への影響

## 実装手順

### Step 4: 低リスク候補から着手

影響範囲が小さい（呼び出し箇所が少ない）関数から変更する。

### Step 5: ヘッダの変更

```cpp
// Before (shogiutils.h)
bool parseMoveLabel(const QString& moveLabel, int* outFile, int* outRank);

// After
std::optional<std::pair<int, int>> parseMoveLabel(const QString& moveLabel);
```

### Step 6: 実装の変更

```cpp
// Before
bool ShogiUtils::parseMoveLabel(const QString& moveLabel, int* outFile, int* outRank)
{
    // ... parsing logic ...
    if (success) {
        *outFile = file;
        *outRank = rank;
        return true;
    }
    return false;
}

// After
std::optional<std::pair<int, int>> ShogiUtils::parseMoveLabel(const QString& moveLabel)
{
    // ... parsing logic ...
    if (success) {
        return std::make_pair(file, rank);
    }
    return std::nullopt;
}
```

### Step 7: 呼び出し元の更新

```cpp
// Before
int file, rank;
if (ShogiUtils::parseMoveLabel(label, &file, &rank)) {
    // use file, rank
}

// After
if (auto result = ShogiUtils::parseMoveLabel(label)) {
    auto [file, rank] = *result;
    // use file, rank
}
```

### Step 8: 変換器群の変更（段階的）

全変換器を一度に変更するのではなく、1-2ファイルずつ変更してビルド・テストを確認。

### Step 9: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] `std::optional` に置換された関数が5件以上
- [ ] 呼び出し元が全て更新されている
- [ ] 全テスト通過
- [ ] 新たなコンパイラ警告が出ない

## KPI変化目標

- `std::optional` 使用数: 14 → 20以上

## 注意事項

- 一度に大量の関数を変更しない（1-2ファイルずつ段階的に進める）
- テストのある関数を優先的に変更する（回帰を即座に検知できる）
- `[[nodiscard]]` も合わせて追加する（Task 19 と連携）
- 変換器の public API を変更する場合は、関連テストの更新も必要
- `std::optional` の `.value()` は例外を投げるため、`.has_value()` + `*` を使う
- 構造化束縛 `auto [a, b] = *result;` を積極的に活用する
