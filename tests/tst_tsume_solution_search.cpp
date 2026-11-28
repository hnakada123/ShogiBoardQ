/// @file tst_tsume_solution_search.cpp
/// @brief 詰将棋の開始局面の内蔵の判定: 証明数探索の最短の手数と手順（TsumeProofNumberSearch）、
///        反復深化の探索との組み合わせ（TsumeSolutionSearch）、判定器・対局・正解手順の再生での長手数の問題

#include <QtTest>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <atomic>

#include "tsume.h"
#include "tsumecollection.h"
#include "tsumeevaluation.h"
#include "tsumegamesession.h"
#include "tsumepositionanalyzer.h"
#include "tsumeprogressstore.h"
#include "tsumeproofnumbersearch.h"
#include "tsumesolutionreplay.h"
#include "tsumesolutionsearch.h"

Q_DECLARE_METATYPE(TsumeEvaluation::Status)

namespace {

using Proof = TsumeProofNumberSearch;
using Status = TsumeEvaluation::Status;

/// 探索が終わるはずの局面に使う判定時間（負荷のかかった環境でも足りる長さ。普通は数十ミリ秒で終わる）
constexpr int kAmpleMs = 60000;
/// 長手数の問題の判定時間。結論はこれを使い切る前に出る
constexpr int kLongProblemMs = 20000;

/// １二歩は打ち歩詰めで指せない（詰まない）。香なら１二香で詰み
QString pawnDropSfen() { return QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b P 1"); }
QString lanceDropSfen() { return QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b L 1"); }

shogi::Position positionOf(const QString& sfen)
{
    shogi::Position position;
    if (!position.set_sfen(sfen.toStdString(), true)) qFatal("bad sfen: %s", qPrintable(sfen));
    return position;
}

/// 探索の手順を USI の指し手の列にする
QStringList usiLine(shogi::Position position, const std::vector<shogi::Move>& line)
{
    QStringList moves;
    for (const shogi::Move& move : line) {
        moves.append(QString::fromStdString(position.move_to_usi(move)));
        position.do_move(move);
    }
    return moves;
}

TsumeCollection::Result readCollection(const char* path)
{
    QFile file(QFINDTESTDATA(path));
    if (!file.open(QIODevice::ReadOnly)) qFatal("cannot read %s", path);
    return TsumeCollection::parse(QString::fromUtf8(file.readAll()));
}

} // namespace

class TestTsumeSolutionSearch : public QObject
{
    Q_OBJECT
    QList<TsumeProblem> m_short;      ///< 5手詰の5問（試験用。盤が空いていて、証明数探索には時間がかかる）
    QList<TsumeProblem> m_seven;      ///< 同梱の7手詰の問題集
    QList<TsumeProblem> m_long;       ///< 同梱の31手詰の問題集

    /// 31手詰の第35問（初手 ３五と = 2f3e）
    const TsumeProblem& problem35() const { return m_long[34]; }

    TsumeEvaluation search(const QString& sfen, int maxPlies, int timeLimitMs, bool requireLine, qint64* elapsed = nullptr)
    {
        TsumeSolutionSearch solution;
        QSignalSpy finished(&solution, &TsumeSolutionSearch::finished);
        QElapsedTimer timer;
        timer.start();
        solution.start(positionOf(sfen), maxPlies, timeLimitMs, requireLine);
        if (!solution.isRunning()) qFatal("the search did not start");
        if (!finished.wait(kAmpleMs)) qFatal("no result");
        if (elapsed) *elapsed = timer.elapsed();
        if (solution.isRunning() || finished.size() != 1) qFatal("the search did not stop");
        return qvariant_cast<TsumeEvaluation>(finished.first()[0]);
    }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<TsumeEvaluation>();
        m_short = readCollection("fixtures/tsume_positions_with_moves.sfen").problems;
        QCOMPARE(m_short.size(), 5);
        m_seven = readCollection("../data/tsumeshogi/tsume_7ply_1000_20261001.txt").problems;
        QCOMPARE(m_seven.size(), 1000);
        m_long = readCollection("../data/tsumeshogi/tsume_31ply_1000_20261127.txt").problems;
        QCOMPARE(m_long.size(), 1000);
        QCOMPARE(problem35().referenceMoves.value(0), QStringLiteral("2f3e"));
    }

    // ---- 証明数探索 ----

    /// 長手数の問題の開始局面: 最短の手数（31手）と初手を確かめ、詰みまでの手順を返す。
    /// どの問題も、手数を限らない探索が最初に見つける詰みは31手より長い（第197問は45手）。
    /// 反復深化の探索は、こうした問題の多くを判定時間（5秒）に判定できない。
    void provesShortestMateOfLongProblems()
    {
        std::atomic_bool stop{false};
        for (int index : {34, 0, 13, 100, 196}) {
            const auto& problem = m_long[index];
            const shogi::Position root = positionOf(problem.sfen);
            const shogi::Color attacker = root.side_to_move();
            const QByteArray where = QStringLiteral("problem %1").arg(index + 1).toUtf8();
            Proof search;
            const auto result = search.solveLine(root, attacker, 31, kAmpleMs, stop);
            QVERIFY2(result.status == Proof::Status::Mate && result.exact, where.constData());
            QVERIFY2(result.plies == 31, where.constData());
            // 検証済みの問題集は、初手が1つだけ
            const QStringList line = usiLine(root, result.line);
            QVERIFY2(line.size() == 31 && line.first() == problem.referenceMoves.first(), where.constData());
            QVERIFY2(QString::fromStdString(root.move_to_usi(result.move)) == line.first(), where.constData());
            QVERIFY2(TsumeCollection::validMateLine(problem.sfen, line), where.constData());

            // 手順を求めなくても、手数を限らなくても、同じ手数
            const auto lengthOnly = search.solve(root, attacker, 31, kAmpleMs, stop);
            QVERIFY2(lengthOnly.status == Proof::Status::Mate && lengthOnly.exact && lengthOnly.plies == 31
                     && lengthOnly.line.empty(), where.constData());
            const auto unlimited = search.solve(root, attacker, Proof::kNoPlyLimit, kAmpleMs, stop);
            QVERIFY2(unlimited.status == Proof::Status::Mate && unlimited.exact && unlimited.plies == 31,
                     where.constData());
            // 2手短い手数では詰まない
            QVERIFY2(search.solve(root, attacker, 29, kAmpleMs, stop).status == Proof::Status::TooLong,
                     where.constData());
        }
    }

    /// 反復深化の探索が判定できる問題では、手数が同じになる。同梱の7手詰の第161〜200問
    /// （第163問・第196問は、手数を限らない探索が最初に見つける詰みが17手・19手）
    void agreesWithIterativeSearch()
    {
        std::atomic_bool stop{false};
        for (int index = 160; index < 200; ++index) {
            const auto& problem = m_seven[index];
            const shogi::Position root = positionOf(problem.sfen);
            const shogi::Color attacker = root.side_to_move();
            const QByteArray where = QStringLiteral("problem %1").arg(index + 1).toUtf8();
            shogi::TsumeSearch iterativeSearch;
            const auto iterative = iterativeSearch.solve(root, attacker, 31, kAmpleMs, stop);
            QVERIFY2(iterative.status == shogi::TsumeStatus::Mate && iterative.plies == 7, where.constData());
            Proof search;
            const auto result = search.solveLine(root, attacker, 31, kAmpleMs, stop);
            QVERIFY2(result.status == Proof::Status::Mate && result.exact, where.constData());
            QVERIFY2(result.plies == iterative.plies, where.constData());
            QVERIFY2(TsumeCollection::validMateLine(problem.sfen, usiLine(root, result.line)), where.constData());
        }
    }

    void lineOfSmallPositions()
    {
        std::atomic_bool stop{false};
        Proof search;
        // 詰まない: 手順は無い
        const shogi::Position noMate = positionOf(pawnDropSfen());
        auto result = search.solveLine(noMate, noMate.side_to_move(), 31, kAmpleMs, stop);
        QCOMPARE(result.status, Proof::Status::NoMate);
        QVERIFY(result.line.empty() && !result.move.is_valid());
        // 1手詰
        const shogi::Position mateInOne = positionOf(lanceDropSfen());
        result = search.solveLine(mateInOne, mateInOne.side_to_move(), 31, kAmpleMs, stop);
        QCOMPARE(result.status, Proof::Status::Mate);
        QVERIFY(result.exact);
        QCOMPARE(usiLine(mateInOne, result.line), QStringList{QStringLiteral("L*1b")});
        // 手数の上限では詰まない: 手順は無い
        const shogi::Position root = positionOf(problem35().sfen);
        result = search.solveLine(root, root.side_to_move(), 29, kAmpleMs, stop);
        QCOMPARE(result.status, Proof::Status::TooLong);
        QVERIFY(result.line.empty());
        // 玉方の手番の局面（初手の後）: 応手から始まる30手の手順
        shogi::Position afterMove = root;
        QVERIFY(afterMove.apply_usi_move("2f3e"));
        result = search.solveLine(afterMove, root.side_to_move(), 30, kAmpleMs, stop);
        QCOMPARE(result.status, Proof::Status::Mate);
        QVERIFY(result.exact);
        const QStringList replies = usiLine(afterMove, result.line);
        QCOMPARE(replies.size(), 30);
        QCOMPARE(replies.first(), QStringLiteral("3d2c"));
        QVERIFY(TsumeCollection::validMateLine(problem35().sfen, QStringList{QStringLiteral("2f3e")} + replies));
    }

    void stopAndTimeLimit()
    {
        // 31手詰の第804問: 最短の手数を確かめるのに20秒ほどかかる
        const shogi::Position hard = positionOf(m_long[803].sfen);
        const shogi::Color attacker = hard.side_to_move();
        std::atomic_bool running{false};
        Proof search;
        QElapsedTimer timer;
        timer.start();
        const auto result = search.solveLine(hard, attacker, 31, 100, running);
        // 詰みを見つけても、最短の手数を確かめられなければ exact にしない（手順も返さない）
        QVERIFY(result.status == Proof::Status::Unknown || (result.status == Proof::Status::Mate && !result.exact));
        QVERIFY(result.line.empty());
        QVERIFY(timer.elapsed() < 5000);
        std::atomic_bool stop{true};
        QCOMPARE(search.solveLine(hard, attacker, 31, kAmpleMs, stop).status, Proof::Status::Cancelled);
    }

    // ---- 反復深化の探索との組み合わせ ----

    void solutionSearch_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<int>("maxPlies");
        QTest::addColumn<bool>("requireLine");
        QTest::addColumn<TsumeEvaluation::Status>("status");
        QTest::addColumn<int>("plies");
        // 短い問題は、反復深化の探索がすぐ結論を出す
        QTest::newRow("short problem") << m_short[0].sfen << 31 << false << Status::Mate << 5;
        QTest::newRow("short problem with line") << m_short[0].sfen << 31 << true << Status::Mate << 5;
        // 長手数の問題は、証明数探索の結論を使う
        QTest::newRow("long problem") << problem35().sfen << 31 << false << Status::Mate << 31;
        QTest::newRow("long problem with line") << problem35().sfen << 31 << true << Status::Mate << 31;
        QTest::newRow("no mate") << pawnDropSfen() << 31 << false << Status::NoMate << 0;
        QTest::newRow("no mate with line") << pawnDropSfen() << 31 << true << Status::NoMate << 0;
        // 手数の上限では詰まないと分かったら、判定時間を待たずに「判定できなかった」と知らせる
        QTest::newRow("longer than the ply limit") << problem35().sfen << 29 << false << Status::Unknown << 0;
        QTest::newRow("longer than the ply limit with line") << problem35().sfen << 29 << true << Status::Unknown << 0;
    }
    void solutionSearch()
    {
        QFETCH(QString, sfen);
        QFETCH(int, maxPlies);
        QFETCH(bool, requireLine);
        QFETCH(TsumeEvaluation::Status, status);
        QFETCH(int, plies);
        qint64 elapsed = 0;
        const TsumeEvaluation result = search(sfen, maxPlies, kLongProblemMs, requireLine, &elapsed);
        QCOMPARE(result.status, status);
        QCOMPARE(result.plies, plies);
        if (requireLine && status == Status::Mate) {
            QCOMPARE(result.pv.size(), plies);
            QVERIFY(TsumeCollection::validMateLine(sfen, result.pv));
        } else {
            QVERIFY(result.pv.isEmpty());
        }
        QVERIFY2(elapsed < kLongProblemMs - 500, "the verdict should not wait for the time limit");
    }

    /// どちらの探索も判定時間内に結論を出せなければ、判定できなかったと知らせる
    void hardProblemTimesOut()
    {
        QCOMPARE(search(m_long[803].sfen, 31, 200, false).status, Status::Unknown);
        QCOMPARE(search(m_long[803].sfen, 31, 200, true).status, Status::Unknown);
    }

    void cancelAndRestart()
    {
        const shogi::Position position = positionOf(problem35().sfen);
        TsumeSolutionSearch solution;
        QSignalSpy finished(&solution, &TsumeSolutionSearch::finished);
        solution.start(position, 31, kLongProblemMs, true);
        solution.cancel();
        QVERIFY(!solution.isRunning());
        // 止めた探索は何も知らせない（証明数探索の結論が出た後の待ち時間を過ぎても）
        QTest::qWait(TsumeSolutionSearch::kExactSearchGraceMs + 300);
        QVERIFY(finished.isEmpty());
        // 始め直すと、前の探索を捨てて新しい結果だけを知らせる
        solution.start(position, 31, kLongProblemMs, true);
        solution.start(position, 31, kLongProblemMs, false);
        QVERIFY(finished.wait(kAmpleMs));
        QTest::qWait(TsumeSolutionSearch::kExactSearchGraceMs + 300);
        QCOMPARE(finished.size(), 1);
        const auto result = qvariant_cast<TsumeEvaluation>(finished.first()[0]);
        QCOMPARE(result.status, Status::Mate);
        QCOMPARE(result.plies, 31);
        QVERIFY(result.pv.isEmpty());
    }

    // ---- 判定器・対局・正解手順の再生 ----

    /// 検証済みの記録もキャッシュも無い長手数の問題を、内蔵の判定で判定できる。
    /// 以前は反復深化の探索だけで、多くの問題が判定時間に収まらなかった
    void analyzerEvaluatesLongProblem()
    {
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        TsumePositionAnalyzer analyzer;
        analyzer.configure({}, &store);
        QSignalSpy finished(&analyzer, &TsumePositionAnalyzer::finished);
        const QString sfen = problem35().sfen;
        QElapsedTimer timer;
        timer.start();
        analyzer.evaluate(sfen, kLongProblemMs);
        QVERIFY(finished.wait(kAmpleMs));
        auto result = qvariant_cast<TsumeEvaluation>(finished.takeFirst()[0]);
        QCOMPARE(result.status, Status::Mate);
        QCOMPARE(result.plies, 31);
        QVERIFY(timer.elapsed() < kLongProblemMs - 500);
        // 手数はキャッシュに残る。手順を頼むと、手順つきで判定し直す
        const auto cached = store.validatedCached(TsumeCollection::positionId(sfen), analyzer.engineKey(), false);
        QVERIFY(cached && cached->status == Status::Mate && cached->plies == 31);
        analyzer.evaluate(sfen, kLongProblemMs, true);
        QVERIFY(finished.wait(kAmpleMs));
        result = qvariant_cast<TsumeEvaluation>(finished.takeFirst()[0]);
        QCOMPARE(result.status, Status::Mate);
        QCOMPARE(result.pv.size(), 31);
        QVERIFY(TsumeCollection::validMateLine(sfen, result.pv));
    }

    /// 検証済みの記録の無い長手数の問題で対局を始め、指し進められる
    void sessionStartsLongProblemWithoutRecord()
    {
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        const auto& problem = problem35();

        using State = TsumeGameSession::State;
        TsumeGameSession session;
        session.configureEngine({}, &store);
        session.setTimeLimit(kLongProblemMs);
        QSignalSpy positions(&session, &TsumeGameSession::positionChanged);
        QVERIFY(session.start(problem.sfen));
        QTRY_VERIFY_WITH_TIMEOUT(session.state() != State::Thinking, kAmpleMs);
        QCOMPARE(session.state(), State::Ready);
        QCOMPARE(session.remainingPlies(), 31);
        // 手順は分かっていないので、応手は1手ごとの判定で決まる
        QVERIFY(session.play(problem.referenceMoves[0]));
        QTRY_VERIFY_WITH_TIMEOUT(session.state() != State::Thinking, kAmpleMs);
        QCOMPARE(session.state(), State::Ready);
        QCOMPARE(positions.last()[1].toString(), problem.referenceMoves[1]);
        QCOMPARE(session.remainingPlies(), 29);
    }

    /// 検証済みの記録の無い長手数の問題の正解手順を、内蔵の判定で再生できる
    void replayShowsSolutionOfLongProblem()
    {
        const QString sfen = problem35().sfen;
        TsumeSolutionReplay replay;
        replay.configure(sfen, {}, nullptr);
        replay.seek(31, kLongProblemMs);
        QVERIFY(replay.loading());
        QTRY_VERIFY_WITH_TIMEOUT(!replay.loading(), kAmpleMs);
        QVERIFY2(replay.available(), qPrintable(replay.detail()));
        QCOMPARE(replay.totalPlies(), 31);
        QCOMPARE(replay.currentPly(), 31);
        QVERIFY(TsumeCollection::validMateLine(sfen, replay.moves()));
    }
};

QTEST_GUILESS_MAIN(TestTsumeSolutionSearch)
#include "tst_tsume_solution_search.moc"
