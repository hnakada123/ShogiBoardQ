#include <QtTest>
#include <QFile>
#include <QSignalSpy>
#include <QProcess>
#include <QElapsedTimer>
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
