/// @file tst_tsume_defense_search.cpp
/// @brief 詰将棋対局の内蔵の判定: 証明数探索（TsumeProofNumberSearch）と、反復深化の探索との組み合わせ（TsumeDefenseSearch）

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
#include "tsumeevaluation.h"
#include "tsumegamesession.h"
#include "tsumepositionanalyzer.h"
#include "tsumeprogressstore.h"
#include "tsumeproofnumbersearch.h"

Q_DECLARE_METATYPE(shogi::TsumeResult)
Q_DECLARE_METATYPE(shogi::TsumeStatus)
Q_DECLARE_METATYPE(TsumeProofNumberSearch::Status)

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

Proof::Result prove(const shogi::Position& position, shogi::Color attacker, int maxPlies = Proof::kNoPlyLimit,
                    int timeLimitMs = kAmpleMs)
{
    std::atomic_bool stop{false};
    Proof search;
    return search.solve(position, attacker, maxPlies, timeLimitMs, stop);
}

bool isLegal(const shogi::Position& position, const shogi::Move& move)
{
    if (!move.is_valid()) return false;
    const std::string usi = position.move_to_usi(move);
    const auto legal = position.generate_legal_moves();
    return std::any_of(legal.begin(), legal.end(),
                       [&](const shogi::Move& candidate) { return position.move_to_usi(candidate) == usi; });
}

QString usiOf(const shogi::Position& position, const shogi::Move& move)
{
    return move.is_valid() ? QString::fromStdString(position.move_to_usi(move)) : QString();
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
    /// 31手詰の第276問で ３二-２二 の後: 詰まないことの確認に10秒以上かかる局面
    shogi::Position hardPosition(shogi::Color& attacker) const
    {
        shogi::Position position = positionOf(m_long.problems[275].sfen);
        attacker = position.side_to_move();
        if (!position.apply_usi_move("3b2b")) qFatal("bad move");
        return position;
    }

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

    /// 正解手順の攻め方の手の後の局面（玉方の手番）は、残りの手数ちょうどで詰み、いちばん長く粘る応手は手順の応手。
    /// 残りの手数より2手短い手数では詰まない（検証済みの問題集の手数は最短）。
    /// 反復深化の探索は、第35問の初手の後の局面を10分かけても判定できなかった。
    void judgesTheMainLineOfLongProblems()
    {
        for (int index : {34, 0, 13, 100, 275}) {
            const auto& problem = m_long.problems[index];
            shogi::Position position = positionOf(problem.sfen);
            const shogi::Color attacker = position.side_to_move();
            // 開始局面（攻め方の手番）も、問題の手数以内に詰む
            const auto start = prove(position, attacker, 31);
            QCOMPARE(start.status, Proof::Status::Mate);
            QCOMPARE(start.plies, 31);
            for (qsizetype ply = 0; ply + 1 < problem.referenceMoves.size(); ply += 2) {
                QVERIFY(position.apply_usi_move(problem.referenceMoves[ply].toStdString()));
                const int remaining = static_cast<int>(problem.referenceMoves.size() - ply - 1);
                const QByteArray where = QStringLiteral("problem %1 ply %2").arg(index + 1).arg(ply).toUtf8();
                const auto result = prove(position, attacker, remaining);
                QVERIFY2(result.status == Proof::Status::Mate, where.constData());
                QVERIFY2(result.plies == remaining && result.exact, where.constData());
                QVERIFY2(usiOf(position, result.reply) == problem.referenceMoves[ply + 1], where.constData());
                if (remaining >= 4) {
                    const auto shorter = prove(position, attacker, remaining - 2);
                    QVERIFY2(shorter.status == Proof::Status::TooLong, where.constData());
                    QVERIFY2(isLegal(position, shorter.reply), where.constData());
                }
                QVERIFY(position.apply_usi_move(problem.referenceMoves[ply + 1].toStdString()));
            }
        }
    }

    /// 31手詰の正解以外の初手は、どれも詰まない（検証済みの問題集は、正解以外の攻め方の手に詰みが無い）
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
                const auto result = prove(afterMove, attacker, 30);
                const QByteArray where = QStringLiteral("problem %1 move %2").arg(index + 1).arg(usi).toUtf8();
                QVERIFY2(result.status == Proof::Status::NoMate, where.constData());
                // 詰みを防ぐ応手は合法で、その後の局面（攻め方の手番）も詰まない
                QVERIFY2(isLegal(afterMove, result.reply), where.constData());
                shogi::Position afterReply = afterMove;
                afterReply.do_move(result.reply);
                QVERIFY2(prove(afterReply, attacker).status == Proof::Status::NoMate, where.constData());
            }
            QVERIFY(wrongMoves > 0);
        }
    }

    /// 反復深化の探索が結論を出せる局面では、その結論と食い違わない（詰みの手数・手数内に詰まない・詰まない）。
    /// 31手詰の正解手順の終わりの7手（7手詰の局面）で、全部の王手の後の局面を、手数 4・6・8 で比べる
    void agreesWithIterativeSearch()
    {
        std::atomic_bool stop{false};
        int mates = 0;
        int tooLong = 0;
        int noMates = 0;
        for (int index : {0, 13, 34, 100, 200, 300, 400, 500}) {
            const auto& problem = m_long.problems[index];
            shogi::Position root = positionOf(problem.sfen);
            const shogi::Color attacker = root.side_to_move();
            for (qsizetype ply = 0; ply + 7 < problem.referenceMoves.size(); ++ply) {
                QVERIFY(root.apply_usi_move(problem.referenceMoves[ply].toStdString()));
            }
            for (const auto& move : root.generate_checking_moves()) {
                shogi::Position afterMove = root;
                afterMove.do_move(move);
                for (int maxPlies : {4, 6, 8}) {
                    shogi::TsumeSearch iterativeSearch;
                    // 長くかかる手は比べない
                    const auto iterative = iterativeSearch.solve(afterMove, attacker, maxPlies, 300, stop);
                    const auto proof = prove(afterMove, attacker, maxPlies);
                    const QByteArray where = QStringLiteral("problem %1 move %2 plies %3")
                        .arg(index + 1).arg(QString::fromStdString(root.move_to_usi(move))).arg(maxPlies).toUtf8();
                    if (iterative.status == shogi::TsumeStatus::Mate) {
                        ++mates;
                        QVERIFY2(proof.status == Proof::Status::Mate && proof.exact, where.constData());
                        QVERIFY2(proof.plies == iterative.plies, where.constData());
                        // いちばん長く粘る応手: その後の局面は、ちょうど1手少ない手数で詰む
                        if (iterative.plies == 0) continue;
                        shogi::Position afterReply = afterMove;
                        afterReply.do_move(proof.reply);
                        shogi::TsumeSearch replySearch;
                        const auto reply = replySearch.solve(afterReply, attacker, iterative.plies - 1, kAmpleMs, stop);
                        QVERIFY2(reply.status == shogi::TsumeStatus::Mate && reply.plies == iterative.plies - 1,
                                 where.constData());
                    } else if (iterative.status == shogi::TsumeStatus::NoMate) {
                        ++noMates;
                        QVERIFY2(proof.status == Proof::Status::NoMate, where.constData());
                    } else if (iterative.status == shogi::TsumeStatus::Limit) {
                        ++tooLong;
                        // 手数内に詰まない。何手かけても詰まないことまで分かることもある
                        QVERIFY2(proof.status == Proof::Status::TooLong || proof.status == Proof::Status::NoMate,
                                 where.constData());
                    }
                }
            }
        }
        QVERIFY2(mates >= 8 && tooLong >= 8 && noMates >= 8,
                 qPrintable(QStringLiteral("mates %1, too long %2, no mates %3").arg(mates).arg(tooLong).arg(noMates)));
    }

    void rules_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<int>("maxPlies");
        QTest::addColumn<Proof::Status>("status");
        QTest::addColumn<int>("plies");
        const int unlimited = Proof::kNoPlyLimit;
        // 王手は続けられるが詰まない（同じ局面に戻るだけ）。反復深化の探索は「詰まない」と言い切れない
        const QString perpetual = QStringLiteral("4k4/9/9/9/9/9/9/9/4R4 b - 1");
        QTest::newRow("perpetual check") << perpetual << unlimited << Proof::Status::NoMate << 0;
        QTest::newRow("perpetual check within 9") << perpetual << 9 << Proof::Status::NoMate << 0;
        // １二歩と打てば玉は動けないが、打ち歩詰めは指せない
        QTest::newRow("pawn drop mate") << QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b P 1") << unlimited
                                         << Proof::Status::NoMate << 0;
        // 同じ形で香なら１二香で詰み
        const QString lance = QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b L 1");
        QTest::newRow("lance drop mate") << lance << unlimited << Proof::Status::Mate << 1;
        QTest::newRow("lance drop mate within 1") << lance << 1 << Proof::Status::Mate << 1;
        // 手数が残っていなければ、1手詰でも詰ませられない
        QTest::newRow("lance drop mate within 0") << lance << 0 << Proof::Status::TooLong << 0;
        // 王手をかけられる駒が無い
        QTest::newRow("no check") << QStringLiteral("4k4/9/9/9/9/9/9/9/G8 b - 1") << unlimited
                                   << Proof::Status::NoMate << 0;
    }
    void rules()
    {
        QFETCH(QString, sfen);
        QFETCH(int, maxPlies);
        QFETCH(Proof::Status, status);
        QFETCH(int, plies);
        const shogi::Position position = positionOf(sfen);
        const auto result = prove(position, position.side_to_move(), maxPlies);
        QCOMPARE(result.status, status);
        QCOMPARE(result.plies, plies);
        // 攻め方の手番の局面には、玉方の応手は無い
        QVERIFY(!result.reply.is_valid());
    }

    /// 玉方の手番で手数が残っていないとき: 詰んでいれば詰み、応手があれば手数内に詰まない
    void noPliesLeft()
    {
        shogi::Position mated = positionOf(QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b L 1"));
        QVERIFY(mated.apply_usi_move("L*1b"));
        auto result = prove(mated, shogi::Color::Black, 0);
        QCOMPARE(result.status, Proof::Status::Mate);
        QCOMPARE(result.plies, 0);
        QVERIFY(result.exact);
        QVERIFY(!result.reply.is_valid());

        shogi::Position escaping = positionOf(QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b L 1"));
        QVERIFY(escaping.apply_usi_move("2c1b"));    // 金で王手。同玉と取れる
        result = prove(escaping, shogi::Color::Black, 0);
        QCOMPARE(result.status, Proof::Status::TooLong);
        QVERIFY(isLegal(escaping, result.reply));
    }

    void stopAndTimeLimit()
    {
        shogi::Color attacker = shogi::Color::Black;
        const shogi::Position hard = hardPosition(attacker);
        QElapsedTimer timer;
        timer.start();
        QCOMPARE(prove(hard, attacker, 30, 50).status, Proof::Status::Unknown);
        QCOMPARE(prove(hard, attacker, Proof::kNoPlyLimit, 50).status, Proof::Status::Unknown);
        QVERIFY(timer.elapsed() < 5000);

        std::atomic_bool stop{true};
        Proof search;
        QCOMPARE(search.solve(hard, attacker, 30, kAmpleMs, stop).status, Proof::Status::Cancelled);
        std::atomic_bool running{false};
        QCOMPARE(search.solve(hard, attacker, 30, kAmpleMs, running, &stop).status, Proof::Status::Cancelled);
        // 同じインスタンスで続けて別の局面を調べられる
        const shogi::Position easy = positionOf(QStringLiteral("7nk/7p1/7G1/9/9/9/9/9/9 b L 1"));
        QCOMPARE(search.solve(easy, easy.side_to_move(), 1, kAmpleMs, running).status, Proof::Status::Mate);
    }

    // ---- 反復深化の探索との組み合わせ ----

    /// すぐ終わる短い問題は、反復深化の探索の結論（いちばん長く粘る応手・手数）をそのまま使う
    void shortProblemsUseTheIterativeSearch()
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

    /// 長手数の問題（31手詰の第35問）は、反復深化の探索では判定時間に収まらない。証明数探索の結論を、
    /// 判定時間を使い切る前に知らせる
    void longProblemUsesTheProofNumberSearch_data()
    {
        QTest::addColumn<QString>("move");
        QTest::addColumn<int>("remaining");
        QTest::addColumn<int>("timeLimit");
        QTest::addColumn<shogi::TsumeStatus>("status");
        QTest::addColumn<int>("plies");
        QTest::addColumn<QString>("reply");
        // 正解以外の手: 何手かけても詰まない
        QTest::newRow("wrong move") << QStringLiteral("4c3c") << 30 << 5000 << shogi::TsumeStatus::NoMate << 0 << QString();
        // 判定時間が、反復深化の探索の結論を待つ時間より短くても同じ
        QTest::newRow("wrong move, short time") << QStringLiteral("4c3c") << 30 << 400 << shogi::TsumeStatus::NoMate << 0
                                                << QString();
        // 正解の手: 残り30手で詰み、いちばん長く粘る応手は ２三玉
        QTest::newRow("correct move") << QStringLiteral("2f3e") << 30 << 5000 << shogi::TsumeStatus::Mate << 30
                                      << QStringLiteral("3d2c");
        // 残りの手数より長い詰み: 手数内には詰まない
        QTest::newRow("longer mate") << QStringLiteral("2f3e") << 28 << 5000 << shogi::TsumeStatus::Limit << 0 << QString();
    }
    void longProblemUsesTheProofNumberSearch()
    {
        QFETCH(QString, move);
        QFETCH(int, remaining);
        QFETCH(int, timeLimit);
        QFETCH(shogi::TsumeStatus, status);
        QFETCH(int, plies);
        QFETCH(QString, reply);
        shogi::Position position = positionOf(problem35().sfen);
        const shogi::Color attacker = position.side_to_move();
        QVERIFY(position.apply_usi_move(move.toStdString()));

        TsumeDefenseSearch search;
        QSignalSpy finished(&search, &TsumeDefenseSearch::finished);
        QElapsedTimer timer;
        timer.start();
        search.start(position, attacker, remaining, timeLimit);
        QVERIFY(search.isRunning());
        QVERIFY(finished.wait(kAmpleMs));
        const auto result = qvariant_cast<shogi::TsumeResult>(finished.first()[0]);
        QCOMPARE(result.status, status);
        QCOMPARE(result.plies, plies);
        QVERIFY(isLegal(position, result.move));
        if (!reply.isEmpty()) QCOMPARE(usiOf(position, result.move), reply);
        if (timeLimit >= 5000) QVERIFY2(timer.elapsed() < timeLimit - 500, "the verdict should not wait for the time limit");
        QVERIFY(!search.isRunning());
        QCOMPARE(finished.size(), 1);
    }

    /// どちらの探索も判定時間内に結論を出せなければ、判定できなかったと知らせる
    void hardPositionTimesOut()
    {
        shogi::Color attacker = shogi::Color::Black;
        const shogi::Position position = hardPosition(attacker);
        TsumeDefenseSearch search;
        QSignalSpy finished(&search, &TsumeDefenseSearch::finished);
        search.start(position, attacker, 30, 200);
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
        // 止めた探索は何も知らせない（証明数探索の結論が出た後の待ち時間を過ぎても）
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

    /// 31手詰の第35問で正解以外の初手を指すと、以前は判定時間の後に「判定が完了していません」になった。
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

    /// 詰みまでの手順が分かっていない長手数の問題（手数だけを判定済み）でも、1手ごとの判定で最後まで指せる。
    /// 以前は、正解の手を指しても反復深化の探索が判定時間に収まらず、対局が進まなかった。
    void sessionPlaysLongProblemWithoutKnownLine()
    {
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        const auto& problem = problem35();
        const QStringList& line = problem.referenceMoves;
        store.cache(m_long.positionIds[34], TsumePositionAnalyzer().engineKey(),
                    {TsumeEvaluation::Status::Mate, 31, {}, {}});

        using State = TsumeGameSession::State;
        TsumeGameSession session;
        session.configureEngine({}, &store);
        QSignalSpy positions(&session, &TsumeGameSession::positionChanged);
        QSignalSpy outcomes(&session, &TsumeGameSession::finished);
        QVERIFY(session.start(problem.sfen));
        QTRY_COMPARE(session.state(), State::Ready);
        QCOMPARE(session.remainingPlies(), 31);
        for (qsizetype ply = 0; ply < line.size(); ply += 2) {
            QVERIFY(session.play(line[ply]));
            QTRY_VERIFY_WITH_TIMEOUT(session.state() != State::Thinking, kAmpleMs);
            if (ply + 1 == line.size()) break;
            const QByteArray where = QByteArray::number(ply);
            QVERIFY2(session.state() == State::Ready, where.constData());
            QVERIFY2(positions.last()[1].toString() == line[ply + 1], where.constData());
            QVERIFY2(session.remainingPlies() == static_cast<int>(line.size() - ply - 2), where.constData());
        }
        QCOMPARE(session.state(), State::Solved);
        QCOMPARE(outcomes.size(), 1);
        QCOMPARE(qvariant_cast<TsumeGameSession::Outcome>(outcomes.first()[0]), TsumeGameSession::Outcome::Solved);
    }

    /// 残りの手数では詰まない手（詰みはあるが長い）は、応手を指して手数超過を知らせる。
    /// 31手詰の問題を29手詰として始め、31手かかる手を指して確かめる
    void sessionReportsLongerMateInLongProblem()
    {
        QTemporaryDir data;
        TsumeProgressStore store(data.path(), data.path());
        QVERIFY(store.open());
        const auto& problem = problem35();
        store.cache(m_long.positionIds[34], TsumePositionAnalyzer().engineKey(),
                    {TsumeEvaluation::Status::Mate, 29, {}, {}});

        using State = TsumeGameSession::State;
        TsumeGameSession session;
        session.configureEngine({}, &store);
        QSignalSpy positions(&session, &TsumeGameSession::positionChanged);
        QSignalSpy outcomes(&session, &TsumeGameSession::finished);
        QVERIFY(session.start(problem.sfen));
        QTRY_COMPARE(session.state(), State::Ready);
        QCOMPARE(session.remainingPlies(), 29);
        QVERIFY(session.play(problem.referenceMoves[0]));
        QTRY_VERIFY_WITH_TIMEOUT(session.state() != State::Thinking, kAmpleMs);
        QCOMPARE(session.state(), State::Failed);
        QCOMPARE(outcomes.size(), 1);
        QCOMPARE(qvariant_cast<TsumeGameSession::Outcome>(outcomes.first()[0]), TsumeGameSession::Outcome::TooLong);
        QVERIFY(!positions.last()[1].toString().isEmpty());
        session.undo();
        QCOMPARE(session.state(), State::Ready);
        QCOMPARE(session.sfen(), problem.sfen);
    }
};

QTEST_GUILESS_MAIN(TestTsumeDefenseSearch)
#include "tst_tsume_defense_search.moc"
