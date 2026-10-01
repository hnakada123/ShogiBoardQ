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
#include <QListWidget>
#include <QLabel>
#include <QPushButton>
#include <QTabWidget>
#include <cmath>

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
#include "boardappearancecatalog.h"
#include "boardappearancepreview.h"

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
            QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("standard"));
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

    void componentSamplesAreIndependent()
    {
        using Component = BoardAppearanceCatalog::Component;
        for (const auto component : {Component::Board, Component::Stand, Component::Information, Component::Background}) {
            const auto samples = BoardAppearanceCatalog::samples(component);
            QCOMPARE(samples.size(), 20);
            QSet<QString> names;
            for (const auto& sample : samples) {
                QVERIFY(!names.contains(sample.name));
                names.insert(sample.name);
                BoardColors colors;
                colors.background = QColor("#123456");
                colors.stand = QColor("#abcdef");
                colors.clockBackground = QColor(40, 50, 60, 90);
                const BoardColors before = colors;
                BoardVisuals visuals{false, false, 95, false};
                BoardAppearanceCatalog::apply(component, sample, colors, visuals);
                QVERIFY(BoardAppearanceCatalog::matches(component, sample, colors, visuals));
                QVERIFY(colors == colors.normalized());
                QCOMPARE(visuals.pieceScale, 95);
                QCOMPARE(visuals.pieceShadow, false);
                const auto members = BoardColors::members();
                for (size_t i = 0; i < members.size(); ++i) {
                    const bool affected = component == Component::Board ? (i == 1 || i == 3)
                        : component == Component::Stand ? i == 2
                        : component == Component::Background ? i == 0 : i >= 4;
                    if (!affected) QCOMPARE(colors.*members[i], before.*members[i]);
                }
                if (component != Component::Board) QCOMPARE(visuals.woodGrain, false);
                if (component != Component::Stand) QCOMPARE(visuals.standWoodGrain, false);
                int matching = 0;
                for (const auto& candidate : samples)
                    if (BoardAppearanceCatalog::matches(component, candidate, colors, visuals)) ++matching;
                QCOMPARE(matching, 1);
            }
        }
        const auto luminance = [](QColor color) {
            const auto channel = [](double value) {
                return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
            };
            return .2126 * channel(color.redF()) + .7152 * channel(color.greenF()) + .0722 * channel(color.blueF());
        };
        for (const auto& sample : BoardAppearanceCatalog::samples(Component::Information)) {
            const auto& c = sample.colors;
            for (const auto& pair : {qMakePair(c.cardBackground, c.nameText),
                                      qMakePair(c.cardBackground, c.clockText), qMakePair(c.turnBackground, c.turnText)}) {
                const auto a = luminance(pair.first), b = luminance(pair.second);
                QVERIFY2((qMax(a, b) + .05) / (qMin(a, b) + .05) >= 4.5, qPrintable(sample.name));
            }
        }
    }

    void appearancePreviewAndRestore()
    {
        ShogiBoard model;
        model.resetGameBoard();
        ShogiView live;
        live.applyBoardAndRender(&model);
        live.setBlackClockText(QStringLiteral("03:21"));
        const auto position = model.convertBoardToSfen();
        BoardColorDialog dialog;
        auto* pieces = dialog.findChild<QListWidget*>("appearancePieces");
        auto* preview = dialog.findChild<BoardAppearancePreview*>();
        auto* combination = dialog.findChild<QComboBox*>("appearanceCombination");
        QVERIFY(pieces && preview && combination);
        QCOMPARE(pieces->count(), 21);
        for (const auto* list : dialog.findChildren<QListWidget*>()) {
            for (int i = 0; i < list->count(); ++i) {
                const auto icon = list->item(i)->icon();
                QCOMPARE(icon.pixmap(142, 86, QIcon::Normal).toImage(),
                         icon.pixmap(142, 86, QIcon::Selected).toImage());
            }
        }
        QCOMPARE(preview->findChild<ShogiBoard*>()->convertBoardToSfen(),
                 QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));
        const auto original = preview->image();
        QVERIFY(QMetaObject::invokeMethod(combination, "activated", Q_ARG(int, 2)));
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("deep_ebony"));
        QVERIFY(preview->image() != original);
        const auto selected = preview->image();
        preview->setPosition(1);
        QVERIFY(preview->image() != selected);
        const auto middle = preview->image();
        preview->setFlipped(true);
        QVERIFY(preview->image() != middle);
        QCOMPARE(preview->findChild<ShogiView*>()->piece('K').pixmap(90).toImage(),
                 QIcon(":/pieces/deep_ebony/Gote_ou45.svg").pixmap(90).toImage());
        QCOMPARE(model.convertBoardToSfen(), position);
        QCOMPARE(live.blackClockLabel()->text(), QStringLiteral("03:21"));
        QVERIFY(!live.flipMode());
        dialog.findChild<QPushButton*>("restoreOpeningAppearanceButton")->click();
        QVERIFY(live.boardColors() == BoardColors{});
        QVERIFY(live.boardVisuals() == BoardVisuals{});
        QCOMPARE(AppSettings::pieceStyle(), QStringLiteral("standard"));
        preview->setPosition(0);
        preview->setFlipped(false);
        QCOMPARE(preview->image(), original);
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
