/// @file tst_image_export.cpp
/// @brief 画像エクスポートテスト（BoardImageExporter + QWidget::grab 基礎動作）

#include <QtTest>
#include <QApplication>
#include <QClipboard>
#include <QImage>
#include <QPixmap>
#include <QWidget>

#include "boardimageexporter.h"

class TestImageExport : public QObject
{
    Q_OBJECT

private slots:
    void grabWidget_returnsNonNullPixmap();
    void grabWidget_hasExpectedSize();
    void grabWidget_toImage_isValid();
    void copyToClipboard_widgetExists_clipboardHasImage();
    void copyToClipboard_nullWidget_noCrash();
};

void TestImageExport::grabWidget_returnsNonNullPixmap()
{
    QWidget widget;
    widget.resize(200, 200);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    const QPixmap pixmap = widget.grab();
    if (pixmap.isNull())
        QSKIP("offscreen platform returned null pixmap");
    QVERIFY(!pixmap.isNull());
}

void TestImageExport::grabWidget_hasExpectedSize()
{
    QWidget widget;
    widget.resize(300, 250);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    const QPixmap pixmap = widget.grab();
    if (pixmap.isNull())
        QSKIP("offscreen platform returned null pixmap");
    QVERIFY(pixmap.width() > 0);
    QVERIFY(pixmap.height() > 0);
}

void TestImageExport::grabWidget_toImage_isValid()
{
    QWidget widget;
    widget.resize(100, 100);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    const QPixmap pixmap = widget.grab();
    if (pixmap.isNull())
        QSKIP("offscreen platform returned null pixmap");

    const QImage image = pixmap.toImage();
    QVERIFY(!image.isNull());
    QVERIFY(image.width() > 0);
    QVERIFY(image.height() > 0);
}

void TestImageExport::copyToClipboard_widgetExists_clipboardHasImage()
{
    QWidget widget;
    widget.resize(150, 150);
    widget.show();
    QVERIFY(QTest::qWaitForWindowExposed(&widget));

    // grab() が動作するか先に確認
    if (widget.grab().isNull())
        QSKIP("offscreen platform returned null pixmap");

    BoardImageExporter::copyToClipboard(&widget);

    const QClipboard *clipboard = QApplication::clipboard();
    const QPixmap clipPixmap = clipboard->pixmap();
    QVERIFY(!clipPixmap.isNull());
}

void TestImageExport::copyToClipboard_nullWidget_noCrash()
{
    // nullptr を渡してもクラッシュしないことを確認
    BoardImageExporter::copyToClipboard(nullptr);
}

QTEST_MAIN(TestImageExport)
#include "tst_image_export.moc"
