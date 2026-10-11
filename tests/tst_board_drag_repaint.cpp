#include <QtTest>
#include <QImage>
#include <QPaintEvent>
#include <QScreen>
#include <QTemporaryDir>
#include <QWindow>
#include <QtMath>

#include "boardappearance.h"
#include "boardconstants.h"
#include "boardinteractioncontroller.h"
#include "playmode.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

/// 駒をつまんで動かすときの部分再描画を検証する。
/// ドラッグ中は駒の周囲だけを描き直し、それでも画面が全体を描き直したときと一致すること。
/// 盤面の変わらない部分を描いた画像が、盤の反転・大きさ・配色・質感の変更に追従すること。
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

    struct BoardUi {
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
            // 対局中と同じく、対局者名・時計・手番表示・直前の指し手を表示する
            view.setBlackPlayerName(QStringLiteral("先手"));
            view.setWhitePlayerName(QStringLiteral("後手"));
            view.setBlackClockText(QStringLiteral("00:08:31"));
            view.setWhiteClockText(QStringLiteral("00:07:12"));
            view.setActiveSide(true);
            controller->showMoveHighlights(QPoint(3, 3), QPoint(3, 4));
            view.show();
        }

        ~BoardUi() { controller->clearAllHighlights(); }

        void moveTo(const QPointF& pos)
        {
            QMouseEvent move(QEvent::MouseMove, pos, view.mapToGlobal(pos), Qt::NoButton,
                             Qt::NoButton, Qt::NoModifier);
            QCoreApplication::sendEvent(&view, &move);
            QCoreApplication::processEvents();
        }

        QImage screenImage()
        {
            QWindow* window = view.window()->windowHandle();
            return window->screen()->grabWindow(view.window()->winId()).toImage()
                .convertToFormat(QImage::Format_ARGB32);
        }
    };

    /// 実際に画面に出ている内容と、全体を描き直した内容とで違う画素の数。
    /// 部分描画での半透明の合成は、全体を描いたときと丸めが1だけ違うことがあるため、それは数えない。
    static int stalePixels(BoardUi& ui)
    {
        const QImage partial = ui.screenImage();
        ui.view.repaint();
        QCoreApplication::processEvents();
        const QImage full = ui.screenImage();
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

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void dragRepaintsOnlyAroundPiece()
    {
        BoardUi ui(sfen(), false);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));

        // 7七の銀をつまみ、盤の上を少しずつ動かす。
        // クリックを直接送っているため、つまんだ位置はマウスカーソルの位置と一致しない。
        // 最初の移動は計測から外す。
        ui.controller->onLeftClick(QPoint(7, 7));
        QVERIFY(ui.view.highlightCount() > 0);
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
        QVERIFY(ui.view.highlightCount() > 2);

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

    // 着手や局面編集で盤・駒台・駒箱が変わったとき、変わった所だけを描き直しても
    // 全体を描き直した画面と一致する（ShogiBoard の変更通知だけで描き直せること）
    void boardChangesLeaveNoStalePixels()
    {
        BoardUi ui(sfen(), false);
        QObject::connect(&ui.gc, &ShogiGameController::endDragSignal, &ui.view, &ShogiView::endDrag);
        QVERIFY(QTest::qWaitForWindowExposed(&ui.view));
        QCoreApplication::processEvents();

        // 対局の着手: 角打ち・歩の突き・歩で取る・歩打ち
        QStringList history{sfen()};
        QList<ShogiMove> moves;
        int ply = 41;
        const QList<QPair<QPoint, QPoint>> game = {
            {QPoint(10, 6), QPoint(5, 5)}, {QPoint(8, 5), QPoint(8, 6)},
            {QPoint(8, 7), QPoint(8, 6)}, {QPoint(11, 9), QPoint(8, 5)},
        };
        for (const auto& [from, to] : game) {
            ui.controller->onLeftClick(from);
            QCoreApplication::processEvents();
            ui.controller->onLeftClick(to);
            QPoint f = from, t = to;
            QString record;
            PlayMode mode = PlayMode::HumanVsHuman;
            const bool ok = ui.gc.validateAndMove(f, t, record, mode, ply++, &history, moves);
            QVERIFY2(ok, qPrintable(record));
            ui.controller->onMoveApplied(f, t, ok);
            QCoreApplication::processEvents();
            QCOMPARE(stalePixels(ui), 0);
        }
        QCOMPARE(ui.gc.board()->pieceStandCount(Piece::BlackPawn), 1);
        QCOMPARE(ui.gc.board()->pieceStandCount(Piece::WhitePawn), 2);

        // 局面編集: 盤→駒台、駒台→相手の駒台、駒箱→盤、盤→駒箱、成りの切り替え
        ui.view.setPositionEditMode(true);
        ui.controller->setMode(BoardInteractionController::Mode::Edit);
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
};

QTEST_MAIN(TestBoardDragRepaint)
#include "tst_board_drag_repaint.moc"
