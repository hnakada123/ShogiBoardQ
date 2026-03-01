# Task 20260301-01: Lambda Connect 複数行検知漏れ修正（P0）

## 背景

`tst_structural_kpi.cpp` の `lambdaConnectCount` テストは、`connect(` と `[` が**同一行**にある場合のみ検出する。
実際には複数行にまたがる lambda connect が存在しており、KPIが pass してしまっている。

### 検知漏れの実例

```cpp
// src/app/kifusubregistry.cpp:144-147（connect( と [this] が別行）
connect(m_mw.m_kifuExportController.get(), &KifuExportController::statusMessage,
        &m_mw, [this](const QString& msg, int timeout) {
            if (m_mw.ui && m_mw.ui->statusbar) {
                m_mw.ui->statusbar->showMessage(msg, timeout);

// src/widgets/recordpane.cpp:262-265（connect( と [this] が別行）
m_connRowsInserted = connect(model, &QAbstractItemModel::rowsInserted,
                             m_kifu, [this](const QModelIndex&, int, int) {
                                 m_kifu->scrollToBottom();
                             });

// src/widgets/recordpane.cpp:304-307（connect( と [brModel] が別行）
m_connBranchCurrentRow = connect(sel, &QItemSelectionModel::currentRowChanged,
        this, [brModel](const QModelIndex& current, const QModelIndex&) {
            if (brModel && current.isValid()) {
                brModel->setCurrentHighlightRow(current.row());
```

## 作業内容

### Step 1: 検知ロジックの改修

`tests/tst_structural_kpi.cpp` の `lambdaConnectCount()` メソッド（317行目付近）を修正する。

**現在のロジック**（行単位マッチ）:
```cpp
if (connectPattern.match(line).hasMatch()
    && lambdaPattern.match(line).hasMatch()) {
    ++fileCount;
}
```

**改修後のロジック**（複数行バッファ方式）:
- `connect(` を含む行を見つけたら、そこから `)` のネスト深度が 0 になるまで（= `connect(...)` の閉じ括弧まで）行をバッファに蓄積する
- バッファ全体に `[` が含まれるかチェック
- コメント行（`//`）は除外済みのまま維持
- ただし `disconnect(` は除外すること（既存の `connect(` パターンを `(?<!dis)connect\s*\(` に変更）

**実装ヒント**:
```cpp
// connect( 検出後のバッファリング
int parenDepth = 0;
QString buffer;
bool inConnect = false;

while (!in.atEnd()) {
    const QString line = in.readLine();
    if (commentPattern.match(line).hasMatch())
        continue;

    if (!inConnect && connectPattern.match(line).hasMatch()) {
        inConnect = true;
        buffer.clear();
    }

    if (inConnect) {
        buffer += line + '\n';
        for (QChar ch : line) {
            if (ch == '(') ++parenDepth;
            if (ch == ')') --parenDepth;
        }
        if (parenDepth <= 0) {
            // connect(...) 全体がバッファに入った
            if (lambdaPattern.match(buffer).hasMatch()) {
                ++fileCount;
            }
            inConnect = false;
            parenDepth = 0;
        }
    }
}
```

### Step 2: しきい値の更新

検知ロジック修正後、実際の lambda connect 数を測定し、`kMaxLambdaConnects` を実測値に更新する。

```cpp
const int kMaxLambdaConnects = <実測値>;  // 実測値: N件
```

### Step 3: 例外箇所の文書化

検出された lambda connect の各箇所について、以下を判定し文書化する:

1. **名前付きスロットに置換可能** → 置換する（CLAUDE.md ルール遵守）
2. **置換困難（QMetaObject::Connection 返却が必要等）** → 例外として登録

`tst_structural_kpi.cpp` のテスト冒頭コメントに例外理由を記載する:
```cpp
// Lambda connect exceptions (理由付き):
//   src/widgets/recordpane.cpp: QMetaObject::Connection 返却が必要なため
//   src/app/kifusubregistry.cpp: ステータスバー転送、UIメンバ直接アクセスのため
```

### Step 4: 可能な置換の実施

名前付きスロットに置換可能な箇所は、この段階で修正する。

- `connect(sender, &Signal, receiver, [capture](...) { body })` を
- `connect(sender, &Signal, receiver, &Receiver::newSlot)` に変更
- 新スロットメソッドを対応するクラスに追加

### Step 5: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure -R tst_structural_kpi
```

- `lambdaConnectCount` テストが pass すること
- 検出件数と `kMaxLambdaConnects` が一致すること
- 他のKPIテストが全て pass すること

## 完了条件

- [x] 複数行 lambda connect を `tst_structural_kpi` が検出できる
- [x] `disconnect(` が誤検出されない
- [x] 現在残る lambda connect の扱い（置換 or 例外登録 + 理由）が文書化される
- [x] 全テスト pass
