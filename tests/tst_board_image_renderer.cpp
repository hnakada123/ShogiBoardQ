/// @file tst_board_image_renderer.cpp
/// @brief BoardImageRenderer のユニットテスト（offscreen で描画して画像を検証する）

#include <QtTest>

#include <QImage>
#include <QSet>
#include <QTemporaryDir>

#include "boardimagerenderer.h"

class TestBoardImageRenderer : public QObject
{
    Q_OBJECT

private slots:
    void rendersHirateToPng()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("board.png"));
        BoardImageRenderer::Options options;
        options.squareSize = 40;
        options.lastMoveUsi = QStringLiteral("7g7f");
        QString error;
        QSize size;
        QVERIFY2(BoardImageRenderer::renderToFile(QStringLiteral("startpos"), path, options, &error, &size), qPrintable(error));
        QVERIFY(size.width() > 40 * 9);
        QVERIFY(size.height() > 40 * 9);

        QImage image(path);
        QVERIFY(!image.isNull());
        QCOMPARE(image.size(), size);
        // 単色ではない（盤・駒が描かれている）
        QSet<QRgb> colors;
        for (int y = 0; y < image.height(); y += 7) {
            for (int x = 0; x < image.width(); x += 7) colors.insert(image.pixel(x, y));
        }
        QVERIFY(colors.size() > 8);
    }

    void flippedAndNormalDiffer()
    {
        const QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3");
        BoardImageRenderer::Options normal;
        normal.squareSize = 30;
        BoardImageRenderer::Options flipped = normal;
        flipped.flip = true;
        QString error;
        const QImage a = BoardImageRenderer::render(sfen, normal, &error);
        QVERIFY2(!a.isNull(), qPrintable(error));
        const QImage b = BoardImageRenderer::render(sfen, flipped, &error);
        QVERIFY2(!b.isNull(), qPrintable(error));
        QCOMPARE(a.size(), b.size());
        QVERIFY(a != b);
    }

    void rejectsInvalidInput()
    {
        QString error;
        QVERIFY(BoardImageRenderer::render(QStringLiteral("not a sfen"), {}, &error).isNull());
        QVERIFY(!error.isEmpty());
        BoardImageRenderer::Options tooSmall;
        tooSmall.squareSize = 5;
        QVERIFY(BoardImageRenderer::render(QStringLiteral("startpos"), tooSmall, &error).isNull());
    }
};

QTEST_MAIN(TestBoardImageRenderer)
#include "tst_board_image_renderer.moc"
