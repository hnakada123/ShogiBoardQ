#include <QtTest>
#include <QImage>
#include <QPaintEvent>
#include <QScreen>
#include <QTemporaryDir>
#include <QWindow>
#include <QtMath>

#include "boardinteractioncontroller.h"
#include "shogigamecontroller.h"
#include "shogiview.h"

/// 駒をつまんで動かすときの部分再描画を検証する。
/// ドラッグ中は駒の周囲だけを描き直し、それでも画面が全体を描き直したときと一致すること。
class TestBoardDragRepaint : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    /// ShogiView が受け取った再描画範囲を記録する
    class PaintSpy : public QObject
    {
    public:
        QList<QRect> rects;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Paint)
                rects.append(static_cast<QPaintEvent*>(event)->rect());
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

        // 7七の歩をつまみ、盤の上を少しずつ動かす。
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
        const QImage partial = ui.screenImage();
        ui.view.repaint();
        QCoreApplication::processEvents();
        const QImage full = ui.screenImage();
        ui.controller->onRightClick(from);

        QCOMPARE(partial.size(), full.size());
        // 部分描画での半透明の合成は、全体を描いたときと丸めが1だけ違うことがある。
        int trails = 0;
        for (int y = 0; y < full.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(partial.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(full.constScanLine(y));
            for (int x = 0; x < full.width(); ++x) {
                if (qAbs(qRed(a[x]) - qRed(b[x])) > 1 || qAbs(qGreen(a[x]) - qGreen(b[x])) > 1
                    || qAbs(qBlue(a[x]) - qBlue(b[x])) > 1)
                    ++trails;
            }
        }
        QCOMPARE(trails, 0);
    }
};

QTEST_MAIN(TestBoardDragRepaint)
#include "tst_board_drag_repaint.moc"
