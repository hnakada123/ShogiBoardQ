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
#include <QtMath>
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
#include "shogiviewlayout.h"
#include "appsettings.h"
#include "boardappearancecatalog.h"
#include "boardappearancepreview.h"
#include "candidatearrowcontroller.h"

namespace {
ShogiViewLayout viewLayout(const ShogiView& view)
{
    ShogiViewLayout layout;
    layout.setSquareSize(view.squareSize());
    layout.setStandGapCols(0.7);
    layout.setFlipMode(view.flipMode());
    layout.recalcLayoutParams(view.font());
    return layout;
}

QImage renderView(ShogiView& view, qreal dpr)
{
    view.resize(view.sizeHint());
    QImage image(qRound(view.width() * dpr), qRound(view.height() * dpr), QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(dpr);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    view.render(&painter);
    return image;
}

QRect changedBounds(const QImage& before, const QImage& after, const QRect& cell, qreal dpr)
{
    QRect result;
    const QRect pixels(qFloor(cell.x() * dpr), qFloor(cell.y() * dpr),
                       qCeil(cell.width() * dpr), qCeil(cell.height() * dpr));
    for (int y = pixels.top(); y <= pixels.bottom(); ++y) {
        for (int x = pixels.left(); x <= pixels.right(); ++x) {
            if (before.pixel(x, y) != after.pixel(x, y)) result |= QRect(x, y, 1, 1);
        }
    }
    return result;
}
}

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
            QCOMPARE(samples.size(), component == Component::Information ? 20 : 26);
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
        QCOMPARE(pieces->count(), AppSettings::availablePieceStyles().size());
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

    void boardOuterMargins()
    {
        BoardAppearance::instance().setVisuals({false, true, 108, false});
        ShogiBoard model;
        model.resetGameBoard();
        ShogiView view;
        view.applyBoardAndRender(&model);
        for (const int size : {20, 53, 100, 150}) {
            view.setSquareSize(size);
            view.setFlipMode(false);
            const QSize normalSize = view.sizeHint();
            const auto normalLayout = viewLayout(view);
            for (const bool flipped : {false, true, false, true}) {
                view.setFlipMode(flipped);
                const auto layout = viewLayout(view);
                QCOMPARE(view.sizeHint(), normalSize);
                QCOMPARE(layout.boardSurfaceRect(9, 9), normalLayout.boardSurfaceRect(9, 9));
                QCOMPARE(layout.offsetX(), normalLayout.offsetX());
                QCOMPARE(layout.offsetY(), normalLayout.offsetY());
                QCOMPARE(flipped ? layout.blackStandBoundingRect(9, 9) : layout.whiteStandBoundingRect(9, 9),
                         normalLayout.whiteStandBoundingRect(9, 9));
                QCOMPARE(flipped ? layout.whiteStandBoundingRect(9, 9) : layout.blackStandBoundingRect(9, 9),
                         normalLayout.blackStandBoundingRect(9, 9));
                for (const qreal dpr : {1.0, 1.25, 2.0}) {
                    const auto image = renderView(view, dpr);
                    const int x = image.width() / 2;
                    const QRgb background = view.boardColors().background.rgb();
                    int top = 0, bottom = image.height() - 1;
                    while (top < image.height() && image.pixel(x, top) == background) ++top;
                    while (bottom >= 0 && image.pixel(x, bottom) == background) --bottom;
                    QVERIFY(top < bottom);
                    QVERIFY(top > 0);
                    QVERIFY2(qAbs(top - (image.height() - 1 - bottom)) <= qCeil(1.5 * dpr),
                             qPrintable(QStringLiteral("size=%1 flip=%2 dpr=%3 top=%4 bottom=%5")
                                 .arg(size).arg(flipped).arg(dpr).arg(top).arg(image.height() - 1 - bottom)));
                }
                // 反転後も、盤上の81マスは表示位置で選択できる。
                for (int file = 1; file <= 9; ++file) {
                    for (int rank = 1; rank <= 9; ++rank) {
                        const QRect cell = view.calculateSquareRectangleBasedOnBoardState(file, rank);
                        QCOMPARE(view.clickedSquare(cell.center() + QPoint(layout.boardLeftPx(), layout.offsetY())), QPoint(file, rank));
                    }
                }
            }
        }
    }

    void standPiecesKeepPadding_data()
    {
        QTest::addColumn<int>("squareSize");
        QTest::addColumn<int>("pieceScale");
        QTest::addColumn<qreal>("dpr");
        QTest::addColumn<bool>("shadow");
        QTest::newRow("small") << 20 << 112 << 1.0 << true;
        QTest::newRow("screenshot") << 53 << 108 << 1.25 << true;
        QTest::newRow("small-pieces") << 53 << 90 << 1.0 << false;
        QTest::newRow("large-pieces-hidpi") << 53 << 112 << 2.0 << true;
        QTest::newRow("large-board") << 100 << 108 << 1.5 << false;
        QTest::newRow("largest-board") << 150 << 112 << 1.0 << true;
    }

    void standPiecesKeepPadding()
    {
        QFETCH(int, squareSize);
        QFETCH(int, pieceScale);
        QFETCH(qreal, dpr);
        QFETCH(bool, shadow);
        BoardAppearance::instance().setVisuals({false, shadow, pieceScale, false});
        ShogiBoard model;
        ShogiView view;
        view.configureFixedSizing(squareSize);
        view.applyBoardAndRender(&model);
        for (const bool flipped : {false, true}) {
            view.setFlipMode(flipped);
            const auto layout = viewLayout(view);
            for (const int count : {1, 2, 5, 18}) {
                model.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1"));
                const auto before = renderView(view, dpr);
                model.incrementPieceOnStand(Piece::BlackBishop);
                model.incrementPieceOnStand(Piece::WhiteBishop);
                for (int i = 0; i < count; ++i) {
                    model.incrementPieceOnStand(Piece::BlackPawn);
                    model.incrementPieceOnStand(Piece::WhitePawn);
                }
                const auto after = renderView(view, dpr);
                for (const bool black : {true, false}) {
                    const QRect stand = black ? layout.blackStandBoundingRect(9, 9) : layout.whiteStandBoundingRect(9, 9);
                    const auto bounds = changedBounds(before, after, stand, dpr);
                    QVERIFY(!bounds.isEmpty());
                    const qreal woodInset = qMax(1.0, squareSize * 0.035);
                    // 駒・影・バッジすべてが木製部分から最低1論理px内側にあること。
                    const QRectF safe = QRectF(stand).adjusted(woodInset + 1, 1, -woodInset - 1, -1);
                    const QRectF ink(bounds.x() / dpr, bounds.y() / dpr,
                                     bounds.width() / dpr, bounds.height() / dpr);
                    QVERIFY2(safe.contains(ink), qPrintable(QStringLiteral("flip=%1 count=%2 black=%3")
                                                              .arg(flipped).arg(count).arg(black)));
                    const int w = view.fieldSize().width(), h = view.fieldSize().height();
                    const bool bottom = black != flipped;
                    const QPoint pawn = stand.topLeft() + QPoint(bottom ? w / 2 : w + w / 2,
                                                                  bottom ? 3 * h + h / 2 : h / 2);
                    const QPoint bishop = stand.topLeft() + QPoint(bottom ? w + w / 2 : w / 2,
                                                                    bottom ? h + h / 2 : 2 * h + h / 2);
                    QCOMPARE(view.clickedSquare(pawn), black ? QPoint(10, 1) : QPoint(11, 9));
                    QCOMPARE(view.clickedSquare(bishop), black ? QPoint(10, 6) : QPoint(11, 4));
                }
            }
        }
    }

    void singleBishopsAreCentered()
    {
        BoardAppearance::instance().setVisuals({false, false, 108, false});
        ShogiBoard model;
        ShogiView view;
        view.configureFixedSizing(53);
        view.applyBoardAndRender(&model);
        for (const bool flipped : {false, true}) {
            view.setFlipMode(flipped);
            const auto layout = viewLayout(view);
            model.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1"));
            const auto before = renderView(view, 2);
            model.incrementPieceOnStand(Piece::BlackBishop);
            model.incrementPieceOnStand(Piece::WhiteBishop);
            const auto after = renderView(view, 2);
            for (const bool black : {true, false}) {
                const QRect stand = black ? layout.blackStandBoundingRect(9, 9) : layout.whiteStandBoundingRect(9, 9);
                const int w = view.fieldSize().width(), h = view.fieldSize().height();
                const bool bottom = black != flipped;
                const QRect cell(stand.topLeft() + QPoint(bottom ? w : 0, bottom ? h : 2 * h), QSize(w, h));
                const auto bounds = changedBounds(before, after, cell, 2);
                QVERIFY(!bounds.isEmpty());
                const qreal centerX = (bounds.left() + bounds.width() / 2.0) / 2;
                QVERIFY(qAbs(centerX - QRectF(cell).center().x()) <= 1.0);
            }
        }
    }

    void dropArrowsStartAtStand_data()
    {
        QTest::addColumn<QChar>("piece");
        QTest::addColumn<int>("standRank");
        QTest::addColumn<bool>("black");
        QTest::addColumn<bool>("flipped");
        const QString pieces = QStringLiteral("PLNSGBR");
        for (int i = 0; i < pieces.size(); ++i) {
            for (const bool black : {true, false}) {
                for (const bool flipped : {false, true}) {
                    const QByteArray name = QStringLiteral("%1-%2-%3")
                        .arg(pieces.at(i)).arg(black ? "black" : "white").arg(flipped ? "flipped" : "normal").toUtf8();
                    QTest::newRow(name.constData()) << pieces.at(i) << (black ? i + 1 : 9 - i) << black << flipped;
                }
            }
        }
    }

    void dropArrowsStartAtStand()
    {
        QFETCH(QChar, piece);
        QFETCH(int, standRank);
        QFETCH(bool, black);
        QFETCH(bool, flipped);
        const QString sfen = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 %1 RBGSNLPrbgsnlp 1")
            .arg(black ? "b" : "w");
        ShogiBoard model;
        model.setSfen(sfen);
        ShogiView view;
        view.configureFixedSizing(50);
        view.applyBoardAndRender(&model);
        view.setFlipMode(flipped);
        auto arrow = CandidateArrowController::arrowForMove(QStringLiteral("%1*5e").arg(piece), sfen, 1);
        QVERIFY(arrow);
        arrow->color = QColor(255, 0, 255);
        view.setArrows({*arrow});
        const auto image = renderView(view, 1.0);
        const QRect source = view.standPieceRect(arrow->dropPiece);
        QVERIFY(!source.isEmpty());
        // 描画とは独立した駒台のヒットテストで、先後・反転・駒種の対応を確認する。
        QCOMPARE(view.clickedSquare(source.center()), QPoint(black ? 10 : 11, standRank));
        const auto layout = viewLayout(view);
        const QPointF from(source.center());
        const QPointF to(view.cachedFieldRect(5, 5).translated(layout.offsetX(), layout.offsetY()).center());
        // 駒台の駒に始点が隠れず、打ち先まで実際に線が描かれていることを確認する。
        for (const qreal progress : {0.0, 0.4, 0.6, 0.8}) {
            const QPoint point = (from + (to - from) * progress).toPoint();
            QCOMPARE(image.pixelColor(point), arrow->color);
        }
        // 線の終端より先にある矢尻の内側を確認し、縁のアンチエイリアスは避ける。
        const QPointF direction = to - from;
        const QPointF unit = direction / std::hypot(direction.x(), direction.y());
        const QPoint head = (to - unit * 5).toPoint();
        QCOMPARE(image.pixelColor(head), arrow->color);
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
