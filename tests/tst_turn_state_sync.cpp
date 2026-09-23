#include <QtTest>
#include <QTemporaryDir>
#include <QFrame>

#include "elidelabel.h"
#include "boardappearance.h"
#include "sfenpositiontracer.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"
#include "shogiview.h"
#include "shogiviewhighlighting.h"
#include "turnmanager.h"
#include "turnstatesyncservice.h"

class TestTurnStateSync : public QObject
{
    Q_OBJECT

    QTemporaryDir m_config;
    TurnStateSyncService m_sync;

    void configure(ShogiGameController& gc, ShogiView& view, ShogiClock& clock,
                   QObject& turnParent, const bool& gameActive)
    {
        TurnStateSyncService::Deps deps;
        deps.gameController = &gc;
        deps.shogiView = &view;
        deps.turnManagerParent = &turnParent;
        deps.isGameActivelyInProgress = [&gameActive]() { return gameActive; };
        deps.updateTurnStatus = [&view, &clock](int player) {
            clock.setCurrentPlayer(player);
            view.setActiveSide(player == 1);
        };
        deps.onTurnManagerCreated = [this](TurnManager* tm) {
            connect(tm, &TurnManager::changed, this, &TestTurnStateSync::onTurnChanged);
        };
        m_sync.updateDeps(deps);
    }

    static void verifyTurn(ShogiView& view, ShogiGameController& gc, bool blackTurn)
    {
        QCOMPARE(gc.currentPlayer(), blackTurn ? ShogiGameController::Player1
                                              : ShogiGameController::Player2);
        QCOMPARE(view.board()->currentPlayer(), blackTurn ? Turn::Black : Turn::White);
        QCOMPARE(view.highlighting()->blackActive(), blackTurn);

        const auto* blackLabel = view.findChild<QLabel*>(QStringLiteral("turnLabelBlack"));
        const auto* whiteLabel = view.findChild<QLabel*>(QStringLiteral("turnLabelWhite"));
        QVERIFY(blackLabel);
        QVERIFY(whiteLabel);
        QCOMPARE(!blackLabel->isHidden(), blackTurn);
        QCOMPARE(!whiteLabel->isHidden(), !blackTurn);

        auto* activeCard = view.findChild<QFrame*>(blackTurn ? QStringLiteral("blackPlayerCard")
                                                            : QStringLiteral("whitePlayerCard"));
        auto* inactiveCard = view.findChild<QFrame*>(blackTurn ? QStringLiteral("whitePlayerCard")
                                                              : QStringLiteral("blackPlayerCard"));
        QVERIFY(activeCard);
        QVERIFY(inactiveCard);
        QCOMPARE(activeCard->palette().color(QPalette::Window), view.boardColors().cardBackground);
        QCOMPARE(inactiveCard->palette().color(QPalette::Window), view.boardColors().cardBackground);
        // 終局後のナビゲーションでも手番バッジとカード全周の枠線が一緒に移る。
        for (auto* card : {activeCard, inactiveCard}) {
            const QImage image = card->grab().toImage();
            const QColor borderColor = card == activeCard ? view.boardColors().activeCardBorder : view.boardColors().cardBorder;
            for (const QPoint& edge : {QPoint(0, image.height() / 2),
                                       QPoint(image.width() - 1, image.height() / 2),
                                       QPoint(image.width() / 2, 0),
                                       QPoint(image.width() / 2, image.height() - 1)}) {
                QCOMPARE(image.pixelColor(edge), borderColor);
            }
        }
        const auto* activeTurn = blackTurn ? blackLabel : whiteLabel;
        QCOMPARE(activeTurn->palette().color(QPalette::Window), view.boardColors().turnBackground);
        QCOMPARE(view.blackNameLabel()->palette().color(QPalette::WindowText),
                 view.whiteNameLabel()->palette().color(QPalette::WindowText));
    }

private slots:
    void onTurnChanged(ShogiGameController::Player player)
    {
        m_sync.onTurnManagerChanged(player);
    }

    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
    }

    void navigationAfterGameEnd_data()
    {
        QTest::addColumn<bool>("whiteStart");
        QTest::addColumn<bool>("flipped");
        QTest::newRow("black-start") << false << false;
        QTest::newRow("black-start-flipped") << false << true;
        QTest::newRow("white-start") << true << false;
        QTest::newRow("white-start-flipped") << true << true;
    }

    void navigationAfterGameEnd()
    {
        QFETCH(bool, whiteStart);
        QFETCH(bool, flipped);
        QString initialSfen = SfenUtils::hirateSfen();
        if (whiteStart) initialSfen.replace(QStringLiteral(" b "), QStringLiteral(" w "));
        const QStringList moves = whiteStart
            ? QStringList{QStringLiteral("3c3d"), QStringLiteral("7g7f"), QStringLiteral("8c8d")}
            : QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")};
        const QStringList sfens = SfenPositionTracer::buildSfenRecord(initialSfen, moves, false);
        QCOMPARE(sfens.size(), 4);

        ShogiGameController gc;
        gc.newGame(initialSfen);
        ShogiView view;
        view.setBoard(gc.board());
        view.setFlipMode(flipped);
        view.show();
        ShogiClock clock;
        QObject turnParent;
        bool gameActive = true;
        configure(gc, view, clock, turnParent, gameActive);
        m_sync.setCurrentTurn();

        // ライブ対局から終局後の閲覧へ。終局時の表示維持も実環境と揃える。
        gc.board()->setSfen(sfens.last());
        gc.setCurrentPlayer(whiteStart ? ShogiGameController::Player1 : ShogiGameController::Player2);
        m_sync.setCurrentTurn();
        gameActive = false;
        view.setGameOverStyleLock(true);
        view.setUiMuted(true);

        // 先頭・末尾へのジャンプ、一手ずつの前後移動、同じ局面の再選択。
        // 終局行は最終局面と同じ手番を維持する。
        for (const int ply : {0, 1, 2, 3, 3, 2, 1, 0, 3, 0}) {
            gc.board()->setSfen(sfens.at(ply));
            view.applyBoardAndRender(gc.board());
            m_sync.setCurrentTurn();
            verifyTurn(view, gc, (ply % 2 == 0) != whiteStart);
        }
    }

    void customColors_followTurnsAndWarnings()
    {
        ShogiGameController gc;
        QString initialSfen = SfenUtils::hirateSfen();
        gc.newGame(initialSfen);
        ShogiView view;
        view.setBoard(gc.board());
        view.resize(view.sizeHint());
        view.setBlackPlayerName(QStringLiteral("先手"));
        view.setWhitePlayerName(QStringLiteral("後手"));
        view.show();
        BoardColors custom;
        custom.cardBackground = QColor("#dde6f0");
        custom.cardBorder = QColor("#647589");
        custom.activeCardBorder = QColor("#235da1");
        custom.turnBackground = QColor("#315e97");
        custom.turnBorder = QColor("#112f58");
        custom.turnText = QColor("#fff4ba");
        custom.nameBackground = QColor("#fae8ce");
        custom.nameBorder = QColor("#af7e48");
        custom.nameText = QColor("#593f2b");
        custom.clockBackground = QColor("#e4efd3");
        custom.clockBorder = QColor("#788f54");
        custom.clockText = QColor("#345b3d");
        custom.clockWarningText = QColor("#996722");
        custom.clockCriticalText = QColor("#aa2961");
        BoardAppearance::instance().setColors(custom);
        ShogiClock clock;
        QObject turnParent;
        const bool gameActive = true;
        configure(gc, view, clock, turnParent, gameActive);
        for (bool black : {true, false}) {
            gc.setCurrentPlayer(black ? ShogiGameController::Player1 : ShogiGameController::Player2);
            m_sync.setCurrentTurn();
            verifyTurn(view, gc, black);
            const auto* badge = view.findChild<QLabel*>(black ? "turnLabelBlack" : "turnLabelWhite");
            QCOMPARE(badge->palette().color(QPalette::WindowText), custom.turnText);
            for (auto* label : {view.blackNameLabel(), view.whiteNameLabel()}) {
                QCOMPARE(label->palette().color(QPalette::WindowText), custom.nameText);
                const QImage image = label->grab().toImage();
                QCOMPARE(image.pixelColor(0, image.height() / 2), custom.nameBorder);
                QCOMPARE(image.pixelColor(image.width() - 3, 2), custom.nameBackground);
            }
            auto* activeClock = black ? view.blackClockLabel() : view.whiteClockLabel();
            auto* inactiveClock = black ? view.whiteClockLabel() : view.blackClockLabel();
            QCOMPARE(activeClock->palette().color(QPalette::WindowText), custom.clockText);
            const QImage clockImage = activeClock->grab().toImage();
            QCOMPARE(clockImage.pixelColor(0, clockImage.height() / 2), custom.clockBorder);
            QCOMPARE(clockImage.pixelColor(clockImage.width() - 3, 2), custom.clockBackground);
            view.setUrgencyVisuals(ShogiView::Urgency::Warn10);
            QCOMPARE(activeClock->palette().color(QPalette::WindowText), custom.clockWarningText);
            view.setUrgencyVisuals(ShogiView::Urgency::Warn5);
            QCOMPARE(activeClock->palette().color(QPalette::WindowText), custom.clockCriticalText);
            custom.clockCriticalText = QColor("#bc2049");
            BoardAppearance::instance().setColors(custom);
            QCOMPARE(activeClock->palette().color(QPalette::WindowText), custom.clockCriticalText);
            QCOMPARE(inactiveClock->palette().color(QPalette::WindowText), custom.clockText);
        }
        // 半透明の名前背景には、カードの色が一度だけ合成される。
        custom.nameBackground = QColor(255, 255, 255, 128);
        BoardAppearance::instance().setColors(custom);
        const auto* name = view.blackNameLabel();
        const QColor blended = view.grab().toImage().pixelColor(name->x() + name->width() - 3, name->y() + 2);
        QVERIFY(qAbs(blended.red() - (255 + custom.cardBackground.red()) / 2) <= 1);
        QVERIFY(qAbs(blended.green() - (255 + custom.cardBackground.green()) / 2) <= 1);
        QVERIFY(qAbs(blended.blue() - (255 + custom.cardBackground.blue()) / 2) <= 1);
    }

    void cleanup()
    {
        BoardAppearance::instance().setColors(BoardColors{});
    }

    void liveGameKeepsControllerTurn()
    {
        QString initialSfen = SfenUtils::hirateSfen();
        ShogiGameController gc;
        gc.newGame(initialSfen);
        ShogiView view;
        view.setBoard(gc.board());
        ShogiClock clock;
        QObject turnParent;
        const bool gameActive = true;
        configure(gc, view, clock, turnParent, gameActive);

        gc.setCurrentPlayer(ShogiGameController::Player2);
        // 同期前の古い盤面手番より、対局制御側の手番を優先する。
        gc.board()->setSfen(initialSfen);
        m_sync.setCurrentTurn();
        const auto* tm = turnParent.findChild<TurnManager*>();
        QVERIFY(tm);
        QCOMPARE(tm->toGc(), ShogiGameController::Player2);
        QCOMPARE(gc.currentPlayer(), ShogiGameController::Player2);
        QVERIFY(!view.highlighting()->blackActive());
        QVERIFY(!view.findChild<QLabel*>(QStringLiteral("turnLabelWhite"))->isHidden());
    }
};

QTEST_MAIN(TestTurnStateSync)
#include "tst_turn_state_sync.moc"
