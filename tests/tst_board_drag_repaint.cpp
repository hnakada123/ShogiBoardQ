#include <QtTest>
#include <QAction>
#include <QFile>
#include <QImage>
#include <QMenu>
#include <QPaintEvent>
#include <QScreen>
#include <QTemporaryDir>
#include <QWindow>
#include <QtMath>

#include "piecestylecatalog.h"
#include "appsettings.h"
#include "boardappearance.h"
#include "boardappearancecatalog.h"
#include "boardconstants.h"
#include "boardinputroute.h"
#include "boardinteractioncontroller.h"
#include "gamesettings.h"
#include "pieceimageprovider.h"
#include "pvboarddialog.h"
#include "boardviewerwidget.h"
#include "sfencollectiondialog.h"
#include "sfenvalidationservice.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "boardchromewidget.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"

/// 駒をつまんで動かすときの部分再描画を検証する。
/// ドラッグ中は駒の周囲だけを描き直し、それでも画面が全体を描き直したときと一致すること。
/// 盤面の変わらない部分を描いた画像が、盤の反転・大きさ・配色・質感の変更に追従すること。
/// 対局画面の外観（駒・将棋盤・背景・駒台・対局者情報）をどれに変えても、描き残しが出ないこと。
/// 読み筋ウィンドウ・局面集ビューアで局面を替えても、変わった所だけを描き直すこと。
class TestBoardDragRepaint : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    /// ShogiView が受け取った再描画範囲を記録する
    class PaintSpy : public QObject
    {
    public:
        QList<QRect> rects;        // 再描画範囲の外接矩形
        QList<QRect> regionRects;  // 再描画範囲を構成する矩形

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Paint) {
                const auto* paint = static_cast<QPaintEvent*>(event);
                rects.append(paint->rect());
                for (const QRect& rect : paint->region()) regionRects.append(rect);
            }
            return QObject::eventFilter(watched, event);
        }
    };

    /// 盤で指した手の行き先（指した手はテストが盤に反映する）
    struct TestRoute : BoardInputRoute {
        bool editing = false;
        bool acceptsInput() const override { return true; }
        bool isEditing() const override { return editing; }
        void submit(const QPoint&, const QPoint&) override {}
    };

    struct BoardUi {
        TestRoute route;
        ShogiGameController gc;
        ShogiView view;
        std::unique_ptr<BoardInteractionController> controller;

        BoardUi(QString sfen, bool flipped)
        {
            gc.newGame(sfen);
            view.setBoard(gc.board());
            view.setFlipMode(flipped);
            view.setFixedSize(view.sizeHint());
            controller = std::make_unique<BoardInteractionController>(&view, &gc);
            controller->setLegalMovesVisible(true);
            controller->setInputRoute(&route);
            // 対局中と同じく、対局者名・時計・手番表示・直前の指し手を表示する
            view.chrome()->card(Turn::Black)->setName(QStringLiteral("先手"));
            view.chrome()->card(Turn::White)->setName(QStringLiteral("後手"));
            view.chrome()->card(Turn::Black)->setClockText(QStringLiteral("00:08:31"));
            view.chrome()->card(Turn::White)->setClockText(QStringLiteral("00:07:12"));
            controller->showMoveHighlights(QPoint(3, 3), QPoint(3, 4));
            view.show();
        }

        ~BoardUi() { controller->clearAllHighlights(); }

        /// 全ての層のハイライトのマスの数
        int highlightCount() const
        {
            int count = 0;
            for (const auto layer : {BoardHighlightLayer::LastMoveFrom, BoardHighlightLayer::LastMoveTo,
                                     BoardHighlightLayer::Selection, BoardHighlightLayer::LegalDestinations})
                count += static_cast<int>(view.highlighting()->highlights(layer).size());
            return count;
        }

        void moveTo(const QPointF& pos)
        {
            QMouseEvent move(QEvent::MouseMove, pos, view.mapToGlobal(pos), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&view, &move);
            QCoreApplication::processEvents();
        }
    };

    /// 実際に画面に出ている内容と、全体を描き直した内容とで違う画素の数。
    /// 部分描画での半透明の合成は、全体を描いたときと丸めが1だけ違うことがあるため、それは数えない。
    static int stalePixels(BoardUi& ui) { return stalePixels(ui.view); }

    static QImage screenImage(QWidget& widget)
    {
        QWindow* window = widget.window()->windowHandle();
        return window->screen()->grabWindow(widget.window()->winId()).toImage()
            .convertToFormat(QImage::Format_ARGB32);
    }

    static int stalePixels(QWidget& view)
    {
        const QImage partial = screenImage(view);
        view.repaint();
        QCoreApplication::processEvents();
        const QImage full = screenImage(view);
        if (partial.size() != full.size()) return -1;
        int stale = 0;
        for (int y = 0; y < full.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(partial.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(full.constScanLine(y));
            for (int x = 0; x < full.width(); ++x) {
                if (qAbs(qRed(a[x]) - qRed(b[x])) > 1 || qAbs(qGreen(a[x]) - qGreen(b[x])) > 1
                    || qAbs(qBlue(a[x]) - qBlue(b[x])) > 1)
                    ++stale;
            }
        }
        return stale;
    }

    /// 同じ状態の盤面を新しく作って描いた絵と比べる
    static bool sameAsNewBoard(ShogiView& view)
    {
        BoardUi fresh(sfen(), view.flipMode());
        fresh.view.setSquareSize(view.squareSize());
        fresh.view.setFixedSize(fresh.view.sizeHint());
        if (fresh.view.size() != view.size()) return false;
        return view.grab().toImage() == fresh.view.grab().toImage();
    }

    static QString sfen()
    {
        // 両方の駒台に持駒（枚数表示を含む）がある局面
        return QStringLiteral("ln1g3nl/1r1s1kg2/p2ppp1pp/2p3p2/1p7/2P1P1P2/PPSP1P2P/2G1G2R1/LN2K2NL b B2Sb3p 41");
    }

    /// 遅れて届く描き直しの要求まで処理する
    static void settle()
    {
        for (int i = 0; i < 3; ++i) QCoreApplication::processEvents();
    }

    /// 読み筋ウィンドウ・局面集ビューアで試す局面。sfen() と同じ配置で、局面集が読み込めるよう
    /// 駒の数を正しくしてある（sfen() は歩が19枚ある）
    static QString validSfen()
    {
        return QStringLiteral("ln1g3nl/1r1s1kg2/p2ppp1pp/2p3p2/1p7/2P1P1P2/PPSP1P2P/2G1G2R1/LN2K2NL b B2Sbp 41");
    }

    /// 角打ち・歩の突き・歩で取る・歩打ちの着手（USI 形式）と、各着手の後の局面
    static QStringList pvMoves()
    {
        return {QStringLiteral("B*5e"), QStringLiteral("8e8f"), QStringLiteral("8g8f"), QStringLiteral("P*8e")};
    }

    static QStringList positionsAfterPvMoves()
    {
        ShogiGameController gc;
        QString start = validSfen();
        gc.newGame(start);
        QStringList history{validSfen()};
        const QList<QPair<QPoint, QPoint>> game = {
            {QPoint(10, 6), QPoint(5, 5)}, {QPoint(8, 5), QPoint(8, 6)},
            {QPoint(8, 7), QPoint(8, 6)}, {QPoint(11, 9), QPoint(8, 5)},
        };
        for (const auto& [from, to] : game) {
            QPoint f = from, t = to;
            const std::optional<RecordEntry> played = gc.validateAndMove(f, t, {});
            if (!played) return {};
            history.append(played->sfen);
        }
        return history;
    }

    /// 局面を替える操作の後、盤が描き直した範囲が盤の4分の1未満で、描き残しが無いこと
    static void verifyPartialStep(ShogiView& view, const std::function<void()>& step, const QString& label)
    {
        PaintSpy spy;
        view.installEventFilter(&spy);
        step();
        settle();
        view.removeEventFilter(&spy);
        QRegion repainted;
        for (const QRect& rect : std::as_const(spy.regionRects)) repainted += rect;
        qint64 area = 0;
        for (const QRect& rect : repainted) area += qint64(rect.width()) * rect.height();
        const qint64 whole = qint64(view.width()) * view.height();
        QVERIFY2(area > 0, qPrintable(label));
        QVERIFY2(area * 4 < whole, qPrintable(QStringLiteral("%1: repainted %2 of %3 px").arg(label).arg(area).arg(whole)));
        QVERIFY2(stalePixels(view) == 0, qPrintable(label));
    }

    // ---- 外観を切り替えて試す（appearanceVariantsLeaveNoStalePixels） ----
    // 行の種類: 0〜3 は将棋盤・駒台・対局者情報・背景の見本（BoardAppearanceCatalog::Component の順）、
    // 4 は駒、5 は駒の大きさ・影・木目
    static constexpr int kComponentKinds = 4;
    static constexpr int kPieceKind = 4;
    static constexpr int kVisualsKind = 5;
    static constexpr int kPieceScales[] = {90, 100, 112};
    static constexpr int kVisualsCount = 12;  // 駒の大きさ3通り×影の有無×木目の有無

    BoardColors m_defaultColors;
    BoardVisuals m_defaultVisuals;
    QString m_defaultPieceStyle;
    std::unique_ptr<BoardUi> m_appearanceBoard;  // 行ごとに作り直さず使い回す盤

    static BoardVisuals visualsVariant(BoardVisuals visuals, int index)
    {
        visuals.pieceScale = kPieceScales[index / 4];
        visuals.pieceShadow = (index / 2) % 2 == 0;
        visuals.woodGrain = index % 2 == 0;
        visuals.standWoodGrain = visuals.woodGrain;
        return visuals;
    }

    /// 外観を既定に戻してから、行で指定された1つだけを変える
    void applyAppearance(int kind, int index) const
    {
        BoardColors colors = m_defaultColors;
        BoardVisuals visuals = m_defaultVisuals;
        QString style = m_defaultPieceStyle;
        if (kind == kPieceKind) {
            style = PieceStyleCatalog::ids().at(index);
        } else if (kind == kVisualsKind) {
            visuals = visualsVariant(visuals, index);
        } else {
            const auto component = static_cast<BoardAppearanceCatalog::Component>(kind);
            BoardAppearanceCatalog::apply(component, BoardAppearanceCatalog::samples(component).at(index),
                                          colors, visuals);
        }
        PieceImageProvider::instance().setStyle(style);
        BoardAppearance::instance().setColors(colors);
        BoardAppearance::instance().setVisuals(visuals);
    }

    /// 外観を試す盤。向きが変わるときだけ作り直す（2枚を同時に出すと画面の上で重なる）
    BoardUi& appearanceBoard(bool flipped)
    {
        if (!m_appearanceBoard || m_appearanceBoard->view.flipMode() != flipped) {
            m_appearanceBoard.reset();
            m_appearanceBoard = std::make_unique<BoardUi>(sfen(), flipped);
            QObject::connect(&m_appearanceBoard->gc, &ShogiGameController::endDragSignal,
                             &m_appearanceBoard->view, &ShogiView::endDrag);
        }
        return *m_appearanceBoard;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        m_defaultColors = BoardAppearance::instance().colors();
        m_defaultVisuals = BoardAppearance::instance().visuals();
        m_defaultPieceStyle = PieceImageProvider::instance().style();
    }

    void cleanupTestCase()
    {
        m_appearanceBoard.reset();
        PieceImageProvider::instance().setStyle(m_defaultPieceStyle);
        BoardAppearance::instance().setColors(m_defaultColors);
        BoardAppearance::instance().setVisuals(m_defaultVisuals);
    }

    void dragRepaintsOnlyAroundPiece()
    {
        BoardUi ui(sfen(), false);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));

        // 7七の銀をつまみ、盤の上を少しずつ動かす。
        // クリックを直接送っているため、つまんだ位置はマウスカーソルの位置と一致しない。
        // 最初の移動は計測から外す。
        ui.controller->onLeftClick(QPoint(7, 7));
        QVERIFY(ui.highlightCount() > 0);
        const QPointF start(ui.view.width() * 0.4, ui.view.height() * 0.5);
        ui.moveTo(start);

        PaintSpy spy;
        ui.view.installEventFilter(&spy);
        const QSize fs = ui.view.fieldSize();
        for (int i = 1; i < 40; ++i)
            ui.moveTo(start + QPointF(i * 4, i * 2));
        ui.view.removeEventFilter(&spy);

        QVERIFY(!spy.rects.isEmpty());
        for (const QRect& rect : std::as_const(spy.rects)) {
            QVERIFY2(rect.width() <= fs.width() * 2 && rect.height() <= fs.height() * 2,
                     qPrintable(QStringLiteral("repaint rect %1x%2 is larger than the dragged piece")
                                    .arg(rect.width()).arg(rect.height())));
        }
        ui.controller->onRightClick(QPoint(7, 7));
    }

    // 駒をつまむと、選択・合法な移動先・つまんだマス・持ち上げた駒のマスだけを描き直す
    void pickRepaintsOnlyChangedSquares()
    {
        BoardUi ui(sfen(), false);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));
        QCoreApplication::processEvents();

        PaintSpy spy;
        ui.view.installEventFilter(&spy);
        ui.controller->onLeftClick(QPoint(7, 7));
        QCoreApplication::processEvents();
        ui.view.removeEventFilter(&spy);
        QVERIFY(ui.highlightCount() > 2);

        // 銀の選択・移動先4マス・つまんだマス・持ち上げた駒で、マス10個分に収まる
        QVERIFY(!spy.regionRects.isEmpty());
        QRegion repainted;
        for (const QRect& rect : std::as_const(spy.regionRects)) repainted += rect;
        qint64 area = 0;
        for (const QRect& rect : repainted) area += qint64(rect.width()) * rect.height();
        const QSize fs = ui.view.fieldSize();
        QVERIFY2(area < qint64(fs.width()) * fs.height() * 10,
                 qPrintable(QStringLiteral("repainted %1 px for %2 px squares")
                                .arg(area).arg(fs.width() * fs.height())));
        ui.controller->onLeftClick(QPoint(7, 7));
    }

    // 変わらない部分の画像を使い回しても、新しく作った盤面と同じ絵になる
    void staticLayerFollowsBoardChanges()
    {
        const BoardColors originalColors = BoardAppearance::instance().colors();
        const BoardVisuals originalVisuals = BoardAppearance::instance().visuals();
        BoardUi ui(sfen(), false);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));
        QVERIFY(sameAsNewBoard(ui.view));

        ui.view.setFlipMode(true);
        QVERIFY2(sameAsNewBoard(ui.view), "flip");

        ui.view.setSquareSize(ui.view.squareSize() + 7);
        ui.view.setFixedSize(ui.view.sizeHint());
        QVERIFY2(sameAsNewBoard(ui.view), "square size");

        BoardColors colors = originalColors;
        colors.background = QColor(40, 60, 90);
        colors.board = QColor(200, 150, 90);
        colors.stand = QColor(120, 80, 40);
        colors.grid = QColor(20, 20, 20);
        BoardAppearance::instance().setColors(colors);
        QVERIFY2(sameAsNewBoard(ui.view), "colors");

        BoardVisuals visuals = originalVisuals;
        visuals.woodGrain = !visuals.woodGrain;
        visuals.standWoodGrain = !visuals.standWoodGrain;
        BoardAppearance::instance().setVisuals(visuals);
        QVERIFY2(sameAsNewBoard(ui.view), "visuals");

        BoardAppearance::instance().setColors(originalColors);
        BoardAppearance::instance().setVisuals(originalVisuals);
    }

    void dragLeavesNoTrails_data()
    {
        QTest::addColumn<bool>("flipped");
        QTest::addColumn<QPoint>("from");
        QTest::newRow("board") << false << QPoint(7, 7);
        QTest::newRow("stand") << false << QPoint(10, 6);
        QTest::newRow("board-flipped") << true << QPoint(7, 7);
        QTest::newRow("stand-flipped") << true << QPoint(10, 6);
    }

    void dragLeavesNoTrails()
    {
        QFETCH(bool, flipped);
        QFETCH(QPoint, from);
        BoardUi ui(sfen(), flipped);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));

        ui.controller->onLeftClick(from);
        QCoreApplication::processEvents();
        // 速さを変えながら盤・駒台・座標・対局者情報の上を動かす
        const QPointF center(ui.view.width() / 2.0, ui.view.height() / 2.0);
        const qreal radius = qMin(ui.view.width(), ui.view.height()) * 0.45;
        for (int i = 0; i < 300; ++i) {
            const qreal angle = i * (0.03 + 0.05 * (i % 7));
            ui.moveTo(center + QPointF(qCos(angle) * radius * (0.3 + 0.07 * (i % 11)),
                                       qSin(angle * 1.3) * radius));
        }
        QCOMPARE(stalePixels(ui), 0);
        ui.controller->onRightClick(from);
    }

    // 棋譜をたどるたび・エンジンが指すたびに呼ばれる applyBoardAndRender は、盤もマスの大きさも
    // 駒の画像も変わらなければ盤を描き直さない。別の盤を渡したときは全体を描き直す
    void applyBoardAndRenderRepaintsOnlyWhenNeeded()
    {
        BoardUi ui(sfen(), false);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));
        QCoreApplication::processEvents();

        PaintSpy spy;
        ui.view.installEventFilter(&spy);
        ui.view.applyBoardAndRender(ui.gc.board());
        QCoreApplication::processEvents();
        QVERIFY(spy.rects.isEmpty());

        ShogiBoard other;
        other.setSfen(sfen());
        ui.view.applyBoardAndRender(&other);
        QCoreApplication::processEvents();
        ui.view.removeEventFilter(&spy);
        QVERIFY(!spy.rects.isEmpty());
        QCOMPARE(spy.rects.last(), ui.view.rect());
        ui.view.applyBoardAndRender(ui.gc.board());
    }

    // 局面を丸ごと替えたとき（棋譜をたどる・待った・読み込みなど）は、画面に出ている駒と違うマスだけを
    // 描き直し、それでも全体を描き直した画面と一致する
    void boardResetRepaintsOnlyChangedSquares()
    {
        BoardUi ui(sfen(), false);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));
        QCoreApplication::processEvents();
        // 7七の銀を6六へ上がった局面
        const QString after = QStringLiteral(
            "ln1g3nl/1r1s1kg2/p2ppp1pp/2p3p2/1p7/2PSP1P2/PP1P1P2P/2G1G2R1/LN2K2NL w B2Sb3p 42");

        PaintSpy spy;
        ui.view.installEventFilter(&spy);
        ui.gc.board()->setSfen(after);
        QCoreApplication::processEvents();
        ui.view.removeEventFilter(&spy);
        QRegion repainted;
        for (const QRect& rect : std::as_const(spy.regionRects)) repainted += rect;
        // 手番が替わる局面なので、対局者のカード（手番の印）は描き直してよい
        for (const bool black : {true, false}) {
            const QRect card = ui.view.chrome()->card(black ? Turn::Black : Turn::White)->geometry();
            repainted -= QRect(ui.view.chrome()->mapTo(&ui.view, card.topLeft()), card.size());
        }
        qint64 area = 0;
        for (const QRect& rect : repainted) area += qint64(rect.width()) * rect.height();
        const QSize fs = ui.view.fieldSize();
        QVERIFY(area > 0);
        QVERIFY2(area < qint64(fs.width()) * fs.height() * 4,
                 qPrintable(QStringLiteral("repainted %1 px for %2 px squares")
                                .arg(area).arg(fs.width() * fs.height())));
        QCOMPARE(stalePixels(ui), 0);

        // 戻す
        ui.gc.board()->setSfen(sfen());
        QCoreApplication::processEvents();
        QCOMPARE(stalePixels(ui), 0);

        // 全部の駒を駒箱へ戻した後でも、違うマスを描き直す
        ui.gc.board()->resetGameBoard();
        QCoreApplication::processEvents();
        ui.gc.board()->setSfen(sfen());
        QCoreApplication::processEvents();
        QCOMPARE(stalePixels(ui), 0);

        // 駒を持ち上げている途中で局面が替わる
        ui.controller->onLeftClick(QPoint(7, 7));
        QCoreApplication::processEvents();
        ui.gc.board()->setSfen(after);
        QCoreApplication::processEvents();
        QCOMPARE(stalePixels(ui), 0);

        // 持駒が違う局面
        ui.gc.board()->setSfen(QStringLiteral(
            "ln1g3nl/1r1s1kg2/p2ppp1pp/2p3p2/1p7/2PSP1P2/PP1P1P2P/2G1G2R1/LN2K2NL b RB2S2Pb3pg 43"));
        QCoreApplication::processEvents();
        QCOMPARE(stalePixels(ui), 0);
    }

    // 着手や局面編集で盤・駒台・駒箱が変わったとき、変わった所だけを描き直しても
    // 全体を描き直した画面と一致する（ShogiBoard の変更通知だけで描き直せること）
    void boardChangesLeaveNoStalePixels()
    {
        BoardUi ui(sfen(), false);
        QObject::connect(&ui.gc, &ShogiGameController::endDragSignal, &ui.view, &ShogiView::endDrag);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));
        QCoreApplication::processEvents();

        // 対局の着手: 角打ち・歩の突き・歩で取る・歩打ち
        const QList<QPair<QPoint, QPoint>> game = {
            {QPoint(10, 6), QPoint(5, 5)}, {QPoint(8, 5), QPoint(8, 6)},
            {QPoint(8, 7), QPoint(8, 6)}, {QPoint(11, 9), QPoint(8, 5)},
        };
        for (const auto& [from, to] : game) {
            ui.controller->onLeftClick(from);
            QCoreApplication::processEvents();
            ui.controller->onLeftClick(to);
            QPoint f = from, t = to;
            const bool ok = ui.gc.validateAndMove(f, t, {}).has_value();
            QVERIFY(ok);
            ui.controller->onMoveApplied(f, t, ok);
            QCoreApplication::processEvents();
            QCOMPARE(stalePixels(ui), 0);
        }
        QCOMPARE(ui.gc.board()->pieceStandCount(Piece::BlackPawn), 1);
        QCOMPARE(ui.gc.board()->pieceStandCount(Piece::WhitePawn), 2);

        // 局面編集: 盤→駒台、駒台→相手の駒台、駒箱→盤、盤→駒箱、成りの切り替え
        ui.view.setPositionEditMode(true);
        ui.route.editing = true;
        QCoreApplication::processEvents();
        // 5五に打った角を駒箱へ入れてから、別のマスへ出す
        int bishopRank = 0;
        for (int rank = 1; rank <= 8 && bishopRank == 0; ++rank) {
            if (ui.gc.board()->pieceCharacter(BoardConstants::kPieceBoxFile, rank) == Piece::BlackBishop)
                bishopRank = rank;
        }
        QVERIFY(bishopRank > 0);
        const QPoint box(BoardConstants::kPieceBoxFile, bishopRank);
        const QList<QPair<QPoint, QPoint>> edits = {
            {QPoint(9, 7), QPoint(10, 1)}, {QPoint(10, 1), QPoint(11, 9)},
            {QPoint(5, 5), box}, {box, QPoint(5, 4)},
        };
        for (const auto& [from, to] : edits) {
            ui.view.startDrag(from);
            QVERIFY2(ui.gc.editPosition(from, to, ui.view.pieceBoxSide()),
                     qPrintable(QStringLiteral("edit %1,%2 -> %3,%4").arg(from.x()).arg(from.y())
                                    .arg(to.x()).arg(to.y())));
            ui.view.endDrag();
            QCoreApplication::processEvents();
            QCOMPARE(stalePixels(ui), 0);
        }
        ui.gc.switchPiecePromotionStatusOnRightClick(7, 7);
        QCoreApplication::processEvents();
        QCOMPARE(stalePixels(ui), 0);
    }

    // 盤を回転して戻したとき・拡大して縮小し戻したときは、盤面の変わらない部分の画像を
    // 作り直さずに使い回す。使い回した画像でも、新しく作った盤と同じ絵になる
    void staticLayerReusedWhenSwitchingBack()
    {
        BoardUi ui(sfen(), false);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));
        settle();
        const auto repaintAndKey = [&ui] {
            ui.view.repaint();
            return ui.view.m_staticLayer.cacheKey();
        };
        const qint64 normal = repaintAndKey();
        QVERIFY(normal != 0);

        ui.view.setFlipMode(true);
        const qint64 flipped = repaintAndKey();
        QVERIFY(flipped != normal);
        QVERIFY(sameAsNewBoard(ui.view));
        ui.view.setFlipMode(false);
        QCOMPARE(repaintAndKey(), normal);
        QVERIFY(sameAsNewBoard(ui.view));
        ui.view.setFlipMode(true);
        QCOMPARE(repaintAndKey(), flipped);
        QVERIFY(sameAsNewBoard(ui.view));
        ui.view.setFlipMode(false);
        QCOMPARE(repaintAndKey(), normal);

        const int square = ui.view.squareSize();
        ui.view.setSquareSize(square + 4);
        ui.view.setFixedSize(ui.view.sizeHint());
        settle();
        const qint64 enlarged = repaintAndKey();
        QVERIFY(enlarged != normal);
        QVERIFY(sameAsNewBoard(ui.view));
        ui.view.setSquareSize(square);
        ui.view.setFixedSize(ui.view.sizeHint());
        settle();
        QCOMPARE(repaintAndKey(), normal);
        QVERIFY(sameAsNewBoard(ui.view));
        QCOMPARE(stalePixels(ui), 0);

        // 配色が変わったときは、どちらの画像も使わずに作り直す
        BoardColors colors = BoardAppearance::instance().colors();
        const BoardColors original = colors;
        colors.board = QColor(200, 160, 100);
        BoardAppearance::instance().setColors(colors);
        const qint64 recolored = repaintAndKey();
        QVERIFY(recolored != normal && recolored != flipped && recolored != enlarged);
        QVERIFY(sameAsNewBoard(ui.view));
        BoardAppearance::instance().setColors(original);
        settle();
    }

    // 読み筋ウィンドウ（対局中・検討中・棋譜解析・詰将棋生成で共通）で手を進め・戻しても、
    // 盤は変わった所（駒・持駒・直前の指し手の強調・手番の表示）だけを描き直す
    void pvBoardWindowRepaintsOnlyChangedSquares()
    {
        const QStringList positions = positionsAfterPvMoves();
        QCOMPARE(positions.size(), 5);
        PvBoardDialog dialog(validSfen(), pvMoves());
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* view = dialog.findChild<ShogiView*>();
        auto* viewer = dialog.findChild<BoardViewerWidget*>();
        QVERIFY(view != nullptr && viewer != nullptr);
        settle();

        int ply = 0;
        const QList<QPair<const char*, int>> steps = {
            {"goForward", 1}, {"goForward", 2}, {"goForward", 3}, {"goForward", 4},
            {"goBack", 3}, {"goFirst", 0}, {"goLast", 4},
        };
        for (const auto& step : steps) {
            const char* slot = step.first;  // ラムダで捕捉するため（構造化束縛の捕捉は C++20）
            verifyPartialStep(*view, [&] { QVERIFY(QMetaObject::invokeMethod(viewer, slot)); },
                              QStringLiteral("%1 from ply %2").arg(QLatin1String(slot)).arg(ply));
            if (QTest::currentTestFailed()) return;
            ply = step.second;
            QCOMPARE(view->board()->position().boardSfen(), positions.at(ply).section(QLatin1Char(' '), 0, 0));
        }
    }

    // 局面集ビューアで局面を進め・戻しても、盤は変わった所だけを描き直す
    void sfenCollectionRepaintsOnlyChangedSquares()
    {
        const QStringList positions = positionsAfterPvMoves();
        QCOMPARE(positions.size(), 5);
        // 読めない局面があると、局面集ビューアがエラーを出して止まる
        for (const QString& position : positions)
            QVERIFY2(SfenValidationService::validate(position).valid, qPrintable(position));
        QTemporaryDir dir;
        QFile file(dir.filePath(QStringLiteral("positions.sfen")));
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        file.write(positions.join(QLatin1Char('\n')).toUtf8());
        file.close();
        // 最近使ったファイルから開く（ファイルを選ぶ画面を出さずに読み込む）
        GameSettings::sfenCollectionRecentFiles.set({file.fileName()});
        SfenCollectionDialog dialog;
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* view = dialog.findChild<ShogiView*>();
        auto* viewer = dialog.findChild<BoardViewerWidget*>();
        auto* menu = dialog.findChild<QMenu*>(QStringLiteral("sfenCollectionRecentMenu"));
        QVERIFY(view != nullptr && viewer != nullptr && menu != nullptr && !menu->actions().isEmpty());
        menu->actions().constFirst()->trigger();
        settle();
        QCOMPARE(stalePixels(*view), 0);
        QCOMPARE(view->board()->position().boardSfen(), positions.at(0).section(QLatin1Char(' '), 0, 0));

        int index = 0;
        const QList<QPair<const char*, int>> steps = {
            {"goForward", 1}, {"goForward", 2}, {"goForward", 3}, {"goForward", 4},
            {"goBack", 3}, {"goFirst", 0}, {"goLast", 4},
        };
        for (const auto& step : steps) {
            const char* slot = step.first;  // ラムダで捕捉するため（構造化束縛の捕捉は C++20）
            verifyPartialStep(*view, [&] { QVERIFY(QMetaObject::invokeMethod(viewer, slot)); },
                              QStringLiteral("%1 from %2").arg(QLatin1String(slot)).arg(index));
            if (QTest::currentTestFailed()) return;
            index = step.second;
            QCOMPARE(view->board()->position().boardSfen(), positions.at(index).section(QLatin1Char(' '), 0, 0));
        }
        GameSettings::sfenCollectionRecentFiles.set({});
    }

    // 対局画面の外観（将棋盤・駒台・対局者情報・背景の見本、駒、駒の大きさ・影・木目）を1つずつ変え、
    // 外観の変更直後、着手と時計・手番の更新、局面の差し替え、ドラッグで、変わった所だけを
    // 描き直した画面が全体を描き直した画面と一致することを、通常と反転の向きで確かめる。
    // 盤を使い回すので、ほかのテストの盤と画面の上で重ならないよう最後に置く。
    void appearanceVariantsLeaveNoStalePixels_data()
    {
        QTest::addColumn<bool>("flipped");
        QTest::addColumn<int>("kind");
        QTest::addColumn<int>("index");
        static const char* const kComponentTags[] = {"board", "stand", "information", "background"};
        const QStringList styles = PieceStyleCatalog::ids();
        for (const bool flipped : {false, true}) {
            const char* suffix = flipped ? "-flipped" : "";
            for (int kind = 0; kind < kComponentKinds; ++kind) {
                const auto component = static_cast<BoardAppearanceCatalog::Component>(kind);
                const auto count = BoardAppearanceCatalog::samples(component).size();
                for (int index = 0; index < count; ++index)
                    QTest::addRow("%s-%02d%s", kComponentTags[kind], index, suffix) << flipped << kind << index;
            }
            for (int index = 0; index < styles.size(); ++index)
                QTest::addRow("piece-%s%s", qPrintable(styles.at(index)), suffix) << flipped << kPieceKind << index;
            for (int index = 0; index < kVisualsCount; ++index) {
                const BoardVisuals visuals = visualsVariant(BoardVisuals(), index);
                QTest::addRow("scale%d-%s-%s%s", visuals.pieceScale, visuals.pieceShadow ? "shadow" : "noshadow",
                              visuals.woodGrain ? "grain" : "nograin", suffix)
                    << flipped << kVisualsKind << index;
            }
        }
    }

    void appearanceVariantsLeaveNoStalePixels()
    {
        QFETCH(bool, flipped);
        QFETCH(int, kind);
        QFETCH(int, index);
        BoardUi& ui = appearanceBoard(flipped);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));

        // 前の行で動かした局面から始めても同じになるよう、局面と表示を戻してから外観を変える
        ui.gc.board()->setSfen(sfen());
        ui.controller->showMoveHighlights(QPoint(3, 3), QPoint(3, 4));
        ui.view.chrome()->card(Turn::Black)->setClockText(QStringLiteral("00:08:31"));
        ui.view.chrome()->card(Turn::White)->setClockText(QStringLiteral("00:07:12"));
        applyAppearance(kind, index);
        settle();
        QCOMPARE(stalePixels(ui), 0);
        // 使い回す画像（盤面の変わらない部分・駒）が古いままだと、全体を描き直しても同じ誤りが
        // 出るので、新しく作った盤とも比べる
        QVERIFY(sameAsNewBoard(ui.view));

        // 着手（角打ち・歩の突き・歩で取る・歩打ち）と、時計・手番・直前の指し手の表示の更新
        int ply = 41;
        const QList<QPair<QPoint, QPoint>> game = {
            {QPoint(10, 6), QPoint(5, 5)}, {QPoint(8, 5), QPoint(8, 6)},
            {QPoint(8, 7), QPoint(8, 6)}, {QPoint(11, 9), QPoint(8, 5)},
        };
        for (const auto& [from, to] : game) {
            ui.controller->onLeftClick(from);
            settle();
            ui.controller->onLeftClick(to);
            QPoint f = from, t = to;
            const bool ok = ui.gc.validateAndMove(f, t, {}).has_value();
            QVERIFY(ok);
            ++ply;
            ui.controller->onMoveApplied(f, t, ok);
            ui.controller->showMoveHighlights(f, t);
            const QString seconds = QStringLiteral("%1").arg(ply % 60, 2, 10, QLatin1Char('0'));
            ui.view.chrome()->card(Turn::Black)->setClockText(QStringLiteral("00:08:") + seconds);
            ui.view.chrome()->card(Turn::White)->setClockText(QStringLiteral("00:07:") + seconds);
            settle();
            QCOMPARE(stalePixels(ui), 0);
        }

        // 局面を丸ごと替える（棋譜をたどる・待ったと同じ経路）
        ui.gc.board()->setSfen(QStringLiteral(
            "ln1g3nl/1r1s1kg2/p2ppp1pp/2p3p2/1p7/2PSP1P2/PP1P1P2P/2G1G2R1/LN2K2NL w B2Sb3p 42"));
        settle();
        QCOMPARE(stalePixels(ui), 0);
        ui.gc.board()->setSfen(sfen());
        settle();
        QCOMPARE(stalePixels(ui), 0);

        // 盤・駒台・座標・対局者情報の上をドラッグしてから取り消す
        ui.controller->onLeftClick(QPoint(7, 7));
        settle();
        const QPointF center(ui.view.width() / 2.0, ui.view.height() / 2.0);
        const qreal radius = qMin(ui.view.width(), ui.view.height()) * 0.45;
        for (int i = 0; i < 40; ++i) {
            const qreal angle = i * (0.03 + 0.05 * (i % 7));
            ui.moveTo(center + QPointF(qCos(angle) * radius * (0.3 + 0.07 * (i % 11)),
                                       qSin(angle * 1.3) * radius));
        }
        settle();
        QCOMPARE(stalePixels(ui), 0);
        ui.controller->onRightClick(QPoint(7, 7));
        settle();
        QCOMPARE(stalePixels(ui), 0);
    }
};

QTEST_MAIN(TestBoardDragRepaint)
#include "tst_board_drag_repaint.moc"
