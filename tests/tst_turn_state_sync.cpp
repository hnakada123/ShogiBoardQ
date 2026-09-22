#include <QtTest>
#include <QTemporaryDir>

#include "elidelabel.h"
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

        const auto* activeName = blackTurn ? view.blackNameLabel() : view.whiteNameLabel();
        const auto* inactiveName = blackTurn ? view.whiteNameLabel() : view.blackNameLabel();
        const auto* activeClock = blackTurn ? view.blackClockLabel() : view.whiteClockLabel();
        const auto* inactiveClock = blackTurn ? view.whiteClockLabel() : view.blackClockLabel();
        const auto* activeTurn = blackTurn ? blackLabel : whiteLabel;
        for (const QLabel* label : {static_cast<const QLabel*>(activeName), activeClock, activeTurn}) {
            QCOMPARE(label->palette().color(QPalette::Window), QColor(Qt::yellow));
        }
        QCOMPARE(inactiveName->palette().color(QPalette::Window), view.boardColors().background);
        QCOMPARE(inactiveClock->palette().color(QPalette::Window), view.boardColors().background);
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
