#include <QtTest>
#include <QFile>
#include <QSignalSpy>
#include <QProcess>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QDir>
#include <QSqlQuery>
#include "tsumepositionanalyzer.h"
#include "tsumeprogressstore.h"
#include "tsumesolutionreplay.h"
#include <limits>
#include "tsumecollection.h"
#include "tsumegamesession.h"

class TestTsumePlay : public QObject
{
    Q_OBJECT
    QList<TsumeProblem> problems;
    static QByteArray response(QProcess& process, const QByteArray& prefix)
    {
        QElapsedTimer timer;
        timer.start();
        while (timer.elapsed() < 10000) {
            while (process.canReadLine()) {
                const auto line = process.readLine().trimmed();
                if (line.startsWith(prefix)) return line;
            }
            process.waitForReadyRead(100);
        }
        return {};
    }
private slots:
    void cleanup() { qunsetenv("SHOGI_TEST_MATE_MODE"); }
    void initTestCase()
    {
        QFile file(QFINDTESTDATA("fixtures/tsume_positions_with_moves.sfen"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto result = TsumeCollection::parse(QString::fromUtf8(file.readAll()));
        QVERIFY(result.invalidLines.isEmpty());
        problems = result.problems;
        QCOMPARE(problems.size(), 5);
    }
    void collectionFormats()
    {
        QFile file(QFINDTESTDATA("fixtures/tsume_positions.sfen"));
        QVERIFY(file.open(QIODevice::ReadOnly));
        const auto plain = TsumeCollection::parse(QString::fromUtf8(file.readAll()));
        QCOMPARE(plain.problems.size(), 5);
        for (qsizetype i = 0; i < problems.size(); ++i) {
            QCOMPARE(plain.problems[i].sfen, problems[i].sfen);
            QVERIFY(plain.problems[i].referenceMoves.isEmpty());
        }
        const QString line = problems.first().sfen;
        const auto prefixed = TsumeCollection::parse(QChar(0xfeff) + QStringLiteral("# comment\r\n\nposition sfen ")
            + line + QStringLiteral("\r\nsfen\t") + line + QLatin1Char('\n') + line);
        QCOMPARE(prefixed.problems.size(), 3);
        QCOMPARE(prefixed.problems.first().lineNumber, 3);
        QVERIFY(prefixed.invalidLines.isEmpty());
        // The trailing reference sequence must never be applied to the starting board.
        QCOMPARE(problems.first().referenceMoves.first(), QStringLiteral("3c5c+"));
        QCOMPARE(problems.first().sfen, plain.problems.first().sfen);
    }
    void malformedPositions()
    {
        const QStringList bad{
            QStringLiteral("9/9/9/9/9/9/9/9/9 b - 1"), // no defending king
            QStringLiteral("9/9/9/9/9/9/9/9/K8 b - 1"),
            QStringLiteral("kk7/9/9/9/9/9/9/9/9 b - 1"),
            QStringLiteral("k8/9/9/9/9/9/9/9/9 b 999999999999P 1"),
            QStringLiteral("k8/9/9/9/9/9/9/9/9 b 19P 1"),
            QStringLiteral("k8/9/9/9/9/9/9/9/9 b P0 1"),
            QStringLiteral("k8/9/9/9/9/9/9/9/+9 b - 1"),
            QStringLiteral("+k8/9/9/9/9/9/9/9/9 b - 1"),
            QStringLiteral("k8/9/9/9/9/9/9/9/9 b - 0"),
            problems.first().sfen + QStringLiteral(" garbage"),
            problems.first().sfen + QStringLiteral(" moves resign")};
        const auto result = TsumeCollection::parse(bad.join(QLatin1Char('\n')));
        QVERIFY(result.problems.isEmpty());
        QCOMPARE(result.invalidLines.size(), bad.size());
    }
    void canonicalIdentityAndMateLine()
    {
        const auto& problem = problems.first();
        QCOMPARE(TsumeCollection::positionId(problem.sfen), TsumeCollection::positionId(
                     problem.sfen.section(QLatin1Char(' '), 0, 2) + QStringLiteral(" 123")));
        // 持駒の記述順序の違いも同一局面として扱う。
        QString reordered = problem.sfen;
        reordered.replace(QStringLiteral("2b4g3s4n4l18p"), QStringLiteral("18p4l4n3s4g2b"));
        QCOMPARE(TsumeCollection::positionId(problem.sfen), TsumeCollection::positionId(reordered));
        QVERIFY(TsumeCollection::validMateLine(problem.sfen, problem.referenceMoves));
        QVERIFY(!TsumeCollection::validMateLine(problem.sfen, problem.referenceMoves.mid(0, 3)));
        QVERIFY(!TsumeCollection::validMateLine(problem.sfen, {QStringLiteral("R*9i")}));
    }
    void solutionReplay_data()
    {
        QTest::addColumn<int>("index");
        QTest::addColumn<bool>("external");
        for (int i = 0; i <= 5; ++i) {
            QTest::newRow(qPrintable(QStringLiteral("internal-%1").arg(i))) << i << false;
            QTest::newRow(qPrintable(QStringLiteral("external-%1").arg(i))) << i << true;
        }
    }
    void solutionReplay()
    {
        QFETCH(int, index);
        QFETCH(bool, external);
        const QString sfen = index < 5 ? problems[index].sfen : QStringLiteral("9/9/9/9/9/9/1g7/2g6/K8 w r 1");
        const int plies = index < 5 ? 5 : 1;
        const auto real = qEnvironmentVariable("SHOGI_REAL_KOMORING");
        const QString engine = external ? (real.isEmpty() ? QStringLiteral(MOCK_MATE_EXECUTABLE) : real) : QString();
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        TsumePositionAnalyzer analyzer;
        analyzer.configure(engine, &store);
        const auto id = TsumeCollection::positionId(sfen);
        // 旧形式の「手数のみ」のキャッシュでも手順を新たに取得する。
        store.cache(id, analyzer.engineKey(), {TsumeEvaluation::Status::Mate, plies, {}, {}});
        TsumeSolutionReplay replay;
        replay.configure(sfen, engine, &store);
        QSignalSpy positions(&replay, &TsumeSolutionReplay::positionChanged);
        replay.seek(std::numeric_limits<int>::max(), 10000);
        QTRY_VERIFY_WITH_TIMEOUT(!replay.loading(), 15000);
        QVERIFY2(replay.available(), qPrintable(replay.detail()));
        QCOMPARE(replay.totalPlies(), plies);
        QCOMPARE(replay.currentPly(), plies);
        shogi::Position terminal;
        QVERIFY(terminal.set_sfen(positions.last()[0].toString().toStdString(), true));
        QVERIFY(terminal.is_in_check(terminal.side_to_move()));
        QVERIFY(terminal.generate_legal_moves().empty());
        replay.seek(0, 1000);
        QCOMPARE(positions.last()[0].toString(), sfen);
        QVERIFY(positions.last()[1].toString().isEmpty());
        shogi::Position previous;
        QVERIFY(previous.set_sfen(sfen.toStdString(), true));
        QStringList moves;
        for (int ply = 1; ply <= plies; ++ply) {
            replay.seek(ply, 1000);
            const auto move = positions.last()[1].toString();
            moves.append(move);
            QVERIFY(previous.apply_usi_move(move.toStdString()));
            QCOMPARE(positions.last()[0].toString(), QString::fromStdString(previous.to_sfen()));
            QCOMPARE(replay.currentPly(), ply);
        }
        QVERIFY(TsumeCollection::validMateLine(sfen, moves));
        replay.seek(plies - 1, 1000);
        QCOMPARE(replay.currentPly(), plies - 1);
        replay.seek(-1, 1000);
        QCOMPARE(replay.currentPly(), 0);
        QCOMPARE(store.progress(id).attempts, 0);
        QCOMPARE(store.progress(id).solves, 0);
        const auto cached = store.cached(id, analyzer.engineKey());
        QVERIFY(cached);
        QCOMPARE(cached->pv, moves);
    }
    void solutionReplayFailureAndCancellation()
    {
        qputenv("SHOGI_TEST_MATE_MODE", "malformed");
        TsumeSolutionReplay replay;
        replay.configure(problems[0].sfen, QStringLiteral(MOCK_MATE_EXECUTABLE), nullptr);
        replay.seek(1, 1000);
        QTRY_VERIFY(!replay.loading());
        QVERIFY(!replay.available());
        QVERIFY(!replay.detail().isEmpty());
        qputenv("SHOGI_TEST_MATE_MODE", "slow");
        replay.configure(problems[0].sfen, QStringLiteral(MOCK_MATE_EXECUTABLE), nullptr);
        replay.seek(1, 5000);
        QTest::qWait(100);
        QVERIFY(replay.loading());
        replay.cancel();
        QVERIFY(!replay.loading());
        qunsetenv("SHOGI_TEST_MATE_MODE");
        replay.configure(problems[1].sfen, {}, nullptr);
        replay.seek(0, 5000);
        QSignalSpy positions(&replay, &TsumeSolutionReplay::positionChanged);
        QTRY_VERIFY(replay.available());
        QCOMPARE(positions.last()[0].toString(), problems[1].sfen);
        const int count = static_cast<int>(positions.size());
        QTest::qWait(2100);
        QCOMPARE(positions.size(), count);
        QCOMPARE(replay.currentPly(), 0);
    }
    void persistentProgressAndCache()
    {
        QTemporaryDir data, cache;
        QVERIFY(data.isValid() && cache.isValid());
        const auto id = TsumeCollection::positionId(problems.first().sfen);
        const TsumeEvaluation mate{TsumeEvaluation::Status::Mate, 5, problems.first().referenceMoves, {}};
        {
            TsumeProgressStore store(data.path(), cache.path());
            QVERIFY2(store.open(), qPrintable(store.error()));
            QCOMPARE(store.progress(id).attempts, 0);
            QVERIFY(!store.cached(id, QStringLiteral("engine-A")));
            store.cache(id, QStringLiteral("engine-A"), {});
            QVERIFY(!store.cached(id, QStringLiteral("engine-A"))); // 判定不能は保存しない
            QVERIFY(store.recordAttempt(id));
            QVERIFY(store.recordSolved(id));
            QVERIFY(store.recordAttempt(id)); // 再挑戦で正答済みを取り消さない
            QCOMPARE(store.progress(id).attempts, 2);
            QCOMPARE(store.progress(id).solves, 1);
            QVERIFY(!store.progress(id).lastSolved.isEmpty());
            store.cache(id, QStringLiteral("engine-A"), mate);
            QVERIFY(!store.cached(id, QStringLiteral("engine-B")));
        }
        {
            TsumeProgressStore reopened(data.path(), cache.path());
            QVERIFY(reopened.open());
            QCOMPARE(reopened.progress(id).solves, 1);
            const auto saved = reopened.cached(id, QStringLiteral("engine-A"));
            QVERIFY(saved);
            QCOMPARE(saved->pv, mate.pv);
            reopened.removeCached(id, QStringLiteral("engine-A"));
            QVERIFY(!reopened.cached(id, QStringLiteral("engine-A")));
            QCOMPARE(reopened.progress(id).attempts, 2);
        }
        QVERIFY(QFile::exists(data.filePath(QStringLiteral("tsume_progress.sqlite"))));
        QVERIFY(QFile::exists(cache.filePath(QStringLiteral("tsume_cache.sqlite"))));
        QVERIFY(!QFile::exists(data.filePath(QStringLiteral("tsume_cache.sqlite"))));
        // 再生成可能なキャッシュを削除しても履歴は残る。
        QVERIFY(QFile::remove(cache.filePath(QStringLiteral("tsume_cache.sqlite"))));
        TsumeProgressStore uncached(data.path(), cache.path());
        QVERIFY(uncached.open());
        QCOMPARE(uncached.progress(id).solves, 1);
    }
    void cacheCapacityAndStorageFailure()
    {
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        {
            auto db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), QStringLiteral("cache-capacity-test"));
            db.setDatabaseName(data.filePath(QStringLiteral("tsume_cache.sqlite")));
            QVERIFY(db.open());
            QVERIFY(db.transaction());
            QSqlQuery query(db);
            query.prepare(QStringLiteral("INSERT INTO evaluations VALUES(?, 'test', 2, 0, '[]', ?)"));
            for (int i = 0; i < 10010; ++i) {
                query.bindValue(0, QString::number(i));
                query.bindValue(1, i);
                QVERIFY(query.exec());
            }
            QVERIFY(db.commit());
            store.cache(QStringLiteral("recent"), QStringLiteral("test"), {TsumeEvaluation::Status::NoMate, 0, {}, {}});
            QVERIFY(query.exec(QStringLiteral("SELECT COUNT(*) FROM evaluations")));
            QVERIFY(query.next());
            QCOMPARE(query.value(0).toInt(), 10000);
            QVERIFY(store.cached(QStringLiteral("recent"), QStringLiteral("test")));
            QVERIFY(!store.cached(QStringLiteral("0"), QStringLiteral("test")));
            QVERIFY(QFile(data.filePath(QStringLiteral("tsume_cache.sqlite"))).size() <= 64 * 1024 * 1024);
        }
        QSqlDatabase::removeDatabase(QStringLiteral("cache-capacity-test"));
        QFile occupied(data.filePath(QStringLiteral("not-a-directory")));
        QVERIFY(occupied.open(QIODevice::WriteOnly));
        occupied.close();
        TsumeProgressStore unavailable(occupied.fileName(), data.path());
        QVERIFY(!unavailable.open());
        QVERIFY(!unavailable.error().isEmpty());
        QVERIFY(!unavailable.recordAttempt(QStringLiteral("position")));
    }
    void externalFailures_data()
    {
        QTest::addColumn<QByteArray>("mode");
        for (const auto* mode : {"unsupported", "crash", "slow", "malformed", "timeout"})
            QTest::newRow(mode) << QByteArray(mode);
    }
    void externalFailures()
    {
        QFETCH(QByteArray, mode);
        qputenv("SHOGI_TEST_MATE_MODE", mode);
        TsumePositionAnalyzer analyzer;
        analyzer.configure(QStringLiteral(MOCK_MATE_EXECUTABLE), nullptr);
        QSignalSpy spy(&analyzer, &TsumePositionAnalyzer::finished);
        analyzer.evaluate(QStringLiteral("8k/6G2/7G1/9/9/9/9/9/9 b R 1"), 100);
        QTRY_COMPARE(spy.size(), 1);
        const auto result = qvariant_cast<TsumeEvaluation>(spy.first()[0]);
        QCOMPARE(result.status, TsumeEvaluation::Status::Unknown);
        QVERIFY(!result.detail.isEmpty());
        QTest::qWait(150);
        QCOMPARE(spy.size(), 1);
    }
    void externalCancellationAndCacheValidation()
    {
        TsumePositionAnalyzer analyzer;
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        const QString sfen = QStringLiteral("8k/6G2/7G1/9/9/9/9/9/9 b R 1");
        analyzer.configure(QStringLiteral(MOCK_MATE_EXECUTABLE), &store);
        const auto id = TsumeCollection::positionId(sfen);
        store.cache(id, analyzer.engineKey(), {TsumeEvaluation::Status::Mate, 1, {QStringLiteral("R*9i")}, {}});
        QSignalSpy spy(&analyzer, &TsumePositionAnalyzer::finished);
        analyzer.evaluate(sfen, 3000);
        QTRY_COMPARE(spy.size(), 1);
        auto result = qvariant_cast<TsumeEvaluation>(spy.takeFirst()[0]);
        QCOMPARE(result.status, TsumeEvaluation::Status::Mate);
        QVERIFY(TsumeCollection::validMateLine(sfen, result.pv)); // 壊れたキャッシュを再検証
        analyzer.evaluate(sfen, 3000);
        analyzer.cancel(); // キャッシュの非同期配信も取り消す
        QTest::qWait(20);
        QVERIFY(spy.isEmpty());
        store.removeCached(id, analyzer.engineKey());
        qputenv("SHOGI_TEST_MATE_MODE", "slow");
        analyzer.evaluate(sfen, 3000);
        QTest::qWait(50);
        analyzer.cancel();
        qunsetenv("SHOGI_TEST_MATE_MODE");
        analyzer.evaluate(sfen, 3000);
        QTRY_COMPARE(spy.size(), 1);
        QCOMPARE(qvariant_cast<TsumeEvaluation>(spy.first()[0]).status, TsumeEvaluation::Status::Mate);
        QTest::qWait(2100);
        QCOMPARE(spy.size(), 1); // 古い応答が新局面に届かない
    }
    void externalDefense_data()
    {
        QTest::addColumn<int>("index");
        QTest::addColumn<QString>("move");
        QTest::addColumn<TsumeGameSession::State>("state");
        QTest::addColumn<TsumeGameSession::Outcome>("outcome");
        QTest::newRow("correct") << 0 << QStringLiteral("3c5c+") << TsumeGameSession::State::Ready << TsumeGameSession::Outcome::Solved;
        QTest::newRow("alternative") << 1 << QStringLiteral("R*9g") << TsumeGameSession::State::Ready << TsumeGameSession::Outcome::Solved;
        QTest::newRow("escape") << 2 << QStringLiteral("R*1i") << TsumeGameSession::State::Failed << TsumeGameSession::Outcome::NoMate;
        QTest::newRow("too-long") << 3 << QStringLiteral("S*4b") << TsumeGameSession::State::Failed << TsumeGameSession::Outcome::TooLong;
    }
    void externalDefense()
    {
        QFETCH(int, index);
        QFETCH(QString, move);
        QFETCH(TsumeGameSession::State, state);
        QFETCH(TsumeGameSession::Outcome, outcome);
        TsumeGameSession session;
        // 環境変数を指定すれば同じ回帰を実際のKomoringHeightsでも実行できる。
        const auto real = qEnvironmentVariable("SHOGI_REAL_KOMORING");
        session.configureEngine(real.isEmpty() ? QStringLiteral(MOCK_MATE_EXECUTABLE) : real);
        session.setTimeLimit(10000);
        QSignalSpy spy(&session, &TsumeGameSession::finished);
        QVERIFY(session.start(problems[index].sfen));
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), TsumeGameSession::State::Ready, 15000);
        QCOMPARE(session.remainingPlies(), 5);
        QVERIFY(session.play(move));
        QTRY_COMPARE_WITH_TIMEOUT(session.state(), state, 15000);
        QCOMPARE(session.sfen().section(QLatin1Char(' '), 1, 1), QStringLiteral("b"));
        if (state == TsumeGameSession::State::Failed) {
            QCOMPARE(qvariant_cast<TsumeGameSession::Outcome>(spy.last()[0]), outcome);
            QCOMPARE(spy.last()[1].toInt(), 3);
        } else QCOMPARE(session.remainingPlies(), 3);
        session.undo();
        QCOMPARE(session.state(), TsumeGameSession::State::Ready);
        QCOMPARE(session.sfen(), problems[index].sfen);
        QCOMPARE(session.remainingPlies(), 5);
    }
    void externalUnknownAndTerminalStates()
    {
        TsumeGameSession session;
        session.configureEngine(QStringLiteral(MOCK_MATE_EXECUTABLE));
        QSignalSpy spy(&session, &TsumeGameSession::finished);
        QVERIFY(session.start(problems.first().sfen));
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Ready);
        // 次の問い合わせは別プロセスなので、途中だけ時間切れを再現できる。
        qputenv("SHOGI_TEST_MATE_MODE", "timeout");
        QVERIFY(session.play(QStringLiteral("3c5c+")));
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Paused);
        QCOMPARE(qvariant_cast<TsumeGameSession::Outcome>(spy.last()[0]), TsumeGameSession::Outcome::Inconclusive);
        QVERIFY(session.sfen().contains(QStringLiteral(" w "))); // 未確認の応手を指さない
        qunsetenv("SHOGI_TEST_MATE_MODE");
        session.retry();
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Ready);
        QCOMPARE(session.remainingPlies(), 3);
        QVERIFY(session.start(QStringLiteral("9/9/9/9/9/9/1g7/2g6/K8 w r 1")));
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Ready);
        QCOMPARE(session.remainingPlies(), 1);
        QVERIFY(session.play(QStringLiteral("R*9h")));
        QCOMPARE(session.state(), TsumeGameSession::State::Solved);
        QCOMPARE(session.remainingPlies(), 0);
        QVERIFY(session.start(QStringLiteral("k8/9/9/9/9/9/9/9/9 b - 1")));
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Failed);
        QCOMPARE(qvariant_cast<TsumeGameSession::Outcome>(spy.last()[0]), TsumeGameSession::Outcome::InvalidProblem);
    }
    void allFiveReferenceSolutions()
    {
        std::atomic_bool stop{false};
        for (const auto& problem : std::as_const(problems)) {
            shogi::Position position;
            QVERIFY(position.set_sfen(problem.sfen.toStdString(), true));
            const auto attacker = position.side_to_move();
            shogi::TsumeSearch solver;
            const auto result = solver.solve(position, attacker, 5, 5000, stop);
            QCOMPARE(result.status, shogi::TsumeStatus::Mate);
            QCOMPARE(result.plies, 5);
            for (const auto& move : problem.referenceMoves) QVERIFY(position.apply_usi_move(move.toStdString()));
            QVERIFY(position.is_in_check(position.side_to_move()));
            QVERIFY(position.generate_legal_moves().empty());
        }
    }
    void alternativeAndRefutation()
    {
        std::atomic_bool stop{false};
        shogi::TsumeSearch solver;
        shogi::Position p;
        QVERIFY(p.set_sfen(problems[1].sfen.toStdString(), true));
        QVERIFY(p.apply_usi_move("R*9g")); // alternative to the supplied G*9g
        auto result = solver.solve(p, shogi::Color::Black, 4, 5000, stop);
        QCOMPARE(result.status, shogi::TsumeStatus::Mate);
        QCOMPARE(result.plies, 4);
        QVERIFY(p.apply_usi_move(p.move_to_usi(result.move)));
        QVERIFY(p.set_sfen(problems[2].sfen.toStdString(), true));
        QVERIFY(p.apply_usi_move("R*1i"));
        result = solver.solve(p, shogi::Color::Black, 4, 5000, stop);
        QCOMPARE(result.status, shogi::TsumeStatus::NoMate);
        QCOMPARE(p.move_to_usi(result.move), std::string("1h1i"));
        QVERIFY(p.set_sfen(problems[3].sfen.toStdString(), true));
        QVERIFY(p.apply_usi_move("S*4b")); // seven-ply mate, not five
        result = solver.solve(p, shogi::Color::Black, 4, 5000, stop);
        QCOMPARE(result.status, shogi::TsumeStatus::Limit);
        QVERIFY(result.move.is_valid());
        result = solver.solve(p, shogi::Color::Black, 6, 5000, stop);
        QCOMPARE(result.status, shogi::TsumeStatus::Mate);
        QCOMPARE(result.plies, 6);
        stop.store(true);
        QCOMPARE(solver.solve(p, shogi::Color::Black, 6, 5000, stop).status, shogi::TsumeStatus::Cancelled);
    }
    void rulesAndWhiteAttacker()
    {
        shogi::Position p;
        // A pawn-drop mate is illegal; a rook drop on the same square is legal.
        QVERIFY(p.set_sfen("8k/6G2/7G1/9/9/9/9/9/9 b PR 1", true));
        QVERIFY(!p.apply_usi_move("P*1b"));
        QVERIFY(p.apply_usi_move("R*1b"));
        QVERIFY(p.generate_legal_moves().empty());
        QVERIFY(p.set_sfen("9/9/9/9/9/9/1g7/2g6/K8 w r 1", true));
        std::atomic_bool stop{false};
        shogi::TsumeSearch solver;
        const auto result = solver.solve(p, shogi::Color::White, 1, 1000, stop);
        QCOMPARE(result.status, shogi::TsumeStatus::Mate);
        QCOMPARE(result.plies, 1);
        // Ordinary shogi still requires both kings.
        QVERIFY(!p.set_sfen(problems.first().sfen.toStdString()));
        p.set_startpos();
        QCOMPARE(p.generate_legal_moves().size(), std::size_t(30));
    }
    void engineProtocol()
    {
        QProcess process;
        process.start(QStringLiteral(HAYANAGI_EXECUTABLE), {});
        QVERIFY(process.waitForStarted());
        process.write("usi\n");
        QCOMPARE(response(process, "usiok"), QByteArray("usiok"));
        process.write("setoption name TsumeMode value true\nsetoption name USI_OwnBook value false\nisready\n");
        QCOMPARE(response(process, "readyok"), QByteArray("readyok"));
        process.write("position sfen " + problems.first().sfen.toUtf8() + "\ngo tsume attack depth 5 movetime 5000\n");
        QVERIFY(response(process, "tsume ").startsWith("tsume mate move 3c5c+ plies 5"));
        process.write("position sfen " + problems[2].sfen.toUtf8() + " moves R*1i\ngo tsume defense depth 4 movetime 5000\n");
        QVERIFY(response(process, "tsume ").startsWith("tsume nomate move 1h1i"));
        process.write("position sfen invalid\ngo tsume attack depth 5 movetime 5000\n");
        QCOMPARE(response(process, "tsume "), QByteArray("tsume invalid"));
        process.write("position sfen " + problems.first().sfen.toUtf8() + " moves 3c4c\ngo tsume defense depth 30 movetime 1\n");
        QVERIFY(response(process, "tsume ").startsWith("tsume timeout"));
        process.write("go tsume defense depth 30 movetime 600000\nstop\n");
        QVERIFY(response(process, "tsume ").startsWith("tsume cancelled"));
        process.write("quit\n");
        QVERIFY(process.waitForFinished());
        QCOMPARE(process.exitCode(), 0);
    }
    void sessionSolveAndUndo()
    {
        TsumeGameSession session;
        QSignalSpy outcomes(&session, &TsumeGameSession::finished);
        QVERIFY(session.start(problems.first().sfen));
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Ready);
        QCOMPARE(session.remainingPlies(), 5);
        QVERIFY(!session.play(QStringLiteral("3c3b"))); // non-check / illegal
        QCOMPARE(session.sfen(), problems.first().sfen);
        QVERIFY(session.play(QStringLiteral("3c5c+")));
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Ready);
        QCOMPARE(session.remainingPlies(), 3);
        session.undo();
        QCOMPARE(session.sfen(), problems.first().sfen);
        QCOMPARE(session.remainingPlies(), 5);
        // Follow the independently verified solution against Hayanagi's defense.
        for (const QString& move : {QStringLiteral("3c5c+"), QStringLiteral("1c4c"), QStringLiteral("4c4d")}) {
            QVERIFY(session.play(move));
            QTRY_VERIFY(session.state() != TsumeGameSession::State::Thinking);
        }
        QCOMPARE(session.state(), TsumeGameSession::State::Solved);
        QCOMPARE(outcomes.size(), 1);
        QCOMPARE(qvariant_cast<TsumeGameSession::Outcome>(outcomes[0][0]), TsumeGameSession::Outcome::Solved);
    }
    void sessionWrongMoveAndSwitch()
    {
        TsumeGameSession session;
        QSignalSpy outcomes(&session, &TsumeGameSession::finished);
        for (int i = 0; i < 10; ++i) QVERIFY(session.start(problems[static_cast<qsizetype>(i % 5)].sfen));
        QVERIFY(session.start(problems[2].sfen));
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Ready);
        QVERIFY(session.play(QStringLiteral("R*1i")));
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Failed);
        QCOMPARE(qvariant_cast<TsumeGameSession::Outcome>(outcomes.last()[0]), TsumeGameSession::Outcome::NoMate);
        QVERIFY(session.sfen().startsWith(QStringLiteral("9/9/9/9/9/9/7+S1/5G3/8k b")));
        session.undo();
        QCOMPARE(session.state(), TsumeGameSession::State::Ready);
        QCOMPARE(session.sfen(), problems[2].sfen);
        QVERIFY(session.start(problems[1].sfen));
        session.cancel();
        QCOMPARE(session.state(), TsumeGameSession::State::Paused);
        session.retry();
        QTRY_COMPARE(session.state(), TsumeGameSession::State::Ready);
        QCOMPARE(session.sfen(), problems[1].sfen);
    }
};
QTEST_GUILESS_MAIN(TestTsumePlay)
#include "tst_tsume_play.moc"
