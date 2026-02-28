# Task 30: QStringView 拡大（Phase 4: C++17機能活用）

## 目的

文字列を変更しないパース関数の引数を `const QString&` から `QStringView` に置換し、コピーコスト削減とAPI意図の明確化を行う。

## 背景

- 現在 `QStringView` は20箇所で使用（低〜中の採用率）
- 文字列を読み取るだけのパース関数が `const QString&` を受け取っている
- Qt6 では `QStringView` が推奨されており、`QString` のサブストリングを追加コピーなしで渡せる
- 包括的改善分析 §5.2.4, §12 P3

## 対象カテゴリ

### カテゴリ1: parsecommon.cpp の文字解析関数群

```bash
rg "const QString&" src/kifu/formats/parsecommon.h
```

### カテゴリ2: shogiboard.cpp の SFEN パース

```bash
rg "const QString&.*sfen\|const QString&.*parse" src/core/shogiboard.h -i
```

### カテゴリ3: usiprotocolhandler.cpp のコマンドパース

```bash
rg "const QString&" src/engine/usiprotocolhandler.h | head -10
```

## 事前調査

### Step 1: 候補関数の洗い出し

```bash
# 文字列引数を受け取り、変更しない（戻り値が非void）関数
rg "const QString&" src/kifu/formats/parsecommon.h src/core/shogiboard.h src/engine/usiprotocolhandler.h
```

### Step 2: 各関数の内部実装の確認

`QStringView` に変更可能かの判断基準:
- 関数内で `QString` 固有のメソッド（`replace()`, `append()` 等）を使用していないこと
- `QStringView` で利用可能なメソッド（`mid()`, `left()`, `at()`, `indexOf()` 等）で代替可能なこと

### Step 3: 呼び出し元の影響確認

```bash
# 各関数の呼び出し箇所
rg "parseXxx\|parseSfen" src --type cpp -n
```

## 実装手順

### Step 4: parsecommon から着手

最も影響範囲が小さく、文字列解析に特化した関数群から変更:

```cpp
// Before
static int parseKanjiNumber(const QString& text, int pos);
static bool isKanjiDigit(const QString& text, int pos);

// After
static int parseKanjiNumber(QStringView text, int pos);
static bool isKanjiDigit(QStringView text, int pos);
```

### Step 5: 内部実装の調整

`QStringView` では一部の `QString` メソッドが使えないため、代替に置換:

```cpp
// QString 固有
str.toInt()  // → QStringView にも toInt() がある（Qt6）
str.mid(pos, len)  // → QStringView にも mid() がある

// 必要に応じて toString() で一時変換
view.toString()  // QStringView → QString
```

### Step 6: shogiboard のSFENパース

```cpp
// Before
void parseSfen(const QString& sfen);

// After
void parseSfen(QStringView sfen);
```

### Step 7: 呼び出し元の確認

`QStringView` は `QString` から暗黙変換されるため、呼び出し元の変更は通常不要:

```cpp
QString sfenStr = "...";
board.parseSfen(sfenStr);  // QString → QStringView の暗黙変換
```

### Step 8: ビルド・テスト

```bash
cmake -B build -S . -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## 完了条件

- [ ] 10関数以上が `QStringView` を使用するよう変更
- [ ] parsecommon, shogiboard, usiprotocolhandler の主要パース関数が対象
- [ ] 全テスト通過
- [ ] コンパイラ警告なし

## KPI変化目標

- `QStringView` 使用数: 20 → 30以上

## 注意事項

- `QStringView` は文字列の所有権を持たないため、一時オブジェクトのライフタイムに注意
- `QStringView` を返す関数は避ける（ダングリング参照の危険）
- シグナル/スロットの引数型は `QString` のまま維持する（MOC互換性）
- `QStringView` を `std::string_view` と混在させない（Qt の世界観に統一）
- 1-2ファイルずつ段階的に進め、各段階でビルド・テストを確認
