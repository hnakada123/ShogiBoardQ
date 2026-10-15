/// @file tst_csagamecoordinator.cpp
/// @brief CsaGameCoordinator の指し手進行と棋譜行の組み立てを、ローカルの偽 CSA サーバーで確かめる
///
/// 実際のソケット（QTcpServer）でログイン・対局条件・指し手・終局を流し、
/// 残り時間・手数・手番・局面列・棋譜行（recordEntryReady）と、配線を通した棋譜への追記を検証する。

#include <QtTest>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <algorithm>

#include "csagamecoordinator.h"
#include "csagameprogress.h"
#include "csagamewiring.h"
#include "gamerecordupdateservice.h"
#include "kifubranchtree.h"
#include "livegamesession.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "shogigamecontroller.h"

namespace {

/// 1接続だけ受け付ける偽 CSA サーバー
struct FakeCsaServer {
    QTcpServer server;
    QTcpSocket* peer = nullptr;
    QStringList lines;
    QByteArray buffer;

    bool listen() { return server.listen(QHostAddress::LocalHost); }
    quint16 port() const { return server.serverPort(); }
    bool accept()
    {
        if (!QTest::qWaitFor([this] { return server.hasPendingConnections(); }, 3000)) return false;
        peer = server.nextPendingConnection();
        return peer != nullptr;
    }
    void pump()
    {
        if (!peer) return;
        buffer += peer->readAll();
        qsizetype idx;
        while ((idx = buffer.indexOf('\n')) != -1) {
            lines.append(QString::fromUtf8(buffer.left(idx)));
            buffer = buffer.mid(idx + 1);
        }
    }
    bool waitForLine(const QString& text, int timeoutMs = 3000)
    {
        return QTest::qWaitFor([this, &text] {
            pump();
            return lines.contains(text);
        }, timeoutMs);
    }
    void send(const QByteArray& text)
    {
        peer->write(text + '\n');
        peer->flush();
    }
};

/// 盤・時計・局面列と司令塔をそろえ、偽サーバーと対局を始める
struct Harness {
    ShogiGameController gc;
    ShogiClock clock;
    QStringList sfenRecord;
    QList<ShogiMove> gameMoves;
    CsaGameCoordinator coordinator;
    FakeCsaServer server;

    Harness()
    {
        QString hirate = SfenUtils::hirateSfen();
        gc.newGame(hirate);
        CsaGameCoordinator::Dependencies deps;
        deps.gameController = &gc;
        deps.clock = &clock;
        deps.sfenRecord = &sfenRecord;
        deps.gameMoves = &gameMoves;
        coordinator.setDependencies(deps);
    }

    /// ログインから START まで進める（持時間 600 秒・加算 10 秒）
    bool start(const QByteArray& yourTurn, const QList<QByteArray>& positionMoves = {})
    {
        if (!server.listen()) return false;
        CsaGameCoordinator::StartOptions options;
        options.host = QStringLiteral("127.0.0.1");
        options.port = server.port();
        options.username = QStringLiteral("user");
        options.password = QStringLiteral("pass");
        options.keepAliveEnabled = false;
        options.gameLimit = 0;  // 終局後に LOGOUT せず、結果表示もモーダルにしない
        coordinator.startGame(options);
        if (!server.accept()) return false;
        if (!server.waitForLine(QStringLiteral("LOGIN user pass"))) return false;
        server.send("LOGIN:user OK");
        server.send("BEGIN Game_Summary");
        server.send("Protocol_Version:1.2");
        server.send("Protocol_Mode:Server");
        server.send("Format:Shogi 1.0");
        server.send("Game_ID:g1");
        server.send("Name+:Sente");
        server.send("Name-:Gote");
        server.send("Your_Turn:" + yourTurn);
        server.send(positionMoves.size() % 2 == 0 ? "To_Move:+" : "To_Move:-");
        server.send("BEGIN Time");
        server.send("Time_Unit:1sec");
        server.send("Total_Time:600");
        server.send("Increment:10");
        server.send("END Time");
        server.send("BEGIN Position");
        server.send("PI");
        server.send("+");
        for (const QByteArray& move : positionMoves) server.send(move);
        server.send("END Position");
        server.send("END Game_Summary");
        if (!server.waitForLine(QStringLiteral("AGREE g1"))) return false;
        server.send("START:g1");
        return QTest::qWaitFor([this] {
            return coordinator.gameState() == CsaGameCoordinator::GameState::InGame;
        }, 3000);
    }

    /// 盤のクリック操作で指したのと同じ状態にしてから、司令塔に人間の手を渡す
    void playHumanMove(const QString& usi, const QPoint& from, const QPoint& to)
    {
        const auto move = UsiMove::parse(usi);
        QVERIFY(move.has_value());
        QVERIFY(gc.board()->applyUsiMove(*move, gc.currentPlayer()));
        gc.changeCurrentPlayer();
        sfenRecord.append(gc.board()->convertBoardToSfen() + QLatin1Char(' ')
                          + (gc.currentPlayer() == Turn::Black ? QStringLiteral("b") : QStringLiteral("w"))
                          + QLatin1Char(' ') + gc.board()->convertStandToSfen()
                          + QLatin1Char(' ') + QString::number(sfenRecord.size()));
        coordinator.onHumanMove(from, to, false);
    }
};

RecordEntry entryAt(const QSignalSpy& spy, int index)
{
    return spy.at(index).at(0).value<RecordEntry>();
}

} // namespace

class TestCsaGameCoordinator : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        qRegisterMetaType<RecordEntry>();
    }

    /// 自分の手（確認）と相手の手で、残り時間・累計・手数・手番・局面列・棋譜行がそろって進む
    void myMoveAndOpponentMoveUpdateTimesAndCount()
    {
        Harness h;
        QSignalSpy records(&h.coordinator, &CsaGameCoordinator::recordEntryReady);
        QSignalSpy turns(&h.coordinator, &CsaGameCoordinator::turnChanged);
        QSignalSpy applied(&h.coordinator, &CsaGameCoordinator::moveAppliedToBoard);
        QVERIFY(h.start("+"));
        QVERIFY(h.coordinator.isMyTurn());
        QVERIFY(h.coordinator.isBlackSide());
        // 手番の始めに加算する
        QCOMPARE(h.coordinator.blackRemainingMs(), 610000);
        QCOMPARE(h.coordinator.whiteRemainingMs(), 600000);
        QCOMPARE(h.sfenRecord.size(), 1);

        h.playHumanMove(QStringLiteral("7g7f"), QPoint(7, 7), QPoint(7, 6));
        QVERIFY(h.server.waitForLine(QStringLiteral("+7776FU")));
        h.server.send("+7776FU,T5");
        QTRY_COMPARE(records.size(), 1);
        const RecordEntry mine = entryAt(records, 0);
        QCOMPARE(mine.mover, Turn::Black);
        QVERIFY(mine.displayText.contains(QStringLiteral("７六歩")));
        QCOMPARE(mine.time.elapsedMs, 5000);
        QCOMPARE(mine.time.totalMs, 5000);
        QCOMPARE(mine.sfen, h.sfenRecord.last());
        QCOMPARE(h.coordinator.blackTotalTimeMs(), 5000);
        QCOMPARE(h.coordinator.blackRemainingMs(), 605000);
        QCOMPARE(h.coordinator.whiteRemainingMs(), 610000);
        QCOMPARE(h.coordinator.progress().moveCount, 1);
        QVERIFY(!h.coordinator.isMyTurn());
        QCOMPARE(turns.last().at(0).toBool(), false);
        QCOMPARE(applied.size(), 0);  // 人間の手の駒音は盤の操作側で鳴らす
        QCOMPARE(h.gameMoves.size(), 1);

        h.server.send("-3334FU,T7");
        QTRY_COMPARE(records.size(), 2);
        const RecordEntry theirs = entryAt(records, 1);
        QCOMPARE(theirs.mover, Turn::White);
        QVERIFY(theirs.displayText.contains(QStringLiteral("３四歩")));
        QCOMPARE(theirs.time.elapsedMs, 7000);
        QCOMPARE(theirs.time.totalMs, 7000);
        QCOMPARE(h.coordinator.whiteTotalTimeMs(), 7000);
        QCOMPARE(h.coordinator.whiteRemainingMs(), 603000);
        QCOMPARE(h.coordinator.blackRemainingMs(), 615000);
        QCOMPARE(h.coordinator.progress().moveCount, 2);
        QVERIFY(h.coordinator.isMyTurn());
        QCOMPARE(turns.last().at(0).toBool(), true);
        QCOMPARE(applied.size(), 1);
        QCOMPARE(h.coordinator.usiMoves(), QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
        QCOMPARE(h.sfenRecord.size(), 3);
        QCOMPARE(theirs.sfen, h.sfenRecord.last());
        QCOMPARE(h.gameMoves.size(), 2);
        QCOMPARE(theirs.move.toUsiMove().toString(), QStringLiteral("3c3d"));
    }

    /// 再開局面（Game_Summary の指し手）は棋譜行として gameStarted で渡し、手数と時間に数える
    void resumedPositionIsDeliveredWithGameStarted()
    {
        Harness h;
        QSignalSpy started(&h.coordinator, &CsaGameCoordinator::gameStarted);
        QSignalSpy records(&h.coordinator, &CsaGameCoordinator::recordEntryReady);
        QVERIFY(h.start("+", {"+7776FU,T3", "-3334FU,T4"}));
        QCOMPARE(started.size(), 1);
        const auto initial = started.at(0).at(2).value<QList<RecordEntry>>();
        QCOMPARE(initial.size(), 2);
        QCOMPARE(initial.at(1).mover, Turn::White);
        QCOMPARE(initial.at(1).time.elapsedMs, 4000);
        QCOMPARE(initial.at(1).sfen, h.sfenRecord.at(2));
        QCOMPARE(records.size(), 0);  // 既存手順は recordEntryReady では流さない
        QCOMPARE(h.coordinator.progress().moveCount, 2);
        QCOMPARE(h.coordinator.blackTotalTimeMs(), 3000);
        QCOMPARE(h.coordinator.whiteTotalTimeMs(), 4000);
        // 先手: 600 + 10 - 3 + 10（今の手番の加算）、後手: 600 + 10 - 4
        QCOMPARE(h.coordinator.blackRemainingMs(), 617000);
        QCOMPARE(h.coordinator.whiteRemainingMs(), 606000);
        QCOMPARE(h.gameMoves.size(), 2);
    }

    /// サーバーの終局通知を終局行（TerminalType）にして、局面列も終局行のぶん伸ばす
    void endCauseMapsToTerminalType_data()
    {
        QTest::addColumn<QByteArray>("causeLine");
        QTest::addColumn<QByteArray>("resultLine");
        QTest::addColumn<int>("terminal");
        QTest::addColumn<QString>("display");
        // 自分は後手。相手（先手）の手の後で、自分の手番（後手番）に終局する
        QTest::newRow("resign-lose") << QByteArray("#RESIGN") << QByteArray("#LOSE")
                                     << int(TerminalType::Resign) << QStringLiteral("△投了");
        QTest::newRow("resign-win") << QByteArray("#RESIGN") << QByteArray("#WIN")
                                    << int(TerminalType::Resign) << QStringLiteral("▲投了");
        QTest::newRow("timeout") << QByteArray("#TIME_UP") << QByteArray("#LOSE")
                                 << int(TerminalType::Timeout) << QStringLiteral("△時間切れ");
        QTest::newRow("sennichite") << QByteArray("#SENNICHITE") << QByteArray("#DRAW")
                                    << int(TerminalType::Repetition) << QStringLiteral("千日手");
        QTest::newRow("illegal") << QByteArray("#ILLEGAL_MOVE") << QByteArray("#WIN")
                                 << int(TerminalType::IllegalLoss) << QStringLiteral("▲反則負け");
        // 連続王手の千日手は終局した局面の手番側（後手番）の行。王手をかけていた先手の反則
        QTest::newRow("oute-sennichite") << QByteArray("#OUTE_SENNICHITE") << QByteArray("#WIN")
                                         << int(TerminalType::IllegalWin) << QStringLiteral("△反則勝ち");
        QTest::newRow("jishogi-win") << QByteArray("#JISHOGI") << QByteArray("#WIN")
                                     << int(TerminalType::DeclarationWin) << QStringLiteral("△入玉勝ち");
        QTest::newRow("jishogi-draw") << QByteArray("#JISHOGI") << QByteArray("#DRAW")
                                      << int(TerminalType::Impasse) << QStringLiteral("持将棋");
        QTest::newRow("max-moves") << QByteArray("#MAX_MOVES") << QByteArray("#CENSORED")
                                   << int(TerminalType::MaxMoves) << QStringLiteral("最大手数到達");
    }

    void endCauseMapsToTerminalType()
    {
        QFETCH(QByteArray, causeLine);
        QFETCH(QByteArray, resultLine);
        QFETCH(int, terminal);
        QFETCH(QString, display);

        Harness h;
        QSignalSpy records(&h.coordinator, &CsaGameCoordinator::recordEntryReady);
        QSignalSpy ended(&h.coordinator, &CsaGameCoordinator::gameEnded);
        QVERIFY(h.start("-"));
        QVERIFY(!h.coordinator.isMyTurn());
        h.server.send("+7776FU,T2");
        QTRY_COMPARE(records.size(), 1);
        h.server.send("%TORYO,T6");  // 消費時間の付いた意思表示（勝敗は次の2行で決まる）
        h.server.send(causeLine);
        h.server.send(resultLine);
        QTRY_COMPARE(ended.size(), 1);
        QCOMPARE(records.size(), 2);
        const RecordEntry ending = entryAt(records, 1);
        QCOMPARE(int(ending.terminal), terminal);
        QCOMPARE(ending.displayText, display);
        QCOMPARE(ending.time.elapsedMs, 6000);
        QCOMPARE(h.sfenRecord.size(), 3);  // 開始局面・1手・終局行
        QCOMPARE(h.sfenRecord.at(2), h.sfenRecord.at(1));
        QCOMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
    }

    /// 終局行の組み立ては値型だけで決まる（消費時間は終局行を行った側の累計に足す）
    void progressEndRecordUsesMoverTotals()
    {
        CsaGameProgress progress;
        progress.isBlackSide = false;
        progress.blackTotalMs = 30000;
        progress.whiteTotalMs = 50000;
        const RecordEntry resign = progress.endRecord(CsaClient::GameResult::Lose,
                                                      CsaClient::GameEndCause::Resign, false, 4000);
        QCOMPARE(resign.displayText, QStringLiteral("△投了"));
        QCOMPARE(resign.time.totalMs, 54000);
        const RecordEntry declared = progress.endRecord(CsaClient::GameResult::Lose,
                                                        CsaClient::GameEndCause::Jishogi, true, 2000);
        QCOMPARE(declared.displayText, QStringLiteral("▲入玉勝ち"));
        QCOMPARE(declared.time.totalMs, 32000);
        const RecordEntry interrupted = progress.endRecord(CsaClient::GameResult::Chudan,
                                                           CsaClient::GameEndCause::Chudan, true, 0);
        QCOMPARE(interrupted.terminal, TerminalType::Interrupt);
        QCOMPARE(interrupted.displayText, QStringLiteral("中断"));
    }

    /// 加算の扱いとエンジンへ渡す時間（手番側は今の手番の加算を除く）
    void progressTimeTracking()
    {
        CsaClient::GameSummary summary;
        summary.myTurn = QStringLiteral("-");
        summary.toMove = QStringLiteral("+");
        summary.timeBlack.specified = true;
        summary.timeBlack.totalTime = 60;
        summary.timeBlack.increment = 5;
        summary.timeWhite = summary.timeBlack;
        CsaGameProgress progress = CsaGameProgress::forSummary(summary);
        QVERIFY(!progress.isBlackSide);
        progress.startClockFromSummary();
        QCOMPARE(progress.blackRemainingMs, 65000);
        QCOMPARE(progress.whiteRemainingMs, 60000);
        progress.recordMoveTime(true, 70000);  // 残りを超えた分は 0 に丸める
        QCOMPARE(progress.blackRemainingMs, 0);
        QCOMPARE(progress.blackTotalMs, 70000);
        QCOMPARE(progress.whiteRemainingMs, 65000);
        const UsiTimingParams timing = progress.engineTiming(-100);
        QCOMPARE(timing.wtimeMs, 60000);
        QCOMPARE(timing.btimeMs, 0);
        QCOMPARE(timing.marginMs, 0);
        QCOMPARE(progress.moveRecord(true, QStringLiteral("▲７六歩"), 70000).time.totalMs, 70000);
    }

    /// 配線を通すと、棋譜（ライブ記録）には着手ごと・終局で1行ずつだけ入る
    void wiringAppendsEachRecordOnce()
    {
        Harness h;
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        LiveGameSession live;
        live.setTree(&tree);
        QString currentSfen;
        live.startFromRoot();
        GameRecordUpdateService::Deps rd;
        rd.liveGameSession = &live;
        rd.currentSfenStr = &currentSfen;
        rd.sfenRecord = &h.sfenRecord;
        GameRecordUpdateService recordService(rd);

        CsaGameWiring::Dependencies wd;
        wd.coordinator = &h.coordinator;
        CsaGameWiring wiring(wd);
        wiring.wireExternalSignals(nullptr, &recordService, nullptr);
        wiring.wire();
        wiring.wire();  // 二重に配線しても1回だけ届く
        wiring.wireExternalSignals(nullptr, &recordService, nullptr);
        QSignalSpy sounds(&wiring, &CsaGameWiring::moveAppliedToBoard);

        QVERIFY(h.start("-"));
        h.server.send("+7776FU,T1");
        QTRY_COMPARE(live.moves().size(), 1);
        QCOMPARE(sounds.size(), 1);
        QCOMPARE(currentSfen, h.sfenRecord.last());
        const KifDisplayItem first = live.moves().at(0);
        QCOMPARE(first.usiMove, QStringLiteral("7g7f"));
        QCOMPARE(first.beforeSfen, h.sfenRecord.at(0));

        // 自分（後手）の手：人間の操作で盤を動かしてから送る
        h.playHumanMove(QStringLiteral("3c3d"), QPoint(3, 3), QPoint(3, 4));
        QVERIFY(h.server.waitForLine(QStringLiteral("-3334FU")));
        h.server.send("-3334FU,T2");
        QTRY_COMPARE(live.moves().size(), 2);
        QCOMPARE(live.moves().at(1).usiMove, QStringLiteral("3c3d"));

        h.server.send("%TORYO,T3");
        h.server.send("#RESIGN");
        h.server.send("#WIN");
        QTRY_COMPARE(h.coordinator.gameState(), CsaGameCoordinator::GameState::GameOver);
        QCOMPARE(live.moves().size(), 3);
        QVERIFY(live.moves().at(2).terminal);
        QCOMPARE(live.moves().at(2).prettyMove, QStringLiteral("▲投了"));
        QCOMPARE(sounds.size(), 1);
    }
};

QTEST_MAIN(TestCsaGameCoordinator)
#include "tst_csagamecoordinator.moc"
