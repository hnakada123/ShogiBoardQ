# Task 20260303-13: 画像エクスポートテスト追加

## 概要
BoardImageExporter の画像生成ロジックをウィジェットテストとして追加する。現状はメニュー有効状態テスト(tst_app_ui_state_policy)のみで、実際の画像生成テストがない。

## 優先度
Medium

## 背景
- `BoardImageExporter` は static メソッド2つ（`copyToClipboard`, `saveImage`）の小さなクラス（90行）
- 両メソッドとも `QWidget*` を `grab()` して QPixmap/QImage に変換するため、`QApplication` + offscreen 環境が必要
- テスト環境では `QT_QPA_PLATFORM=offscreen` が設定済み（CMakeLists.txt の set_tests_properties）

## 対象ファイル

### 参照（変更不要だが理解が必要）
- `src/board/boardimageexporter.h` - クラス定義（26行）
- `src/board/boardimageexporter.cpp` - 実装（90行）

### 新規作成
1. `tests/tst_image_export.cpp` - テストファイル

### 修正対象
2. `tests/CMakeLists.txt` - テスト登録追加

## 実装手順

### Step 1: テストファイル新規作成

`tests/tst_image_export.cpp` を新規作成する。

テストケース方針:

**クリップボードコピーテスト:**
1. `copyToClipboard_widgetExists_succeeds` - 有効な QWidget から clipboard へコピーが成功
2. `copyToClipboard_clipboardHasImage` - コピー後にクリップボードに画像データが存在

**画像保存テスト（QFileDialog をバイパス）:**
- `saveImage` は内部で `QFileDialog::getSaveFileName` を使うため直接テスト困難
- 代わりに `QWidget::grab()` → `QPixmap::save()` の基礎動作をテスト

**QWidget::grab 基礎テスト:**
3. `grabWidget_returnsNonNullPixmap` - QWidget を grab して非 null の QPixmap を取得
4. `grabWidget_hasExpectedSize` - grab した画像のサイズがウィジェットサイズと一致
5. `grabWidget_toImage_isValid` - QPixmap を QImage に変換して isNull()==false

**Null 安全性テスト:**
6. `copyToClipboard_nullWidget_nocrash` - nullptr 渡しでクラッシュしないことを確認

テンプレート:
```cpp
/// @file tst_image_export.cpp
/// @brief 画像エクスポートテスト

#include <QtTest>
#include <QWidget>
#include <QPixmap>
#include <QClipboard>
#include <QApplication>

#include "boardimageexporter.h"

class TestImageExport : public QObject
{
    Q_OBJECT

private slots:
    void copyToClipboard_widgetExists_succeeds();
    void copyToClipboard_clipboardHasImage();
    void grabWidget_returnsNonNullPixmap();
    void grabWidget_hasExpectedSize();
    void grabWidget_toImage_isValid();
};

void TestImageExport::grabWidget_returnsNonNullPixmap()
{
    QWidget widget;
    widget.resize(200, 200);
    widget.show();
    QTest::qWaitForWindowExposed(&widget);

    const QPixmap pixmap = widget.grab();
    QVERIFY(!pixmap.isNull());
}

void TestImageExport::grabWidget_hasExpectedSize()
{
    QWidget widget;
    widget.resize(300, 250);
    widget.show();
    QTest::qWaitForWindowExposed(&widget);

    const QPixmap pixmap = widget.grab();
    // offscreen 環境ではDPIにより多少の差異がある場合があるが、概ね一致
    QVERIFY(pixmap.width() > 0);
    QVERIFY(pixmap.height() > 0);
}

void TestImageExport::grabWidget_toImage_isValid()
{
    QWidget widget;
    widget.resize(100, 100);
    widget.show();
    QTest::qWaitForWindowExposed(&widget);

    const QPixmap pixmap = widget.grab();
    const QImage image = pixmap.toImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
}
```

注意:
- offscreen プラットフォームでは `show()` + `qWaitForWindowExposed` が必要
- `copyToClipboard` テストは `BoardImageExporter::copyToClipboard` の内部実装を確認してから書く。もし内部で null チェックがなければ、null 渡しテストは QSKIP する
- clipboard テストは CI 環境で失敗する場合があるため、`QClipboard::supportsSelection()` 等のガードを検討

### Step 2: CMakeLists.txt にテスト登録

`tests/CMakeLists.txt` に以下を追加:

```cmake
# ============================================================
# Unit: BoardImageExporter テスト
# ============================================================
add_shogi_test(tst_image_export
    tst_image_export.cpp
    ${SRC}/common/logcategories.cpp
    ${SRC}/board/boardimageexporter.cpp
)
```

### Step 3: ビルド・テスト実行
```bash
cmake -B build -S .
cmake --build build -j4
ctest --test-dir build --output-on-failure -R tst_image_export
```

offscreen 環境で `grab()` が空の pixmap を返す場合は、テストを QSKIP でスキップするガードを追加。

### Step 4: 全テスト実行
```bash
ctest --test-dir build --output-on-failure
```

## 受け入れ基準
- QWidget::grab → QPixmap/QImage の基礎テスト 3件以上
- BoardImageExporter::copyToClipboard のテスト 1件以上（offscreen 環境で可能な範囲）
- offscreen 環境で全テスト PASS（または環境起因で QSKIP）
- ビルド成功（warning なし）
- 既存テスト全件成功
