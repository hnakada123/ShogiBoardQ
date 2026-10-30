/// @file tst_board_image_renderer.cpp
/// @brief BoardImageRenderer のユニットテスト（offscreen で描画して画像を検証する）

#include <QtTest>

#include <QImage>
#include <QSet>
#include <QTemporaryDir>
#include <QPainter>
#include <QPixmapCache>
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

#include "piecestylecatalog.h"
#include "appearancestate.h"
#include "boardimagerenderer.h"
#include "boardappearance.h"
#include "boardappearancedialog.h"
#include "boardsurfacepainter.h"
#include "piecepainter.h"
#include "pieceimageprovider.h"
#include "settingscommon.h"
#include "positioneditrules.h"
#include "shogiboard.h"
#include <QRegion>

#include "boardgeometry.h"
#include "shogiview.h"
#include "appearancesettings.h"
#include "boardappearancecatalog.h"
#include "boardappearancepreview.h"
#include "candidatearrowpresenter.h"

namespace {
BoardGeometry viewLayout(const ShogiView& view)
{
    return view.boardGeometry();
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
    /// 盤の位置の値は、マス・駒台のセル・座標の文字の帯をウィジェット座標で持ち、反転しても全体の位置を変えない
    void geometryIsComputedOnceInWidgetCoordinates()
    {
        for (const int size : {20, 50, 53, 150}) {
            BoardGeometry::Params params;
            params.squareSize = size;
            const BoardGeometry normal(params);
            params.flipped = true;
            const BoardGeometry flipped(params);
            QCOMPARE(flipped.surface(), normal.surface());
            QCOMPARE(flipped.board(), normal.board());
            QCOMPARE(flipped.viewSize(), normal.viewSize());
            QCOMPARE(normal.fieldSize(), QSize(size, qRound(size * BoardGeometry::kSquareAspectRatio)));
            for (const BoardGeometry* geometry : {&normal, &flipped}) {
                for (int file = 1; file <= 9; ++file) {
                    for (int rank = 1; rank <= 9; ++rank) {
                        const QRect cell = geometry->square(file, rank);
                        QVERIFY(geometry->board().contains(cell));
                        QCOMPARE(geometry->squareAt(cell.center()), QPoint(file, rank));
                    }
                }
                QVERIFY(geometry->square(0, 1).isEmpty());
                QVERIFY(geometry->squareAt(geometry->board().topLeft() - QPoint(1, 1)).isNull());
                // 駒台の8セルは駒台の中で重ならず、成駒は生駒のセル
                for (const Turn side : {Turn::Black, Turn::White}) {
                    QRegion cells;
                    for (const Piece piece : BoardGeometry::standPieces(side)) {
                        const QRect cell = geometry->standCell(piece);
                        QVERIFY(geometry->stand(side).contains(cell));
                        QVERIFY(!cells.intersects(cell));
                        cells += cell;
                    }
                    QCOMPARE(geometry->standCell(side == Turn::Black ? Piece::BlackDragon : Piece::WhiteDragon),
                             geometry->standCell(side == Turn::Black ? Piece::BlackRook : Piece::WhiteRook));
                }
                QVERIFY(!geometry->board().intersects(geometry->rankLabel(1)));
                QVERIFY(!geometry->board().intersects(geometry->fileLabel(1)));
            }
            // 手前の駒台では王が右上・歩が左下。向こうの駒台はそれを180度回す
            const QRect blackStand = normal.stand(Turn::Black);
            QCOMPARE(normal.standCell(Piece::BlackKing).topLeft(), blackStand.topLeft() + QPoint(size, 0));
            QCOMPARE(normal.standCell(Piece::BlackPawn).bottomLeft(), blackStand.bottomLeft());
            const QRect whiteStand = normal.stand(Turn::White);
            QCOMPARE(normal.standCell(Piece::WhiteKing).bottomLeft(), whiteStand.bottomLeft());
            QCOMPARE(flipped.standCell(Piece::WhiteKing).topLeft(), flipped.stand(Turn::White).topLeft() + QPoint(size, 0));
            // 反転すると駒台の位置が入れ替わる（盤の右下にあるのは手前の駒台）
            QVERIFY(blackStand.left() > normal.board().right());
            QVERIFY(flipped.stand(Turn::White).left() > flipped.board().right());
        }
    }

    /// 駒台の大きさは条件で変えられる（詰将棋問題一覧のプレビューは1列の持ち駒の一覧）。手前の駒台は盤の右で
    /// 下端を、奥の駒台は盤の左で上端を揃え、全体の幅は盤と両側の駒台・間の和
    void standSizeFollowsParams()
    {
        for (const bool flip : {false, true}) {
            BoardGeometry::Params params;
            params.squareSize = 24;
            params.flipped = flip;
            const BoardGeometry standard(params);
            params.standColumns = 1.8;
            params.standRows = 7;
            const BoardGeometry list(params);
            const QSize field = list.fieldSize();
            const Turn nearSide = flip ? Turn::White : Turn::Black;
            const Turn farSide = flip ? Turn::Black : Turn::White;
            QCOMPARE(list.stand(nearSide).size(), QSize(qRound(field.width() * 1.8), field.height() * 7));
            QCOMPARE(list.stand(farSide).size(), list.stand(nearSide).size());
            QCOMPARE(list.stand(nearSide).bottom(), list.board().bottom());
            QVERIFY(list.stand(nearSide).left() > list.board().right());
            QCOMPARE(list.stand(farSide).top(), list.board().top());
            QVERIFY(list.stand(farSide).right() < list.board().left());
            QCOMPARE(list.viewSize().width() - list.board().width(),
                     2 * (list.board().left() - list.stand(farSide).left()));
            // 盤のマスの大きさは駒台の大きさによらない
            QCOMPARE(list.fieldSize(), standard.fieldSize());
            QCOMPARE(list.board().size(), standard.board().size());
            QCOMPARE(standard.stand(nearSide).size(), QSize(field.width() * 2, field.height() * 4));
        }
    }

    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void init()
    {
        SettingsCommon::openSettings().clear();
    }

    void appearanceControlsPropagateAndPersist()
    {
        ShogiView first, second;
        BoardAppearanceDialog dialog;
        // 外観のテーマはおすすめの組み合わせにまとめた
        QVERIFY(!dialog.findChild<QComboBox*>("boardThemeCombo"));
        auto* combination = dialog.findChild<QComboBox*>("appearanceCombination");
        auto* grain = dialog.findChild<QCheckBox*>("boardWoodGrain");
        auto* shadow = dialog.findChild<QCheckBox*>("boardPieceShadow");
        auto* scale = dialog.findChild<QSpinBox*>("boardPieceScale");
        QVERIFY(combination && grain && shadow && scale);
        QCOMPARE(combination->currentIndex(), 0);  // 標準・榧と畳
        grain->setChecked(false);
        shadow->setChecked(false);
        scale->setValue(94);
        QCOMPARE(combination->currentIndex(), -1);
        QVERIFY(first.boardVisuals() == (BoardVisuals{false, false, 94}));
        QVERIFY(second.boardVisuals() == first.boardVisuals());
        SettingsCommon::openSettings().sync();
        ShogiView restored;
        BoardAppearanceDialog restoredDialog;
        QVERIFY(restored.boardVisuals() == first.boardVisuals());
        QVERIFY(restored.boardColors() == first.boardColors());
        QCOMPARE(restoredDialog.findChild<QSpinBox*>("boardPieceScale")->value(), 94);
        QVERIFY(QMetaObject::invokeMethod(combination, "activated", Q_ARG(int, 0)));
        QVERIFY(first.boardColors() == BoardColors{});
        // 組み合わせは部品の見本（木目を含む）と駒のセットだけを変え、駒の影と大きさは残す
        QVERIFY(first.boardVisuals() == (BoardVisuals{true, false, 94, true}));
        QCOMPARE(restoredDialog.findChild<QComboBox*>("appearanceCombination")->currentIndex(), 0);
    }

    /// 以前の外観のテーマ「墨と榧」「琥珀」は、標準の駒のおすすめの組み合わせにした（盤・駒台・背景・カードはテーマの色）
    void formerThemesAreCombinations()
    {
        using Catalog = BoardAppearanceCatalog;
        struct Theme {
            const char* name;
            QRgb background, board, stand, cardBackground, nameText, accent;
        };
        const Theme themes[] = {{"標準・墨と榧", 0x303b38, 0xe1c68e, 0x86715a, 0x3e4b44, 0xe5e7d9, 0x7d9878},
                                {"標準・琥珀", 0x39352f, 0xe3b161, 0xa26a37, 0xeee4cf, 0x483c29, 0x8c6937}};
        const auto& combinations = Catalog::combinations();
        for (const Theme& theme : themes) {
            int index = -1;
            for (int i = 0; i < combinations.size(); ++i)
                if (QString::fromUtf8(combinations.at(i).name) == QString::fromUtf8(theme.name)) index = i;
            QVERIFY2(index > 0, theme.name);
            const AppearanceState state = Catalog::applied(combinations.at(index), AppearanceState{});
            QCOMPARE(state.pieceStyle, QStringLiteral("standard"));
            QCOMPARE(state.colors.background, QColor(theme.background));
            QCOMPARE(state.colors.board, QColor(theme.board));
            QCOMPARE(state.colors.grid, BoardColors{}.grid);
            QCOMPARE(state.colors.stand, QColor(theme.stand));
            QCOMPARE(state.colors.cardBackground, QColor(theme.cardBackground));
            QCOMPARE(state.colors.nameText, QColor(theme.nameText));
            QCOMPARE(state.colors.turnBackground, QColor(theme.accent));
            QVERIFY(state.visuals == BoardVisuals{});
        }
    }

    /// おすすめの組み合わせは、ある駒のセットとある見本だけを指し、当てはめるとその組み合わせとして選ばれる
    void combinationsReferToCatalog()
    {
        using Catalog = BoardAppearanceCatalog;
        const auto& combinations = Catalog::combinations();
        QVERIFY(!combinations.isEmpty());
        QSet<QString> names;
        for (int i = 0; i < combinations.size(); ++i) {
            const auto& combination = combinations.at(i);
            const QString name = Catalog::combinationName(combination);
            QVERIFY2(!names.contains(name), qPrintable(name));
            names.insert(name);
            QVERIFY2(PieceStyleCatalog::contains(QString::fromLatin1(combination.pieceStyle)), combination.pieceStyle);
            for (int c = 0; c < Catalog::kComponentCount; ++c) {
                const char* key = combination.samples.at(static_cast<std::size_t>(c));
                QVERIFY2(Catalog::indexOf(static_cast<Catalog::Component>(c), QString::fromUtf8(key)) >= 0, key);
            }
            AppearanceState start;
            start.pieceStyle = QStringLiteral("deep_navy");
            start.visuals.pieceScale = 95;
            const AppearanceState state = Catalog::applied(combination, start);
            QCOMPARE(state.pieceStyle, QString::fromLatin1(combination.pieceStyle));
            QCOMPARE(state.visuals.pieceScale, 95);
            QCOMPARE(Catalog::combinationIndex(state), i);
        }
        QCOMPARE(Catalog::combinationIndex(AppearanceState{}), 0);  // 標準・榧と畳
    }

    void componentSamplesAreIndependent()
    {
        using Component = BoardAppearanceCatalog::Component;
        for (const auto component : {Component::Board, Component::Stand, Component::Information, Component::Background}) {
            const auto samples = BoardAppearanceCatalog::samples(component);
            QCOMPARE(samples.size(), component == Component::Information ? 22 : component == Component::Background ? 27 : 28);
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
        PositionEditRules::clear(model);
        ShogiView live;
        live.applyBoardAndRender(&model);
        live.chrome()->card(Turn::Black)->setClockText(QStringLiteral("03:21"));
        const auto position = model.position().boardSfen();
        BoardAppearanceDialog dialog;
        auto* pieces = dialog.findChild<QListWidget*>("appearancePieces");
        auto* preview = dialog.findChild<BoardAppearancePreview*>();
        auto* combination = dialog.findChild<QComboBox*>("appearanceCombination");
        QVERIFY(pieces && preview && combination);
        QCOMPARE(pieces->count(), PieceStyleCatalog::ids().size());
        for (const auto* list : dialog.findChildren<QListWidget*>()) {
            for (int i = 0; i < list->count(); ++i) {
                const auto icon = list->item(i)->icon();
                QCOMPARE(icon.pixmap(142, 86, QIcon::Normal).toImage(),
                         icon.pixmap(142, 86, QIcon::Selected).toImage());
            }
        }
        QCOMPARE(preview->findChild<ShogiBoard*>()->position().boardSfen(),
                 QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL"));
        const auto original = preview->image();
        QVERIFY(QMetaObject::invokeMethod(combination, "activated", Q_ARG(int, 4)));  // 黒檀・月白
        QCOMPARE(AppearanceSettings::pieceStyle(), QStringLiteral("deep_ebony"));
        QVERIFY(preview->image() != original);
        const auto selected = preview->image();
        preview->setPosition(1);
        QVERIFY(preview->image() != selected);
        const auto middle = preview->image();
        preview->setFlipped(true);
        QVERIFY(preview->image() != middle);
        QCOMPARE(preview->findChild<ShogiView*>()->piece(Piece::BlackKing).pixmap(90).toImage(),
                 QIcon(":/pieces/deep_ebony/Gote_ou45.svg").pixmap(90).toImage());
        QCOMPARE(model.position().boardSfen(), position);
        QCOMPARE(live.chrome()->card(Turn::Black)->clockLabel()->text(), QStringLiteral("03:21"));
        QVERIFY(!live.flipMode());
        dialog.findChild<QPushButton*>("restoreOpeningAppearanceButton")->click();
        QVERIFY(live.boardColors() == BoardColors{});
        QVERIFY(live.boardVisuals() == BoardVisuals{});
        QCOMPARE(AppearanceSettings::pieceStyle(), QStringLiteral("standard"));
        preview->setPosition(0);
        preview->setFlipped(false);
        QCOMPARE(preview->image(), original);
    }

    void appearanceSamplesAreDrawnWhenShown()
    {
        // 見本は一覧に描くときに作る。空の画像のまま表示されず、駒を変えると盤の見本も描き直す
        BoardAppearanceDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* sections = dialog.findChild<QTabWidget*>("appearanceSections");
        auto* pieces = dialog.findChild<QListWidget*>("appearancePieces");
        auto* boards = dialog.findChild<QListWidget*>("appearanceBoards");
        QVERIFY(sections && pieces && boards);
        auto sample = [](QListWidget* list) {
            QCoreApplication::processEvents();
            const QRect item = list->visualItemRect(list->item(0));
            // 見本の画像が入る上側の中央だけを見る（項目名の文字は含めない）
            const QRect icon(item.center().x() - 50, item.top() + 20, 100, 50);
            return list->viewport()->grab(icon).toImage();
        };
        auto colorCount = [](const QImage& image) {
            QSet<QRgb> colors;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x) colors.insert(image.pixel(x, y));
            return colors.size();
        };
        sections->setCurrentWidget(pieces->parentWidget());
        pieces->scrollToTop();
        QVERIFY(colorCount(sample(pieces)) > 20);
        sections->setCurrentWidget(boards->parentWidget());
        boards->scrollToTop();
        const auto standard = sample(boards);
        QVERIFY(colorCount(standard) > 20);
        BoardAppearance::instance().setPieceStyle(QStringLiteral("chess_facet_wood"));
        QVERIFY(sample(boards) != standard);
        BoardAppearance::instance().setPieceStyle(QStringLiteral("standard"));
        QCOMPARE(sample(boards), standard);
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
            PiecePainter::draw(painter, PieceImageProvider::instance().icon(Piece::BlackKing), QRect(20, 20, 60, 66),
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

    // 駒画像は条件（駒・大きさ・倍率・影）ごとに作って使い回し、キャッシュから消えたら作り直す
    void pieceImagesAreReusedPerCondition()
    {
        const QIcon king = PieceImageProvider::instance().icon(Piece::BlackKing);
        const QIcon pawn = PieceImageProvider::instance().icon(Piece::BlackPawn);
        const BoardVisuals shadowed{true, true, 100};
        const BoardVisuals flat{true, false, 100};
        for (const qreal dpr : {1.0, 1.25, 2.0}) {
            const auto first = PiecePainter::image(king, 60, shadowed, dpr);
            QVERIFY(!first.pixmap.isNull());
            QCOMPARE(PiecePainter::image(king, 60, shadowed, dpr).pixmap.cacheKey(), first.pixmap.cacheKey());
            for (const auto& other : {PiecePainter::image(pawn, 60, shadowed, dpr), PiecePainter::image(king, 60, flat, dpr),
                                      PiecePainter::image(king, 50, shadowed, dpr),
                                      PiecePainter::image(king, 60, shadowed, dpr + 0.5)}) {
                QVERIFY(other.pixmap.cacheKey() != first.pixmap.cacheKey());
                QVERIFY(other.pixmap.toImage() != first.pixmap.toImage());
            }
            const QImage before = first.pixmap.toImage();
            QPixmapCache::clear();
            const auto again = PiecePainter::image(king, 60, shadowed, dpr);
            QVERIFY(again.pixmap.cacheKey() != first.pixmap.cacheKey());
            QCOMPARE(again.pixmap.toImage(), before);
            QCOMPARE(again.bounds, first.bounds);
        }
    }

    void configuredSizeSurvivesBoardAssignment()
    {
        ShogiBoard model;
        PositionEditRules::clear(model);
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
        PositionEditRules::clear(model);
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
                QCOMPARE(layout.surface(), normalLayout.surface());
                QCOMPARE(layout.board().left(), normalLayout.board().left());
                QCOMPARE(layout.board().top(), normalLayout.board().top());
                QCOMPARE(flipped ? layout.stand(Turn::Black) : layout.stand(Turn::White),
                         normalLayout.stand(Turn::White));
                QCOMPARE(flipped ? layout.stand(Turn::White) : layout.stand(Turn::Black),
                         normalLayout.stand(Turn::Black));
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
                        const QRect cell = layout.square(file, rank);
                        QCOMPARE(view.clickedSquare(cell.center()), QPoint(file, rank));
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
                // 両方の駒台に角1枚と歩 count 枚
                const QString pawns = count > 1 ? QString::number(count) : QString();
                model.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b B%1Pb%1p 1").arg(pawns));
                const auto after = renderView(view, dpr);
                for (const bool black : {true, false}) {
                    const QRect stand = black ? layout.stand(Turn::Black) : layout.stand(Turn::White);
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
            model.setSfen(QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b Bb 1"));
            const auto after = renderView(view, 2);
            for (const bool black : {true, false}) {
                const QRect stand = black ? layout.stand(Turn::Black) : layout.stand(Turn::White);
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
        auto arrow = CandidateArrowPresenter::arrowForMove(QStringLiteral("%1*5e").arg(piece), sfen, 1);
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
        const QPointF to(layout.square(5, 5).center());
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
        Failure failure;
        QSize size;
        QVERIFY2(BoardImageRenderer::renderToFile(QStringLiteral("startpos"), path, options, &failure, &size),
                 qPrintable(failure.message));
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
        Failure failure;
        const QImage a = BoardImageRenderer::render(sfen, normal, &failure);
        QVERIFY2(!a.isNull(), qPrintable(failure.message));
        const QImage b = BoardImageRenderer::render(sfen, flipped, &failure);
        QVERIFY2(!b.isNull(), qPrintable(failure.message));
        QCOMPARE(a.size(), b.size());
        QVERIFY(a != b);
    }

    /// 局面・大きさ・最後の手の誤りは引数の誤り（invalid_argument）として返す
    void rejectsInvalidInput()
    {
        Failure failure;
        QVERIFY(BoardImageRenderer::render(QStringLiteral("not a sfen"), {}, &failure).isNull());
        QCOMPARE(failure.kind, ErrorKind::InvalidArgument);
        QVERIFY(!failure.message.isEmpty());
        BoardImageRenderer::Options tooSmall;
        tooSmall.squareSize = 5;
        failure = {};
        QVERIFY(BoardImageRenderer::render(QStringLiteral("startpos"), tooSmall, &failure).isNull());
        QCOMPARE(failure.kind, ErrorKind::InvalidArgument);
        BoardImageRenderer::Options badMove;
        badMove.lastMoveUsi = QStringLiteral("zzzz");
        failure = {};
        QVERIFY(BoardImageRenderer::render(QStringLiteral("startpos"), badMove, &failure).isNull());
        QCOMPARE(failure.kind, ErrorKind::InvalidArgument);
    }
};

QTEST_MAIN(TestBoardImageRenderer)
#include "tst_board_image_renderer.moc"
