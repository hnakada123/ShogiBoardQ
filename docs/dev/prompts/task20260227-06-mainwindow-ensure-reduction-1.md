# Task 06: MainWindow ensure* メソッド削減の実装（第1弾: 転送メソッドの削除）

## 目的
Task 05 の分析結果に基づき、単純転送のみの `ensure*` メソッドを MainWindow から削除する。

## 前提
- Task 05 の分析ドキュメントが完成していること
- Task 04 の friend 削減が完了していること

## 作業内容

### Phase A: 単純転送 ensure* の洗い出し
`mainwindow.cpp` で以下のパターンのメソッドを抽出する:
```cpp
void MainWindow::ensureXxx()
{
    m_registry->ensureXxx();
}
```

### Phase B: 呼び出し元の書き換え
各転送メソッドの呼び出し元を以下のように変更する:

1. **MainWindow 内からの呼び出し**: `m_registry->ensureXxx()` に直接置換
2. **friend class からの呼び出し**: `w->m_registry->ensureXxx()` に直接置換
3. **connect() のターゲット**: ラッパースロットとして残すか、std::bind で直接接続に変更

### Phase C: mainwindow.h からの宣言削除
- 不要になった `void ensureXxx();` 宣言を削除する
- コメントも合わせて削除する

### Phase D: テスト実行
```bash
cmake --build build
cd build && ctest --output-on-failure
```

## 完了条件
- [ ] 単純転送の ensure* が mainwindow.h から削除されている
- [ ] ensure* 宣言数が 20以下に減少している（目標: 第1弾で半減）
- [ ] 全テストが PASS する
- [ ] 構造KPIテストの ensure 上限値を更新する

## 注意事項
- `connect()` のターゲットになっている ensure* は慎重に扱う。`connect(sender, &Sender::sig, this, &MainWindow::ensureXxx)` の場合、this を変更する必要がある
- 1メソッドずつ削除→ビルド確認のサイクルで進める
- 追加ロジックがある ensure*（転送以外の処理を含む）は第2弾で対応する
