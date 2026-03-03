# Task 20260303-02: closeEvent() の shutdown 呼び出し順序修正

## 概要
`MainWindow::closeEvent()` で `QMainWindow::closeEvent(e)` より先に `runShutdown()` を実行しているのを、イベント受理後に変更する。終了拒否シナリオでの状態不整合を防止する。

## 優先度
High

## 背景
- 現状: `runShutdown()` → `QMainWindow::closeEvent(e)` の順
- 問題: イベント受理前に内部状態が shutdown 済みになるため、終了拒否（`e->ignore()`）シナリオで状態不整合が起きうる
- 既存の二重実行防止ガード (`m_shutdownDone`) は維持する

## 対象ファイル

### 修正対象
1. `src/app/mainwindow.cpp` - `closeEvent()` の順序変更

### 参照（変更不要だが理解が必要）
- `src/app/mainwindowlifecyclepipeline.h/.cpp` - `runShutdown()` と `m_shutdownDone` の実装確認
- `src/app/mainwindow.h` - `closeEvent` 宣言確認

## 実装手順

### Step 1: closeEvent() の順序を変更

`src/app/mainwindow.cpp` の `closeEvent()` を以下のように変更:

**Before (126-131行目):**
```cpp
// `closeEvent`: 終了時に設定保存とエンジン停止を行ってから親クラス処理へ委譲する。
void MainWindow::closeEvent(QCloseEvent* e)
{
    m_pipeline->runShutdown();
    QMainWindow::closeEvent(e);
}
```

**After:**
```cpp
// `closeEvent`: 親クラスでイベント受理を確認してから shutdown を実行する。
void MainWindow::closeEvent(QCloseEvent* e)
{
    QMainWindow::closeEvent(e);
    if (!e->isAccepted())
        return;
    m_pipeline->runShutdown();
}
```

### Step 2: shutdownAndQuit() / saveSettingsAndClose() との二重実行確認

`src/app/mainwindowlifecyclepipeline.cpp` の `runShutdown()` 冒頭に二重実行ガード (`m_shutdownDone`) が存在することを確認する。既にある場合は追加不要。

```cpp
void MainWindowLifecyclePipeline::runShutdown()
{
    if (m_shutdownDone) return;
    m_shutdownDone = true;
    // ...
}
```

これにより `shutdownAndQuit()` → `closeEvent()` の経路でも安全に動作する。

### Step 3: ビルド確認
```bash
cmake --build build -j4
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- `closeEvent()` が `QMainWindow::closeEvent(e)` を先に呼び、受理後に `runShutdown()` を実行する
- `shutdownAndQuit()` 経由の終了パスも引き続き正常に動作する
- ビルド成功（warning なし）
- 既存テスト全件成功

## 注意事項
- `QMainWindow::closeEvent()` はデフォルトで `e->accept()` するため、通常の終了フローでは動作変化なし
- 将来「終了確認ダイアログ」を追加する場合にこの順序が重要になる
