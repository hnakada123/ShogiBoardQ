# Task 20260301-mt-08: 棋譜保存/エクスポート/画像出力のバックグラウンド化（P2）

## 背景

棋譜の保存・エクスポートと盤面画像の出力がメインスレッドで同期実行されている。大量の手数（10,000行以上）の棋譜を保存する場合や、高解像度の画像出力で数秒のUI凍結が発生する。

### 対象ファイル

| ファイル | 行 | メソッド | 処理内容 |
|---------|-----|---------|---------|
| `src/kifu/kifuexportcontroller.cpp` | 167-172 | 保存前の複数形式生成 | KIF/CSA/SFEN変換 |
| `src/kifu/kifuioservice.cpp` | 112, 119 | `writeKifuFile()` | 行ごとの同期書き込み + Shift-JIS |
| `src/board/boardimageexporter.cpp` | 34-90 | `saveImage()` | `grab()` + エンコーディング + 書き込み |
| `src/board/boardimageexporter.cpp` | 29-32 | `copyToClipboard()` | `grab()` + クリップボード |

## 作業内容

### Step 1: 棋譜保存の非同期化

#### 1a: KifuIoService に非同期メソッドを追加

`src/kifu/kifuioservice.h/.cpp` を修正:

```cpp
// 非同期書き込み
void writeKifuFileAsync(const QString& filePath,
                         const QStringList& kifuLines,
                         bool useShiftJis);

signals:
    void writeCompleted(const QString& filePath, bool success);
    void writeError(const QString& filePath, const QString& errorMessage);
```

**実装**:
```cpp
void KifuIoService::writeKifuFileAsync(const QString& filePath,
                                        const QStringList& kifuLines,
                                        bool useShiftJis)
{
    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished,
            this, &KifuIoService::onWriteFinished);

    QFuture<bool> future = QtConcurrent::run(
        &KifuIoService::writeKifuFileImpl, filePath, kifuLines, useShiftJis);
    watcher->setFuture(future);
}
```

- `writeKifuFileImpl` は static メソッドとして実装（UI非依存の純粋なファイルI/O）
- `QFile`, `QStringEncoder` はスレッドセーフに使用可能
- 書き込み完了のコールバックで `writeCompleted` シグナルを発行

#### 1b: KifuExportController の非同期対応

`src/kifu/kifuexportcontroller.cpp` を修正:

- 棋譜文字列の生成（KIF/CSA/SFEN変換）もバックグラウンド化
- 変換結果を値で受け取り、`writeKifuFileAsync` に渡す
- 保存中はステータスバーに「保存中...」表示

#### 1c: 自動保存の考慮

対局中の自動保存がある場合:
- 書き込み中に同じファイルへの読み込みが発生しないよう、`QLockFile` を検討
- 書き込みが完了するまで次の自動保存をスキップ

### Step 2: 盤面画像出力の非同期化

`src/board/boardimageexporter.cpp` を修正:

```cpp
void BoardImageExporter::saveImageAsync(QWidget* boardWidget,
                                         const QString& filePath,
                                         const QString& format)
{
    // Step 1: メインスレッドで画像キャプチャ（QWidget::grab はメインスレッド限定）
    QImage image = boardWidget->grab().toImage();

    // Step 2: エンコーディング + 書き込みをワーカースレッドで実行
    auto* watcher = new QFutureWatcher<bool>(this);
    connect(watcher, &QFutureWatcher<bool>::finished,
            this, &BoardImageExporter::onImageSaveFinished);

    QFuture<bool> future = QtConcurrent::run(
        &BoardImageExporter::saveImageImpl, image, filePath, format);
    watcher->setFuture(future);
}
```

**`saveImageImpl` (static)**:
```cpp
bool BoardImageExporter::saveImageImpl(const QImage& image,
                                        const QString& filePath,
                                        const QString& format)
{
    QImageWriter writer(filePath, format.toLatin1());
    return writer.write(image);
}
```

**注意**:
- `QWidget::grab()` はメインスレッドでのみ呼び出せる（Qt の制約）
- `QImage` は暗黙的共有（implicit sharing）なのでコピーは軽量
- PNG圧縮が最もCPU集約的（高解像度で1〜3秒）

### Step 3: クリップボードコピーの最適化

`copyToClipboard()` はクリップボード操作がメインスレッド限定のため、本体は同期のまま維持。
ただし `grab()` → `QImage` → `QPixmap` の変換が重い場合は、`grab()` の結果を直接 `QPixmap` として使う。

### Step 4: 保存完了のUI通知

- ステータスバーに「保存完了」メッセージを表示（2秒後に消去）
- エラー時はエラーダイアログを表示
- 保存中にアプリケーションを終了しようとした場合は、保存完了を待ってから終了

### Step 5: ビルド・テスト確認

```bash
cmake --build build
cd build && ctest --output-on-failure
```

**手動テスト項目**:
1. KIF/CSA形式で棋譜を保存できること
2. 保存中にUI操作が可能なこと
3. 盤面画像の保存（PNG/JPG）が完了すること
4. クリップボードへのコピーが動作すること
5. 上書き保存が動作すること
6. 保存エラー時にメッセージが表示されること

## 完了条件

- [ ] 棋譜保存が `QtConcurrent::run` でバックグラウンド実行される
- [ ] 画像エンコーディング + 書き込みがバックグラウンド実行される
- [ ] `grab()` はメインスレッドで実行される
- [ ] 保存完了/エラーのUI通知がある
- [ ] 全テスト pass
