/// @file tst_board_image_renderer.cpp
/// @brief BoardImageRenderer のユニットテスト（offscreen で描画して画像を検証する）

#include <QtTest>

#include <QImage>
#include <QSet>
#include <QTemporaryDir>
#include <QPainter>
#include <QComboBox>
#include <QCheckBox>
#include <QSpinBox>
#include <QSettings>

#include "boardimagerenderer.h"
#include "boardappearance.h"
#include "boardcolordialog.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"
#include "pieceimageprovider.h"
#include "settingscommon.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "appsettings.h"

class TestBoardImageRenderer : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
    }

    void init()
    {
        SettingsCommon::openSettings().clear();
    }

    void appearanceControlsPropagateAndPersist()
    {
        ShogiView first, second;
        BoardColorDialog dialog;
        auto* themes = dialog.findChild<QComboBox*>("boardThemeCombo");
        auto* grain = dialog.findChild<QCheckBox*>("boardWoodGrain");
        auto* shadow = dialog.findChild<QCheckBox*>("boardPieceShadow");
        auto* scale = dialog.findChild<QSpinBox*>("boardPieceScale");
        QVERIFY(themes && grain && shadow && scale);
        QCOMPARE(themes->currentData().toString(), QStringLiteral("kaya"));
        for (int i = 0; i < themes->count(); ++i) {
            QVERIFY(QMetaObject::invokeMethod(themes, "activated", Q_ARG(int, i)));
            QVERIFY(first.boardColors() == BoardColorPresets::themes().at(i).colors);
            QVERIFY(second.boardColors() == first.boardColors());
            QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("wood"));
        }
        grain->setChecked(false);
        shadow->setChecked(false);
        scale->setValue(94);
        QCOMPARE(themes->currentIndex(), -1);
        QVERIFY(first.boardVisuals() == (BoardVisuals{false, false, 94}));
        QVERIFY(second.boardVisuals() == first.boardVisuals());
        SettingsCommon::openSettings().sync();
        ShogiView restored;
        BoardColorDialog restoredDialog;
        QVERIFY(restored.boardVisuals() == first.boardVisuals());
        QVERIFY(restored.boardColors() == first.boardColors());
        QCOMPARE(restoredDialog.findChild<QSpinBox*>("boardPieceScale")->value(), 94);
        QVERIFY(QMetaObject::invokeMethod(themes, "activated", Q_ARG(int, 0)));
        QVERIFY(first.boardVisuals() == BoardVisuals{});
        QCOMPARE(themes->currentData().toString(), QStringLiteral("kaya"));
        QCOMPARE(restoredDialog.findChild<QComboBox*>("boardThemeCombo")->currentIndex(), 0);
    }

    void surfaceTextureIsStableAndOptional()
    {
        auto render = [](bool grain) {
            QImage image(440, 360, QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(2);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            BoardSurfacePainter::draw(painter, QRect(10, 10, 200, 160), BoardColors{}.board, grain, 40);
            return image;
        };
        const auto wood = render(true);
        QCOMPARE(wood, render(true));
        const auto flat = render(false);
        QVERIFY(flat != wood);
        QCOMPARE(flat.pixelColor(200, 160), BoardColors{}.board);
        QSet<QRgb> colors;
        for (int x = 40; x < 400; ++x) colors.insert(wood.pixel(x, 160));
        QVERIFY(colors.size() > 15);
    }

    void pieceSizeAndShadowAtHighDpi()
    {
        auto render = [](int scale, bool shadow, qreal dpr) {
            QImage image(qRound(100 * dpr), qRound(100 * dpr), QImage::Format_ARGB32_Premultiplied);
            image.setDevicePixelRatio(dpr);
            image.fill(Qt::transparent);
            QPainter painter(&image);
            PiecePainter::draw(painter, PieceImageProvider::instance().icon('K'), QRect(20, 20, 60, 66),
                               {true, shadow, scale});
            return image;
        };
        auto opaqueCount = [](const QImage& image) {
            int count = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if (qAlpha(image.pixel(x, y)) > 200) ++count;
            return count;
        };
        for (const qreal dpr : {1.0, 1.5, 2.0}) {
            const auto small = render(90, false, dpr);
            const auto large = render(112, false, dpr);
            QVERIFY(opaqueCount(large) > opaqueCount(small) * 1.4);
            QVERIFY(large != render(112, true, dpr));
            QCOMPARE(large, render(112, false, dpr));
            QVERIFY(qAlpha(large.pixel(qRound(50 * dpr), qRound(50 * dpr))) > 200);
        }
    }

    void configuredSizeSurvivesBoardAssignment()
    {
        ShogiBoard model;
        model.resetGameBoard();
        for (const int size : {20, 50, 100, 150}) {
            ShogiView view;
            view.configureFixedSizing(size);
            view.applyBoardAndRender(&model);
            QCOMPARE(view.squareSize(), size);
            QCOMPARE(view.fieldSize().width(), size);
            view.resize(view.sizeHint());
            const auto normal = view.toImage();
            QVERIFY(!normal.isNull());
            view.setFlipMode(true);
            const auto flipped = view.toImage();
            QCOMPARE(flipped.size(), normal.size());
            QVERIFY(flipped != normal);
        }
    }

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
