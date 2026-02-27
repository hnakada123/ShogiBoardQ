# Task 09: JosekiWindow QAbstractTableModel 導入

## 目的
`JosekiWindow` のテーブルデータ管理を `QTableWidget` 直操作から `QAbstractTableModel` ベースに移行する。

## 前提
- Task 08 の分析ドキュメントが完成していること

## 作業内容

### 1. JosekiTableModel クラスの作成
`src/dialogs/josekitablemodel.h/.cpp` を新規作成する。

`QAbstractTableModel` を継承し、以下を実装する:
- `rowCount()`, `columnCount()`, `data()`, `headerData()`, `flags()`
- `setData()`, `insertRows()`, `removeRows()`
- 内部データ構造（`QVector<JosekiEntry>` 等）

### 2. JosekiEntry 構造体の定義
テーブルの1行に相当するデータ構造を定義する:
```cpp
struct JosekiEntry {
    QString sfen;       // 局面SFEN
    QString move;       // 指し手
    QString comment;    // コメント
    int frequency;      // 出現頻度
    // ... Task 08 の分析に基づく
};
```

### 3. QTableWidget → QTableView 置換
- `josekiwindow.ui` の `QTableWidget` を `QTableView` に変更する
- `JosekiWindow` コンストラクタで `JosekiTableModel` を `QTableView` にセットする
- `item()->text()` 等の直接操作を Model 経由に書き換える

### 4. CMakeLists.txt 更新
新規ファイルを追加する。

### 5. テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] `JosekiTableModel` が作成され、`QTableView` で動作する
- [ ] `QTableWidget` への直接操作が Model 経由に置換されている
- [ ] 既存の `tst_josekiwindow.cpp` テストが PASS する
- [ ] 全テストが PASS する

## 注意事項
- 一度に全テーブルを置換するのではなく、メインテーブルから段階的に行う
- セル編集・選択変更のシグナルが正しく動作することを確認する
- ソート・フィルタ機能がある場合は `QSortFilterProxyModel` の導入も検討する
- `josekiwindow.cpp` の行数がこの段階で大幅に減ることは期待しない（構造変更が主目的）
