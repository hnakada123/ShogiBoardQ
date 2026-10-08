#include <QtTest>
#include <QSettings>
#include <QTemporaryDir>

#include "appsettings.h"
#include "boardinteractioncontroller.h"
#include "settingscommon.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"

class TestLegalMoveHighlights : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    struct BoardUi {
        ShogiGameController gc;
        ShogiView view;
        std::unique_ptr<BoardInteractionController> controller;

        explicit BoardUi(QString sfen, bool flipped = false)
        {
            gc.newGame(sfen);
            view.setBoard(gc.board());
            view.setFlipMode(flipped);
            view.setFixedSize(view.sizeHint());
            controller = std::make_unique<BoardInteractionController>(&view, &gc);
            controller->setLegalMovesVisible(true);
            QObject::connect(&view, &ShogiView::clicked, controller.get(), &BoardInteractionController::onLeftClick);
            QObject::connect(&view, &ShogiView::rightClicked, controller.get(), &BoardInteractionController::onRightClick);
            view.show();
        }

        ~BoardUi() { controller->clearAllHighlights(); }

        QList<QPoint> destinations() const
        {
            QList<QPoint> result;
            for (int i = 0; i < view.highlighting()->highlightCount(); ++i) {
                auto* hl = static_cast<ShogiView::FieldHighlight*>(view.highlight(i));
                if (hl->purpose() == ShogiView::FieldHighlight::Purpose::LegalDestination)
                    result.append(QPoint(hl->file(), hl->rank()));
            }
            return result;
        }

        void click(const QPoint& square, Qt::MouseButton button = Qt::LeftButton)
        {
            // 実際の盤・駒台のヒットテストを通してクリックする（反転にも対応）。
            for (int y = 0; y < view.height(); y += 4) {
                for (int x = 0; x < view.width(); x += 4) {
                    if (view.clickedSquare(QPoint(x, y)) == square) {
                        QTest::mouseClick(&view, button, Qt::NoModifier, QPoint(x, y));
                        return;
                    }
                }
            }
            QFAIL("Clicked square was not found");
        }
    };

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void boardMoves_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<QPoint>("from");
        QTest::addColumn<QList<QPoint>>("expected");
        QTest::addColumn<bool>("flipped");
        for (bool flipped : {false, true}) {
            const QByteArray suffix = flipped ? "-flipped" : "";
            QTest::newRow(("black-pawn" + suffix).constData())
                << SfenUtils::hirateSfen() << QPoint(7, 7) << QList<QPoint>{QPoint(7, 6)} << flipped;
            QTest::newRow(("white-pawn" + suffix).constData())
                << SfenUtils::hirateSfen().replace(" b ", " w ") << QPoint(3, 3)
                << QList<QPoint>{QPoint(3, 4)} << flipped;
            QTest::newRow(("blocked-bishop" + suffix).constData())
                << SfenUtils::hirateSfen() << QPoint(8, 8) << QList<QPoint>{} << flipped;
            QTest::newRow(("enemy-piece" + suffix).constData())
                << SfenUtils::hirateSfen() << QPoint(3, 3) << QList<QPoint>{} << flipped;
            QTest::newRow(("empty-square" + suffix).constData())
                << SfenUtils::hirateSfen() << QPoint(5, 5) << QList<QPoint>{} << flipped;
            QTest::newRow(("empty-hand" + suffix).constData())
                << SfenUtils::hirateSfen() << QPoint(10, 1) << QList<QPoint>{} << flipped;
            QTest::newRow(("forced-promotion" + suffix).constData())
                << QStringLiteral("k8/4P4/9/9/9/9/9/9/4K4 b - 1") << QPoint(5, 2)
                << QList<QPoint>{QPoint(5, 1)} << flipped;
            // 飛車で玉にピンされた銀は、飛車の筋を離れられない。
            QTest::newRow(("pinned-silver" + suffix).constData())
                << QStringLiteral("k3r4/9/9/9/9/9/9/4S4/4K4 b - 1") << QPoint(5, 8)
                << QList<QPoint>{QPoint(5, 7)} << flipped;
            QTest::newRow(("in-check" + suffix).constData())
                << QStringLiteral("k3r4/9/9/9/9/9/6P2/9/4K4 b - 1") << QPoint(3, 7)
                << QList<QPoint>{} << flipped;
            QTest::newRow(("king-escapes-check" + suffix).constData())
                << QStringLiteral("k3r4/9/9/9/9/9/9/9/4K4 b - 1") << QPoint(5, 9)
                << QList<QPoint>{QPoint(4, 8), QPoint(6, 8), QPoint(4, 9), QPoint(6, 9)} << flipped;
            // 前方の敵駒は取れるが、その奥や味方の駒があるマスへは進めない。
            QTest::newRow(("rook-capture" + suffix).constData())
                << QStringLiteral("k8/9/4p4/9/3PRP3/4P4/9/9/4K4 b - 1") << QPoint(5, 5)
                << QList<QPoint>{QPoint(5, 3), QPoint(5, 4)} << flipped;
        }
    }

    void boardMoves()
    {
        QFETCH(QString, sfen);
        QFETCH(QPoint, from);
        QFETCH(QList<QPoint>, expected);
        QFETCH(bool, flipped);
        BoardUi ui(sfen, flipped);
        const auto before = ui.gc.board()->boardData();
        const auto hand = ui.gc.board()->pieceStand();
        ui.click(from);
        QCOMPARE(ui.destinations(), expected);
        QCOMPARE(ui.gc.board()->boardData(), before);
        QCOMPARE(ui.gc.board()->pieceStand(), hand);
    }

    void drops_data()
    {
        QTest::addColumn<bool>("white");
        QTest::addColumn<bool>("flipped");
        QTest::newRow("black") << false << false;
        QTest::newRow("black-flipped") << false << true;
        QTest::newRow("white") << true << false;
        QTest::newRow("white-flipped") << true << true;
    }

    void drops()
    {
        QFETCH(bool, white);
        QFETCH(bool, flipped);
        BoardUi ui(white ? QStringLiteral("4k4/9/4p4/9/9/9/9/9/4K4 w pn 1")
                         : QStringLiteral("4k4/9/9/9/9/9/4P4/9/4K4 b PN 1"), flipped);
        ui.click(QPoint(white ? 11 : 10, white ? 9 : 1));
        auto targets = ui.destinations();
        QVERIFY(targets.contains(QPoint(4, 5)));
        for (int rank = 1; rank <= 9; ++rank) QVERIFY(!targets.contains(QPoint(5, rank))); // 二歩
        for (int file = 1; file <= 9; ++file) QVERIFY(!targets.contains(QPoint(file, white ? 9 : 1)));
        ui.click(QPoint(5, 5), Qt::RightButton);
        ui.click(QPoint(white ? 11 : 10, white ? 7 : 3));
        targets = ui.destinations();
        QVERIFY(targets.contains(QPoint(4, white ? 7 : 3)));
        for (int file = 1; file <= 9; ++file) {
            QVERIFY(!targets.contains(QPoint(file, white ? 9 : 1)));
            QVERIFY(!targets.contains(QPoint(file, white ? 8 : 2)));
        }
    }

    void pawnDropMate()
    {
        // 5二歩打ちは金の支えがあり、後手玉の退路も塞がれている。
        BoardUi ui(QStringLiteral("3lkl3/3p1p3/4G4/9/9/9/9/9/4K4 b P 1"));
        ui.click(QPoint(10, 1));
        QVERIFY(ui.destinations().contains(QPoint(4, 4)));
        QVERIFY(!ui.destinations().contains(QPoint(5, 2)));
    }

    void toggleAndCancel()
    {
        BoardUi ui(SfenUtils::hirateSfen());
        ui.controller->showMoveHighlights(QPoint(3, 3), QPoint(3, 4));
        ui.click(QPoint(7, 7));
        QCOMPARE(ui.destinations().size(), 1);
        ui.controller->setLegalMovesVisible(false);
        QVERIFY(ui.destinations().isEmpty());
        QVERIFY(!AppSettings::legalMovesVisible());
        ui.controller->setLegalMovesVisible(true);
        QCOMPARE(ui.destinations().size(), 1);
        QVERIFY(AppSettings::legalMovesVisible());
        ui.click(QPoint(7, 7));
        QVERIFY(ui.destinations().isEmpty());
        QCOMPARE(ui.view.highlighting()->highlightCount(), 2); // 直前手の色は残す
        ui.click(QPoint(7, 7));
        ui.click(QPoint(5, 5), Qt::RightButton);
        QVERIFY(ui.destinations().isEmpty());
    }

    void invalidation_data()
    {
        QTest::addColumn<QString>("operation");
        for (const char* op : {"applied", "rejected", "clear", "view-clear", "reset", "turn", "disable", "edit"})
            QTest::newRow(op) << QString::fromLatin1(op);
    }

    void invalidation()
    {
        QFETCH(QString, operation);
        BoardUi ui(SfenUtils::hirateSfen());
        ui.click(QPoint(7, 7));
        QCOMPARE(ui.destinations().size(), 1);
        if (operation == "applied" || operation == "rejected")
            ui.controller->onMoveApplied(QPoint(7, 7), QPoint(7, 6), operation == "applied");
        else if (operation == "clear") ui.controller->clearAllHighlights();
        else if (operation == "view-clear") ui.view.removeHighlightAllData();
        else if (operation == "reset") ui.gc.board()->setSfen(SfenUtils::hirateSfen());
        else if (operation == "turn") ui.gc.setCurrentPlayer(ShogiGameController::Player2);
        else if (operation == "disable") ui.controller->setMoveInputEnabled(false);
        else if (operation == "edit") ui.controller->setMode(BoardInteractionController::Mode::Edit);
        QVERIFY(ui.destinations().isEmpty());
    }

    void inputRestrictions()
    {
        BoardUi ui(SfenUtils::hirateSfen());
        ui.controller->setMode(BoardInteractionController::Mode::Edit);
        ui.click(QPoint(7, 7));
        QVERIFY(ui.destinations().isEmpty());
        ui.controller->setMode(BoardInteractionController::Mode::HumanVsEngine);
        ui.controller->setIsHumanTurnCallback([] { return false; });
        ui.click(QPoint(7, 7));
        QVERIFY(ui.destinations().isEmpty());
    }
};

QTEST_MAIN(TestLegalMoveHighlights)
#include "tst_legal_move_highlights.moc"
