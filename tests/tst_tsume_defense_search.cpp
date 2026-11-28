/// @file tst_tsume_defense_search.cpp
/// @brief 詰将棋対局の内蔵の判定: 証明数探索（TsumeProofNumberSearch）と、手数つきの探索との組み合わせ（TsumeDefenseSearch）

#include <QtTest>
#include <QElapsedTimer>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <algorithm>
#include <atomic>
#include <string>
#include <vector>

#include "tsume.h"
#include "tsumecollection.h"
#include "tsumedefensesearch.h"
#include "tsumegamesession.h"
#include "tsumeprogressstore.h"
#include "tsumeproofnumbersearch.h"

Q_DECLARE_METATYPE(shogi::TsumeResult)

namespace {

using Proof = TsumeProofNumberSearch;

/// 探索が終わるはずの局面に使う判定時間（負荷のかかった環境でも足りる長さ。普通は数十ミリ秒で終わる）
constexpr int kAmpleMs = 60000;

shogi::Position positionOf(const QString& sfen)
{
    shogi::Position position;
    if (!position.set_sfen(sfen.toStdString(), true)) qFatal("bad sfen: %s", qPrintable(sfen));
    return position;
}

Proof::Result prove(const shogi::Position& position, shogi::Color attacker, int timeLimitMs = kAmpleMs)
{
    std::atomic_bool stop{false};
    Proof search;
    return search.solve(position, attacker, timeLimitMs, stop);
}

bool isLegal(const shogi::Position& position, const shogi::Move& move)
{
    if (!move.is_valid()) return false;
    const std::string usi = position.move_to_usi(move);
    const auto legal = position.generate_legal_moves();
    return std::any_of(legal.begin(), legal.end(),
                       [&](const shogi::Move& candidate) { return position.move_to_usi(candidate) == usi; });
}

} // namespace

class TestTsumeDefenseSearch : public QObject
{
    Q_OBJECT
    QList<TsumeProblem> m_short;      ///< 5手詰の5問（試験用）
    QByteArray m_longContents;        ///< 同梱の31手詰の問題集
    TsumeCollection::Result m_long;

    /// 31手詰の第35問（初手 ３五と = 2f3e）
    const TsumeProblem& problem35() const { return m_long.problems[34]; }

private slots:
    void initTestCase()
    {
        qRegisterMetaType<shogi::TsumeResult>();
        QFile fixture(QFINDTESTDATA("fixtures/tsume_positions_with_moves.sfen"));
        QVERIFY(fixture.open(QIODevice::ReadOnly));
        m_short = TsumeCollection::parse(QString::fromUtf8(fixture.readAll())).problems;
        QCOMPARE(m_short.size(), 5);
        QFile bundled(QFINDTESTDATA("../data/tsumeshogi/tsume_31ply_1000_20261127.txt"));
        QVERIFY(bundled.open(QIODevice::ReadOnly));
        m_longContents = bundled.readAll();
        m_long = TsumeCollection::parse(QString::fromUtf8(m_longContents));
        QCOMPARE(m_long.problems.size(), 1000);
        QCOMPARE(problem35().referenceMoves.value(0), QStringLiteral("2f3e"));
    }

    // ---- 証明数探索 ----

    /// 詰む局面を「詰まない」としない: 開始局面と、正解手順の攻め方の手の後の局面はすべて詰み
    void provesMatesOnTheMainLine()
    {
        QList<TsumeProblem> sample = m_short;
        for (int index : {0, 13, 34, 100, 275}) sample.append(m_long.problems[index]);
        for (const auto& problem : std::as_const(sample)) {
            shogi::Position position = positionOf(problem.sfen);
            const shogi::Color attacker = position.side_to_move();
            QCOMPARE(prove(position, attacker).status, Proof::Status::Mate);
            for (qsizetype ply = 0; ply < problem.referenceMoves.size(); ++ply) {
                QVERIFY(position.apply_usi_move(problem.referenceMoves[ply].toStdString()));
                if (ply % 2 != 0) continue;
                const auto result = prove(position, attacker);
                QVERIFY2(result.status == Proof::Status::Mate,
                         qPrintable(QStringLiteral("line %1 ply %2").arg(problem.lineNumber).arg(ply)));
            }
        }
    }

    /// 31手詰の正解以外の初手は、どれも詰まない（検証済みの問題集は、正解以外の攻め方の手に詰みが無い）。
    /// 手数つきの探索は、第35問の ４三金-３三・４三金-４四・３六成桂-３五 を5秒では判定できなかった。
    void disprovesWrongFirstMoves()
    {
        for (int index : {34, 0, 13}) {
            const auto& problem = m_long.problems[index];
            const shogi::Position root = positionOf(problem.sfen);
            const shogi::Color attacker = root.side_to_move();
            int wrongMoves = 0;
            for (const auto& move : root.generate_checking_moves()) {
                const QString usi = QString::fromStdString(root.move_to_usi(move));
                if (usi == problem.referenceMoves[0]) continue;
                ++wrongMoves;
                shogi::Position afterMove = root;
                afterMove.do_move(move);
                const auto result = prove(afterMove, attacker);
                const QByteArray where = QStringLiteral("problem %1 move %2").arg(index + 1).arg(usi).toUtf8();
                QVERIFY2(result.status == Proof::Status::NoMate, where.constData());
                // 詰みを防ぐ応手は合法で、その後の局面（攻め方の手番）も詰まない
                QVERIFY2(isLegal(afterMove, result.refutation), where.constData());
                shogi::Position afterReply = afterMove;
                afterReply.do_move(result.refutation);
                QVERIFY2(prove(afterReply, attacker).status == Proof::Status::NoMate, where.constData());
            }
            QVERIFY(wrongMoves > 0);
        }
    }

    /// 手数つきの探索が結論を出せる局面では、その結論と食い違わない（詰み・不詰のどちらも）。
    /// 31手詰の正解手順の終わりの5手（5手詰の局面）で、全部の王手を比べる
    void agreesWithExactSearch()
    {
        std::atomic_bool stop{false};
        int mates = 0;
        int noMates = 0;
        for (int index : {0, 13, 34, 100, 200, 300, 400, 500}) {
            const auto& problem = m_long.problems[index];
            shogi::Position root = positionOf(problem.sfen);
            const shogi::Color attacker = root.side_to_move();
            for (qsizetype ply = 0; ply + 5 < problem.referenceMoves.size(); ++ply) {
                QVERIFY(root.apply_usi_move(problem.referenceMoves[ply].toStdString()));
            }
            for (const auto& move : root.generate_checking_moves()) {
                shogi::Position afterMove = root;
                afterMove.do_move(move);
                shogi::TsumeSearch exactSearch;
                // 手数つきの探索は、王手が続かなくなるまで調べられたときだけ「詰まない」と結論する。
                // 長くかかる手は比べない
                const auto exact = exactSearch.solve(afterMove, attacker, 10, 300, stop);
                const auto proof = prove(afterMove, attacker);
                const QByteArray where = QStringLiteral("problem %1 move %2")
                    .arg(index + 1).arg(QString::fromStdString(root.move_to_usi(move))).toUtf8();
                if (exact.status == shogi::TsumeStatus::Mate) {
                    ++mates;
                    QVERIFY2(proof.status == Proof::Status::Mate, where.constData());
                } else if (exact.status == shogi::TsumeStatus::NoMate) {
                    ++noMates;
                    QVERIFY2(proof.status == Proof::Status::NoMate, where.constData());
                } else {
                    // 10手以内に詰まないか、時間切れ: 長い詰みがあるか、詰まないか。どちらかの結論は出る
                    QVERIFY2(proof.status == Proof::Status::Mate || proof.status == Proof::Status::NoMate,
                             where.constData());
                }
            }
        }
        QVERIFY2(mates >= 8 && noMates >= 8, qPrintable(QStringLiteral("mates %1, no mates %2").arg(mates).arg(noMates)));
    }

    void rules_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<bool>("mate");
        // 王手は続けられるが詰まない（同じ局面に戻るだけ）。手数つきの探索は「詰まない」と言い切れない
        QTest::newRow("perpetual check") << QStringLiteral("4k4/9/9/9/9/9/9/9/4R4 b - 1") << false;
        // １二歩と打てば玉は動けないが、打ち歩詰めは指せない
        QTest::newRow("pawn drop mate") << QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b P 1") << false;
        // 同じ形で香なら１二香で詰み
        QTest::newRow("lance drop mate") << QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b L 1") << true;
        // 王手をかけられる駒が無い
        QTest::newRow("no check") << QStringLiteral("4k4/9/9/9/9/9/9/9/G8 b - 1") << false;
    }
    void rules()
    {
        QFETCH(QString, sfen);
        QFETCH(bool, mate);
        const shogi::Position position = positionOf(sfen);
        const auto result = prove(position, position.side_to_move());
        QCOMPARE(result.status, mate ? Proof::Status::Mate : Proof::Status::NoMate);
        // 攻め方の手番の局面には、詰みを防ぐ応手は無い
        QVERIFY(!result.refutation.is_valid());
    }

    void stopAndTimeLimit()
    {
        // 第276問で ３二-２二 の後は、不詰の確認に数十秒かかる
        shogi::Position hard = positionOf(m_long.problems[275].sfen);
        const shogi::Color attacker = hard.side_to_move();
        QVERIFY(hard.apply_usi_move("3b2b"));
        QElapsedTimer timer;
        timer.start();
        QCOMPARE(prove(hard, attacker, 50).status, Proof::Status::Unknown);
        QVERIFY(timer.elapsed() < 5000);

        std::atomic_bool stop{true};
        Proof search;
        QCOMPARE(search.solve(hard, attacker, kAmpleMs, stop).status, Proof::Status::Cancelled);
        std::atomic_bool running{false};
        QCOMPARE(search.solve(hard, attacker, kAmpleMs, running, &stop).status, Proof::Status::Cancelled);
        // 同じインスタンスで続けて別の局面を調べられる
        const shogi::Position easy = positionOf(QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b L 1"));
        QCOMPARE(search.solve(easy, easy.side_to_move(), kAmpleMs, running).status, Proof::Status::Mate);
    }

    // ---- 手数つきの探索との組み合わせ ----

    /// すぐ終わる短い問題は、手数つきの探索の結論（最長の応手・手数）をそのまま使う
    void shortProblemsUseTheExactSearch()
    {
        TsumeDefenseSearch search;
        QSignalSpy finished(&search, &TsumeDefenseSearch::finished);
        const auto run = [&](const TsumeProblem& problem, const char* move, int remaining) {
            shogi::Position position = positionOf(problem.sfen);
            const shogi::Color attacker = position.side_to_move();
            if (!position.apply_usi_move(move)) qFatal("bad move");
            finished.clear();
            search.start(position, attacker, remaining, 5000);
            if (!finished.wait(kAmpleMs)) qFatal("no result");
            const auto result = qvariant_cast<shogi::TsumeResult>(finished.first()[0]);
            return std::make_pair(result, result.move.is_valid() ? position.move_to_usi(result.move) : std::string());
        };
        // 逃れる手: 1h1i で詰まない
        auto [refuted, refutation] = run(m_short[2], "R*1i", 4);
        QCOMPARE(refuted.status, shogi::TsumeStatus::NoMate);
        QCOMPARE(refutation, std::string("1h1i"));
        // 7手かかる手: 残り4手では詰まない（詰みはあるので「詰まない」にはしない）
        auto [tooLong, longReply] = run(m_short[3], "S*4b", 4);
        QCOMPARE(tooLong.status, shogi::TsumeStatus::Limit);
        QVERIFY(!longReply.empty());
        auto [mate, mateReply] = run(m_short[3], "S*4b", 6);
        QCOMPARE(mate.status, shogi::TsumeStatus::Mate);
        QCOMPARE(mate.plies, 6);
        QVERIFY(!mateReply.empty());
        QVERIFY(!search.isRunning());
    }

    /// 長手数の問題で正解以外の手を指した後は、判定時間を使い切る前に「詰まない」と応手を知らせる
    void wrongMoveInLongProblemIsRefuted_data()
    {
        QTest::addColumn<int>("timeLimit");
        QTest::newRow("default") << 5000;
        // 判定時間が、手数つきの探索の結論を待つ時間より短くても同じ
        QTest::newRow("shorter than grace") << 400;
    }
    void wrongMoveInLongProblemIsRefuted()
    {
        QFETCH(int, timeLimit);
        shogi::Position position = positionOf(problem35().sfen);
        const shogi::Color attacker = position.side_to_move();
        QVERIFY(position.apply_usi_move("4c3c"));

        TsumeDefenseSearch search;
        QSignalSpy finished(&search, &TsumeDefenseSearch::finished);
        QElapsedTimer timer;
        timer.start();
        search.start(position, attacker, 30, timeLimit);
        QVERIFY(search.isRunning());
        QVERIFY(finished.wait(kAmpleMs));
        const auto result = qvariant_cast<shogi::TsumeResult>(finished.first()[0]);
        QCOMPARE(result.status, shogi::TsumeStatus::NoMate);
        QVERIFY(isLegal(position, result.move));
        if (timeLimit >= 5000) QVERIFY2(timer.elapsed() < timeLimit - 500, "the refutation should not wait for the time limit");
        QVERIFY(!search.isRunning());
        QCOMPARE(finished.size(), 1);
    }

    /// 詰みがあることしか分からないとき（手数が分からない）は、判定できなかったと知らせる
    void correctMoveInLongProblemTimesOut()
    {
        shogi::Position position = positionOf(problem35().sfen);
        const shogi::Color attacker = position.side_to_move();
        QVERIFY(position.apply_usi_move("2f3e"));
        TsumeDefenseSearch search;
        QSignalSpy finished(&search, &TsumeDefenseSearch::finished);
        search.start(position, attacker, 30, 300);
        QVERIFY(finished.wait(kAmpleMs));
        QCOMPARE(qvariant_cast<shogi::TsumeResult>(finished.first()[0]).status, shogi::TsumeStatus::Timeout);
    }

    void cancelAndRestart()
    {
        shogi::Position position = positionOf(problem35().sfen);
        const shogi::Color attacker = position.side_to_move();
        QVERIFY(position.apply_usi_move("4c3c"));
        TsumeDefenseSearch search;
        QSignalSpy finished(&search, &TsumeDefenseSearch::finished);
        search.start(position, attacker, 30, 5000);
        search.cancel();
        QVERIFY(!search.isRunning());
        // 止めた探索は何も知らせない（「詰まない」と分かった後の待ち時間を過ぎても）
        QTest::qWait(TsumeDefenseSearch::kExactSearchGraceMs + 300);
        QVERIFY(finished.isEmpty());
        // 始め直すと、前の探索を捨てて新しい結果だけを知らせる
        search.start(position, attacker, 30, 5000);
        search.start(position, attacker, 30, 5000);
        QVERIFY(finished.wait(kAmpleMs));
        QTest::qWait(200);
        QCOMPARE(finished.size(), 1);
        QCOMPARE(qvariant_cast<shogi::TsumeResult>(finished.first()[0]).status, shogi::TsumeStatus::NoMate);
    }

    // ---- 対局 ----

    /// 31手詰の第35問で正解以外の初手を指すと、以前は判定時間（5秒）の後に「判定が完了していません」になった。
    /// 詰みを防ぐ応手を指して不正解を知らせ、一手戻せば正解手順の上に戻る。
    void sessionRefutesWrongMoveInLongProblem()
    {
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        store.setVerifiedCollection(m_longContents, m_long);
        const auto& problem = problem35();

        using State = TsumeGameSession::State;
        TsumeGameSession session;
        session.configureEngine({}, &store);
        QSignalSpy positions(&session, &TsumeGameSession::positionChanged);
        QSignalSpy outcomes(&session, &TsumeGameSession::finished);
        QVERIFY(session.start(problem.sfen));
        QTRY_COMPARE(session.state(), State::Ready);
        QCOMPARE(session.remainingPlies(), 31);

        for (const char* wrong : {"4c3c", "4c4d", "3f3e"}) {
            outcomes.clear();
            QElapsedTimer timer;
            timer.start();
            QVERIFY(session.play(QString::fromLatin1(wrong)));
            QTRY_VERIFY_WITH_TIMEOUT(session.state() != State::Thinking, kAmpleMs);
            QCOMPARE(session.state(), State::Failed);
            QVERIFY2(timer.elapsed() < 4500, wrong);
            QCOMPARE(outcomes.size(), 1);
            QCOMPARE(qvariant_cast<TsumeGameSession::Outcome>(outcomes.first()[0]), TsumeGameSession::Outcome::NoMate);
            // 玉方の応手を盤に指している
            const QString reply = positions.last()[1].toString();
            QVERIFY(!reply.isEmpty() && reply != QLatin1String(wrong));
            session.undo();
            QCOMPARE(session.state(), State::Ready);
            QCOMPARE(session.sfen(), problem.sfen);
            QCOMPARE(session.remainingPlies(), 31);
        }
        // 正解の手には、手順の応手を指す
        QVERIFY(session.play(problem.referenceMoves[0]));
        QTRY_COMPARE(session.state(), State::Ready);
        QCOMPARE(positions.last()[1].toString(), problem.referenceMoves[1]);
        QCOMPARE(session.remainingPlies(), 29);
    }
};

QTEST_GUILESS_MAIN(TestTsumeDefenseSearch)
#include "tst_tsume_defense_search.moc"
