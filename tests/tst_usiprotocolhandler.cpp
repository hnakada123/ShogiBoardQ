/// @file tst_usiprotocolhandler.cpp
/// @brief UsiProtocolHandler（エンジン1台の状態と送受信）のユニットテスト
///
/// 受信行は onDataReceived で直接渡し、結果（bestmove・checkmate・info）と状態の移り変わりを検証する。
/// 送ったコマンドは、テスト用エンジンを起動した EngineProcessManager の commandSent で確かめる。

#include <QtTest>
#include <QSignalSpy>

#include "engineprocessmanager.h"
#include "thinkinginfoformatter.h"
#include "usiinfo.h"
#include "usilineparser.h"
#include "usiprotocolhandler.h"

namespace {

/// position コマンドの局面
UsiPosition usiPosition(const char* command)
{
    return UsiPosition::parse(QString::fromLatin1(command)).value_or(UsiPosition{});
}

UsiInfo infoAt(const QSignalSpy& spy, qsizetype index)
{
    return qvariant_cast<UsiInfo>(spy.at(index).at(0));
}

UsiBestMove bestAt(const QSignalSpy& spy, qsizetype index)
{
    return qvariant_cast<UsiBestMove>(spy.at(index).at(0));
}

UsiCheckmate checkmateAt(const QSignalSpy& spy, qsizetype index)
{
    return qvariant_cast<UsiCheckmate>(spy.at(index).at(0));
}

UsiGoTime byoyomi(qint64 btime, qint64 byoyomiMs)
{
    UsiGoTime time;
    time.btime = btime;
    time.byoyomi = byoyomiMs;
    time.useByoyomi = true;
    return time;
}

} // namespace

class TestUsiProtocolHandler : public QObject
{
    Q_OBJECT

private:
    static bool startProcess(EngineProcessManager& process)
    {
        QString path = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
#ifdef Q_OS_WIN
        path += QStringLiteral(".exe");
#endif
        QSignalSpy started(&process, &EngineProcessManager::processStarted);
        return process.startProcessAsync(path) && (!started.isEmpty() || started.wait(5000));
    }

    /// 初期化を終えて待機の状態にする
    static void makeReady(UsiProtocolHandler& handler)
    {
        handler.initializeEngineAsync();
        handler.onDataReceived(QStringLiteral("usiok"));
        handler.onDataReceived(QStringLiteral("readyok"));
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<UsiInfo>();
        qRegisterMetaType<UsiBestMove>();
        qRegisterMetaType<UsiCheckmate>();
    }

    // ================================================================
    // 状態
    // ================================================================

    void initialState()
    {
        UsiProtocolHandler handler;
        QCOMPARE(handler.state(), UsiProtocolHandler::State::NotStarted);
        QVERIFY(!handler.isInitializing());
        QVERIFY(!handler.isSearching());
        QVERIFY(!handler.isPonderEnabled());
        QVERIFY(!handler.searchCandidate().active);
    }

    /// 起動から初期化・探索・停止・結果までを1つの状態で追う
    void stateFollowsLifecycle()
    {
        UsiProtocolHandler handler;
        handler.beginStartup();
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Starting);
        QVERIFY(handler.isInitializing());
        handler.initializeEngineAsync();
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Handshaking);
        handler.onDataReceived(QStringLiteral("usiok"));
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Handshaking);
        handler.onDataReceived(QStringLiteral("readyok"));
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Ready);

        handler.startSearch(usiPosition("position startpos"), UsiGo::infinite());
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Searching);
        QCOMPARE(handler.searchKind(), UsiProtocolHandler::SearchKind::Main);
        handler.sendStop();
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Stopping);
        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Ready);

        handler.startSearch(UsiPosition{}, UsiGo::mate(1000));
        QCOMPARE(handler.searchKind(), UsiProtocolHandler::SearchKind::Mate);
        handler.onDataReceived(QStringLiteral("checkmate nomate"));
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Ready);

        handler.markStopped();
        QCOMPARE(handler.state(), UsiProtocolHandler::State::NotStarted);
    }

    /// 送るオプションは外から渡す（設定ファイルを読まない）。報告されたオプションだけを送る
    void launchOptionsAreSentAfterUsiOk()
    {
        EngineProcessManager process;
        QVERIFY(startProcess(process));
        UsiProtocolHandler handler(&process);
        EngineProfileStore::LaunchOptions launch;
        launch.options = {{QStringLiteral("Threads"), QStringLiteral("4"), false},
                          {QStringLiteral("ClearHash"), QString(), true},
                          {QStringLiteral("Unreported"), QStringLiteral("1"), false}};
        launch.ponderEnabled = true;
        launch.sendUnreportedPonder = false;
        handler.setLaunchOptions(launch);
        QVERIFY(handler.isPonderEnabled());
        QSignalSpy commands(&process, &EngineProcessManager::commandSent);
        handler.initializeEngineAsync();
        handler.onDataReceived(QStringLiteral("option name Threads type spin default 1 min 1 max 16"));
        handler.onDataReceived(QStringLiteral("option name ClearHash type button"));
        handler.onDataReceived(QStringLiteral("usiok"));
        QStringList sent;
        for (const auto& args : std::as_const(commands)) sent.append(args.first().toString());
        QVERIFY(sent.contains(QStringLiteral("setoption name Threads value 4")));
        QVERIFY(sent.contains(QStringLiteral("setoption name ClearHash")));
        QVERIFY(!sent.join(QLatin1Char('\n')).contains(QStringLiteral("Unreported")));
        // 報告のない USI_Ponder は、送らない指定なら送らない
        QVERIFY(!sent.join(QLatin1Char('\n')).contains(QStringLiteral("USI_Ponder")));
    }

    // ================================================================
    // 探索と先読み
    // ================================================================

    void ponderPreservesPredictionAndSendsTime()
    {
        EngineProcessManager process;
        QVERIFY(startProcess(process));
        UsiProtocolHandler handler(&process);
        QSignalSpy commands(&process, &EngineProcessManager::commandSent);
        handler.onDataReceived(QStringLiteral("bestmove 8c8d ponder 2g2f"));
        UsiGoTime goTime = byoyomi(290000, 4750);
        handler.startSearch(UsiPosition{}, UsiGo::ponder(goTime));
        QCOMPARE(handler.searchCandidate().predictedMove, QStringLiteral("2g2f"));
        QCOMPARE(handler.searchKind(), UsiProtocolHandler::SearchKind::Ponder);
        QCOMPARE(commands.last().at(0).toString(), QStringLiteral("go ponder btime 290000 wtime 0 byoyomi 4750"));
        handler.sendPonderHit();
        QCOMPARE(handler.searchKind(), UsiProtocolHandler::SearchKind::Main);
        QCOMPARE(commands.last().at(0).toString(), QStringLiteral("ponderhit"));
    }

    void ponderSendsIncrements()
    {
        EngineProcessManager process;
        QVERIFY(startProcess(process));
        UsiProtocolHandler handler(&process);
        QSignalSpy commands(&process, &EngineProcessManager::commandSent);
        UsiGoTime goTime;
        goTime.btime = 1000;
        goTime.wtime = 2000;
        goTime.binc = 300;
        goTime.winc = 400;
        handler.startSearch(UsiPosition{}, UsiGo::ponder(goTime));
        QCOMPARE(commands.last().at(0).toString(), QStringLiteral("go ponder btime 1000 wtime 2000 binc 300 winc 400"));
    }

    /// position を送ると読み筋の局面が変わり、go の種類ごとのコマンドを送る
    void startSearchSendsPositionAndGo()
    {
        EngineProcessManager process;
        QVERIFY(startProcess(process));
        UsiProtocolHandler handler(&process);
        QSignalSpy commands(&process, &EngineProcessManager::commandSent);
        QSignalSpy positions(&handler, &UsiProtocolHandler::positionChanged);
        QSignalSpy started(&handler, &UsiProtocolHandler::searchStarted);
        handler.startSearch(usiPosition("position startpos moves 7g7f"), UsiGo::infinite());
        QCOMPARE(commands.at(0).at(0).toString(), QStringLiteral("position startpos moves 7g7f"));
        QCOMPARE(commands.at(1).at(0).toString(), QStringLiteral("go infinite"));
        QCOMPARE(positions.count(), 1);
        const auto searched = qvariant_cast<UsiPosition>(positions.first().first());
        QCOMPARE(searched.sideToMove(), Turn::White);
        QCOMPARE(searched.lastMove()->toString(), QStringLiteral("7g7f"));
        QCOMPARE(started.count(), 1);
        handler.onDataReceived(QStringLiteral("bestmove 3c3d"));

        // 詰み探索も同じ入口を通り、思考情報を消す
        commands.clear();
        handler.startSearch(usiPosition("position sfen 7k1/9/9/9/9/9/9/9/K8 b RS 1"), UsiGo::mate(3000));
        QCOMPARE(commands.last().at(0).toString(), QStringLiteral("go mate 3000"));
        QCOMPARE(started.count(), 2);
        handler.onDataReceived(QStringLiteral("checkmate nomate"));
        handler.startSearch(UsiPosition{}, UsiGo::mate(0));
        QCOMPARE(commands.last().at(0).toString(), QStringLiteral("go mate infinite"));
    }

    /// 外れた先読みを stop で回収した結果は、先読みの結果として知らせる（投了・宣言勝ちも対局の結果ではない）
    void discardedPonderResultIsMarked()
    {
        UsiProtocolHandler handler;
        QSignalSpy bestmove(&handler, &UsiProtocolHandler::bestMoveReceived);
        handler.startSearch(UsiPosition{}, UsiGo::ponder({}));
        handler.sendStop();
        handler.onDataReceived(QStringLiteral("bestmove resign"));
        QCOMPARE(bestmove.count(), 1);
        QVERIFY(bestAt(bestmove, 0).pondering);
        QVERIFY(bestAt(bestmove, 0).stopped);
        QCOMPARE(bestAt(bestmove, 0).special, SpecialMove::Resign);

        // 当たった先読みは本探索の結果になる
        handler.startSearch(UsiPosition{}, UsiGo::ponder({}));
        handler.sendPonderHit();
        handler.onDataReceived(QStringLiteral("bestmove resign"));
        QVERIFY(!bestAt(bestmove, 1).pondering);
        QVERIFY(!bestAt(bestmove, 1).stopped);
    }

    void stoppedPonderCannotBeHit_data()
    {
        QTest::addColumn<QString>("result");
        QTest::newRow("move") << QStringLiteral("bestmove 3c3d ponder 2f2e");
        QTest::newRow("resign") << QStringLiteral("bestmove resign");
        QTest::newRow("win") << QStringLiteral("bestmove win");
    }

    void stoppedPonderCannotBeHit()
    {
        QFETCH(QString, result);
        EngineProcessManager process;
        QVERIFY(startProcess(process));
        UsiProtocolHandler handler(&process);
        QSignalSpy commands(&process, &EngineProcessManager::commandSent);
        QSignalSpy bestmove(&handler, &UsiProtocolHandler::bestMoveReceived);
        handler.onDataReceived(QStringLiteral("bestmove 8c8d ponder 2g2f"));
        handler.startSearch(UsiPosition{}, UsiGo::ponder({}));
        commands.clear();
        handler.sendStop();
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Stopping);
        handler.sendPonderHit(); // stopへの応答待ちの間も切り替えてはいけない
        handler.sendStop();
        QCOMPARE(commands.count(), 1);
        QCOMPARE(commands.first().at(0).toString(), QStringLiteral("stop"));
        handler.onDataReceived(result);
        QCOMPARE(bestmove.count(), 2);
        QVERIFY(bestAt(bestmove, 1).pondering);
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Ready);
        handler.sendPonderHit(); // 回収済みの探索にも送らない
        handler.sendStop();      // 探索していなければ stop も送らない
        QCOMPARE(commands.count(), 1);
        // 外れた先読みの ponder は次の先読みの予想にしない
        handler.startSearch(UsiPosition{}, UsiGo::ponder({}));
        QVERIFY(handler.searchCandidate().predictedMove.isEmpty());
    }

    // ================================================================
    // info
    // ================================================================

    void infoLine_parsedAndForwarded()
    {
        UsiProtocolHandler handler;
        QSignalSpy spy(&handler, &UsiProtocolHandler::infoReceived);
        handler.onDataReceived(QStringLiteral(
            "info depth 20 seldepth 30 score cp 150 nodes 1000000 nps 500000 pv 7g7f 3c3d 2g2f"));
        QCOMPARE(spy.count(), 1);
        const UsiInfo info = infoAt(spy, 0);
        QCOMPARE(info.depth, 20);
        QCOMPARE(info.seldepth, 30);
        QVERIFY(info.score.isCp());
        QCOMPARE(info.score.value, 150);
        QCOMPARE(info.nodes, 1000000LL);
        QCOMPARE(info.pv, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f")}));

        handler.onDataReceived(QStringLiteral("info string evaluation started"));
        QCOMPARE(infoAt(spy, 1).string, QStringLiteral("evaluation started"));
        QVERIFY(!infoAt(spy, 1).hasPv());
        handler.onDataReceived(QStringLiteral("info depth 5 nodes 100"));
        QVERIFY(!infoAt(spy, 2).hasScore());
    }

    /// 最善の読み筋（multipv 1）の最後の評価値と候補手を探索ごとに覚え、bestmove に添える
    void bestmoveCarriesSearchScoreAndCandidate()
    {
        UsiProtocolHandler handler;
        QSignalSpy bestmove(&handler, &UsiProtocolHandler::bestMoveReceived);
        handler.startSearch(usiPosition("position startpos"), UsiGo::infinite());
        handler.onDataReceived(QStringLiteral("info depth 5 multipv 1 score cp 80 pv 2g2f 8c8d"));
        handler.onDataReceived(QStringLiteral("info depth 5 multipv 2 score cp -40 pv 7g7f"));
        QCOMPARE(handler.searchCandidate().move, QStringLiteral("2g2f"));
        handler.onDataReceived(QStringLiteral("info depth 6 multipv 1 score mate 3 pv 2g2f"));
        handler.onDataReceived(QStringLiteral("bestmove 2g2f ponder 8c8d"));
        const UsiBestMove result = bestAt(bestmove, 0);
        QCOMPARE(result.move, QStringLiteral("2g2f"));
        QCOMPARE(result.ponder, QStringLiteral("8c8d"));
        QVERIFY(result.score.isMate());
        QCOMPARE(result.score.value, 3);
        QVERIFY(result.baseSfen.startsWith(QStringLiteral("lnsgkgsnl")));
        QVERIFY(!handler.searchCandidate().active);

        // 次の探索では前の評価値を使わない
        handler.startSearch(UsiPosition{}, UsiGo::infinite());
        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));
        QVERIFY(bestAt(bestmove, 1).score.isNone());
    }

    // ================================================================
    // bestmove
    // ================================================================

    void bestmove_variants_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("move");
        QTest::addColumn<QString>("ponder");
        QTest::addColumn<int>("special");
        QTest::newRow("normal") << "bestmove 7g7f" << "7g7f" << "" << int(SpecialMove::None);
        QTest::newRow("ponder") << "bestmove 7g7f ponder 3c3d" << "7g7f" << "3c3d" << int(SpecialMove::None);
        QTest::newRow("promotion") << "bestmove 7g7f+" << "7g7f+" << "" << int(SpecialMove::None);
        QTest::newRow("drop-ponder") << "bestmove P*5e ponder 5d5e" << "P*5e" << "5d5e" << int(SpecialMove::None);
        QTest::newRow("tab") << "bestmove\t7g7f\tponder\t3c3d" << "7g7f" << "3c3d" << int(SpecialMove::None);
        QTest::newRow("extra-tokens") << "bestmove 7g7f ponder 3c3d extra garbage" << "7g7f" << "3c3d"
                                      << int(SpecialMove::None);
        QTest::newRow("empty-ponder") << "bestmove 2g2f ponder " << "2g2f" << "" << int(SpecialMove::None);
        QTest::newRow("non-usi") << "bestmove INVALID_MOVE" << "INVALID_MOVE" << "" << int(SpecialMove::None);
        QTest::newRow("resign") << "bestmove resign" << "resign" << "" << int(SpecialMove::Resign);
        QTest::newRow("win") << "bestmove win" << "win" << "" << int(SpecialMove::Win);
        QTest::newRow("resign-upper") << "bestmove RESIGN" << "RESIGN" << "" << int(SpecialMove::Resign);
    }

    void bestmove_variants()
    {
        QFETCH(QString, line);
        QFETCH(QString, move);
        QFETCH(QString, ponder);
        QFETCH(int, special);
        UsiProtocolHandler handler;
        QSignalSpy bestmove(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy errors(&handler, &UsiProtocolHandler::errorOccurred);
        handler.onDataReceived(line);
        QCOMPARE(bestmove.count(), 1);
        QCOMPARE(errors.count(), 0);
        const UsiBestMove result = bestAt(bestmove, 0);
        QCOMPARE(result.move, move);
        QCOMPARE(result.ponder, ponder);
        QCOMPARE(int(result.special), special);
        QCOMPARE(result.elapsedMs, 0); // go を送っていない
    }

    /// 指し手の語のない bestmove はエラーにする
    void bestmove_withoutMoveIsError_data()
    {
        QTest::addColumn<QString>("line");
        QTest::newRow("no-token") << QStringLiteral("bestmove");
        QTest::newRow("whitespace") << QStringLiteral("bestmove   ");
    }

    void bestmove_withoutMoveIsError()
    {
        QFETCH(QString, line);
        UsiProtocolHandler handler;
        QSignalSpy bestmove(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy errors(&handler, &UsiProtocolHandler::errorOccurred);
        handler.onDataReceived(line);
        QCOMPARE(bestmove.count(), 0);
        QCOMPARE(errors.count(), 1);
    }

    /// 取り消した探索に遅れて届いた結果は知らせない。次の探索の結果は知らせる
    void cancelledSearchResultIsDropped()
    {
        UsiProtocolHandler handler;
        QSignalSpy bestmove(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy checkmate(&handler, &UsiProtocolHandler::checkmateReceived);
        handler.startSearch(UsiPosition{}, UsiGo::infinite());
        handler.cancelCurrentOperation();
        // エンジンはまだ考えているので、取り消した後も stop は送る
        QVERIFY(handler.isSearching());
        handler.onDataReceived(QStringLiteral("bestmove 7g7f"));
        QCOMPARE(bestmove.count(), 0);
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Ready);

        handler.startSearch(UsiPosition{}, UsiGo::mate(1000));
        handler.cancelCurrentOperation();
        handler.onDataReceived(QStringLiteral("checkmate nomate"));
        QCOMPARE(checkmate.count(), 0);

        handler.startSearch(UsiPosition{}, UsiGo::infinite());
        handler.onDataReceived(QStringLiteral("bestmove 2g2f"));
        QCOMPARE(bestmove.count(), 1);
        QCOMPARE(bestAt(bestmove, 0).move, QStringLiteral("2g2f"));
    }

    /// 連続して届く bestmove はそのつど知らせる
    void rapidBestmovesAreEachReported()
    {
        UsiProtocolHandler handler;
        QSignalSpy bestmove(&handler, &UsiProtocolHandler::bestMoveReceived);
        handler.onDataReceived(QStringLiteral("bestmove 7g7f ponder 3c3d"));
        handler.onDataReceived(QStringLiteral("bestmove 2g2f ponder 8c8d"));
        handler.onDataReceived(QStringLiteral("bestmove 5g5f"));
        QCOMPARE(bestmove.count(), 3);
        QCOMPARE(bestAt(bestmove, 2).move, QStringLiteral("5g5f"));
        QVERIFY(bestAt(bestmove, 2).ponder.isEmpty());
    }

    // ================================================================
    // checkmate
    // ================================================================

    void checkmate_variants_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<int>("result");
        QTest::addColumn<QStringList>("pv");
        QTest::newRow("solved") << "checkmate 5e5d 4a3b 5d5c" << int(UsiCheckmate::Result::Mate)
                                << QStringList{"5e5d", "4a3b", "5d5c"};
        QTest::newRow("single") << "checkmate G*5b" << int(UsiCheckmate::Result::Mate) << QStringList{"G*5b"};
        QTest::newRow("nomate") << "checkmate nomate" << int(UsiCheckmate::Result::NoMate) << QStringList{};
        QTest::newRow("NOMATE") << "checkmate NOMATE" << int(UsiCheckmate::Result::NoMate) << QStringList{};
        QTest::newRow("notimplemented") << "checkmate NotImplemented" << int(UsiCheckmate::Result::NotImplemented)
                                        << QStringList{};
        QTest::newRow("timeout") << "checkmate timeout" << int(UsiCheckmate::Result::Unknown) << QStringList{};
        QTest::newRow("unknown") << "checkmate Unknown" << int(UsiCheckmate::Result::Unknown) << QStringList{};
        QTest::newRow("empty") << "checkmate" << int(UsiCheckmate::Result::Unknown) << QStringList{};
        QTest::newRow("whitespace") << "checkmate   " << int(UsiCheckmate::Result::Unknown) << QStringList{};
    }

    void checkmate_variants()
    {
        QFETCH(QString, line);
        QFETCH(int, result);
        QFETCH(QStringList, pv);
        UsiProtocolHandler handler;
        QSignalSpy checkmate(&handler, &UsiProtocolHandler::checkmateReceived);
        handler.onDataReceived(line);
        QCOMPARE(checkmate.count(), 1);
        QCOMPARE(int(checkmateAt(checkmate, 0).result), result);
        QCOMPARE(checkmateAt(checkmate, 0).pv, pv);
    }

    /// stop の後の checkmate は打ち切った探索の結果として知らせる
    void checkmateAfterStopIsMarked()
    {
        UsiProtocolHandler handler;
        QSignalSpy checkmate(&handler, &UsiProtocolHandler::checkmateReceived);
        handler.startSearch(UsiPosition{}, UsiGo::mate(0));
        handler.sendStop();
        handler.onDataReceived(QStringLiteral("checkmate 5e5d"));
        QVERIFY(checkmateAt(checkmate, 0).stopped);
    }

    /// info 行を出さずに checkmate だけを返すエンジンでも、思考の表に詰み手順の行を残す
    /// 先読み中の読み筋の「同」は、予想手（探索の局面に至った手）の移動先を基準にする（U2）
    void ponderPvUsesPredictedMoveForSameSquare()
    {
        ThinkingInfoFormatter formatter;
        // 自分が指した後、相手の予想手 5d5e を指した局面で先読みする（「同」の基準は探索の局面に至った手）
        formatter.setSearchPosition(usiPosition("position sfen 4k4/9/9/4p4/9/9/9/4R4/4K4 w - 1 moves 5d5e"));
        QSignalSpy rows(&formatter, &ThinkingInfoFormatter::thinkingInfoUpdated);
        formatter.processInfo(*UsiLineParser::parseInfo(QStringLiteral("info depth 3 score cp 500 pv 5h5e 5a4b")));
        QCOMPARE(rows.count(), 1);
        const auto row = qvariant_cast<ThinkingInfoRow>(rows.at(0).at(0));
        QVERIFY2(row.pvKanji.startsWith(QStringLiteral("▲同飛(58)")), qPrintable(row.pvKanji));
    }

    void checkmate_solved_addsThinkingRow()
    {
        UsiProtocolHandler handler;
        ThinkingInfoFormatter formatter;
        connect(&handler, &UsiProtocolHandler::positionChanged, &formatter, &ThinkingInfoFormatter::setSearchPosition);
        connect(&handler, &UsiProtocolHandler::checkmateReceived, &formatter, &ThinkingInfoFormatter::presentCheckmate);
        QSignalSpy rows(&formatter, &ThinkingInfoFormatter::thinkingInfoUpdated);

        handler.startSearch(usiPosition("position sfen 7k1/9/9/9/9/9/9/9/K8 b RS 1"), UsiGo::mate(1000));
        handler.onDataReceived(QStringLiteral("checkmate S*1b 2a3a R*2a"));

        QCOMPARE(rows.count(), 1);
        const auto row = qvariant_cast<ThinkingInfoRow>(rows.at(0).at(0));
        QVERIFY(row.score.isMate());
        QCOMPARE(row.score.value, 3);
        QCOMPARE(row.pvKanji, QStringLiteral("▲１二銀打△３一玉(21)▲２一飛打"));
        QCOMPARE(row.usiPv, QStringLiteral("S*1b 2a3a R*2a"));
    }

    // ================================================================
    // 初期化
    // ================================================================

    void initializationRequiresFreshResponses()
    {
        UsiProtocolHandler handler;
        QSignalSpy initialized(&handler, &UsiProtocolHandler::initializationFinished);
        handler.onDataReceived(QStringLiteral("usiok"));
        handler.onDataReceived(QStringLiteral("readyok"));
        handler.initializeEngineAsync();
        handler.onDataReceived(QStringLiteral("readyok")); // usiok前の応答は完了扱いしない
        QCOMPARE(initialized.count(), 0);
        handler.onDataReceived(QStringLiteral("  usiok"));
        QCOMPARE(initialized.count(), 0);
        handler.onDataReceived(QStringLiteral("readyok "));
        QCOMPARE(initialized.count(), 1);
        QVERIFY(initialized.first().first().toBool());
        handler.onDataReceived(QStringLiteral("readyok"));
        QCOMPARE(initialized.count(), 1);
    }

    /// 登録の問い合わせは usiok で終える（isready を送らない）
    void initializationUntilUsiOk()
    {
        EngineProcessManager process;
        QVERIFY(startProcess(process));
        UsiProtocolHandler handler(&process);
        QSignalSpy commands(&process, &EngineProcessManager::commandSent);
        QSignalSpy initialized(&handler, &UsiProtocolHandler::initializationFinished);
        handler.initializeEngineAsync(5000, true);
        handler.onDataReceived(QStringLiteral("id name Mock Engine 1.0"));
        handler.onDataReceived(QStringLiteral("id author someone"));
        handler.onDataReceived(QStringLiteral("option name Multi PV type spin default 1 min 1 max 500"));
        handler.onDataReceived(QStringLiteral("option name Broken"));
        handler.onDataReceived(QStringLiteral("usiok"));
        QCOMPARE(initialized.count(), 1);
        QCOMPARE(handler.state(), UsiProtocolHandler::State::Ready);
        QCOMPARE(commands.count(), 1);
        QCOMPARE(commands.first().first().toString(), QStringLiteral("usi"));
        QCOMPARE(handler.reportedEngineName(), QStringLiteral("Mock Engine 1.0"));
        QCOMPARE(handler.reportedEngineAuthor(), QStringLiteral("someone"));
        QCOMPARE(handler.reportedOptionLines(),
                 QStringList({QStringLiteral("option name Multi PV type spin default 1 min 1 max 500"),
                              QStringLiteral("option name Broken")}));
        const auto multiPv = handler.reportedOption(QStringLiteral("Multi PV"));
        QVERIFY(multiPv);
        QCOMPARE(multiPv->maxValue, QStringLiteral("500"));
        QVERIFY(!handler.reportedOption(QStringLiteral("Broken")));
    }

    void initializationTimeout_data()
    {
        QTest::addColumn<bool>("receivedUsiOk");
        QTest::newRow("usiok-timeout") << false;
        QTest::newRow("readyok-timeout") << true;
    }

    void initializationTimeout()
    {
        QFETCH(bool, receivedUsiOk);
        UsiProtocolHandler handler;
        QSignalSpy initialized(&handler, &UsiProtocolHandler::initializationFinished);
        QSignalSpy errors(&handler, &UsiProtocolHandler::errorOccurred);
        handler.initializeEngineAsync(30);
        if (receivedUsiOk) handler.onDataReceived(QStringLiteral("usiok"));
        QCOMPARE(initialized.count(), 0); // 要求は完了を待たずに返る
        QTRY_COMPARE_WITH_TIMEOUT(initialized.count(), 1, 1000);
        QVERIFY(!initialized.first().first().toBool());
        QCOMPARE(errors.count(), 1);
        QCOMPARE(handler.state(), UsiProtocolHandler::State::NotStarted);
        handler.onDataReceived(QStringLiteral("usiok"));
        handler.onDataReceived(QStringLiteral("readyok"));
        QCOMPARE(initialized.count(), 1); // 期限後の応答を採用しない
    }

    void cancelledInitializationDoesNotCompleteOrTimeout()
    {
        UsiProtocolHandler handler;
        QSignalSpy initialized(&handler, &UsiProtocolHandler::initializationFinished);
        QSignalSpy errors(&handler, &UsiProtocolHandler::errorOccurred);
        handler.initializeEngineAsync(30);
        handler.cancelCurrentOperation();
        QCOMPARE(handler.state(), UsiProtocolHandler::State::NotStarted);
        handler.onDataReceived(QStringLiteral("usiok"));
        handler.onDataReceived(QStringLiteral("readyok"));
        QTest::qWait(60);
        QCOMPARE(initialized.count(), 0);
        QCOMPARE(errors.count(), 0);
        makeReady(handler);
        QCOMPARE(initialized.count(), 1);
        QVERIFY(initialized.first().first().toBool());
    }

    // ================================================================
    // そのほかのコマンド・受信
    // ================================================================

    void commandsAreFormatted()
    {
        EngineProcessManager process;
        QVERIFY(startProcess(process));
        UsiProtocolHandler handler(&process);
        QSignalSpy commands(&process, &EngineProcessManager::commandSent);
        handler.sendSetOption(QStringLiteral("USI_Hash"), QStringLiteral("256"));
        handler.sendGameOver(GameOverResult::Lose);
        handler.sendConsoleCommand(QStringLiteral("d"));
        QCOMPARE(commands.at(0).at(0).toString(), QStringLiteral("setoption name USI_Hash value 256"));
        QCOMPARE(commands.at(1).at(0).toString(), QStringLiteral("gameover lose"));
        QCOMPARE(commands.at(2).at(0).toString(), QStringLiteral("d"));
    }

    void unknownAndEmptyLinesAreIgnored_data()
    {
        QTest::addColumn<QString>("line");
        QTest::newRow("empty") << QString();
        QTest::newRow("whitespace") << QStringLiteral("   ");
        QTest::newRow("unknown") << QStringLiteral("unknowncommand some data");
        QTest::newRow("prefix") << QStringLiteral("bestmovex 7g7f");
        QTest::newRow("binary") << QStringLiteral("\x01\x02\x03");
        QTest::newRow("long") << QString(10000, QLatin1Char('x'));
    }

    void unknownAndEmptyLinesAreIgnored()
    {
        QFETCH(QString, line);
        UsiProtocolHandler handler;
        QSignalSpy info(&handler, &UsiProtocolHandler::infoReceived);
        QSignalSpy bestmove(&handler, &UsiProtocolHandler::bestMoveReceived);
        QSignalSpy checkmate(&handler, &UsiProtocolHandler::checkmateReceived);
        QSignalSpy errors(&handler, &UsiProtocolHandler::errorOccurred);
        handler.onDataReceived(line);
        QCOMPARE(info.count(), 0);
        QCOMPARE(bestmove.count(), 0);
        QCOMPARE(checkmate.count(), 0);
        QCOMPARE(errors.count(), 0);
    }
};

QTEST_MAIN(TestUsiProtocolHandler)
#include "tst_usiprotocolhandler.moc"
