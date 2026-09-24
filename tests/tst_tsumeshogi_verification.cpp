#include <QtTest>
#include <QSignalSpy>
#include <QProcess>

#include "tsumeshogigenerator.h"
#include "tsumeshogiverifier.h"
#include "usi.h"
#include <position.h>
#include <tsume.h>

#include <atomic>
#include <set>

namespace {
const QString kUnique = QStringLiteral("7nk/7nn/9/9/9/9/9/9/9 b N 1");
const QString kSecond = QStringLiteral("9/9/9/R8/9/9/1k2+S4/9/3+N5 b RG2b3g3s3n4l18p 1");
const QString kThird = QStringLiteral("9/9/9/9/9/9/7+S1/5G2k/9 b RSr2b3g2s4n4l18p 1");
const QString kFourth = QStringLiteral("5k3/9/9/3+P1B1N1/9/9/9/9/9 b RSrb4g3s3n4l17p 1");
using Status = TsumeshogiVerifier::Status;
using Reply = TsumeshogiVerifier::Reply;

QString after(const QString& sfen, const QString& moves)
{
    shogi::Position position;
    if (!position.set_sfen(sfen.toStdString(), true)) qFatal("Invalid fixture");
    for (const auto& move : moves.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        if (!position.apply_usi_move(move.toStdString())) qFatal("Invalid fixture move");
    return QString::fromStdString(position.to_sfen());
}

// 深さ制限はUnknown。有限探索のLimitを不詰証明へ変換しない。
Reply solve(const QString& sfen, QStringList& pv)
{
    shogi::Position position;
    if (!position.set_sfen(sfen.toStdString(), true)) return Reply::Unknown;
    const auto attacker = position.side_to_move();
    std::atomic_bool stop{false};
    shogi::TsumeSearch search;
    auto result = search.solve(position, attacker, 5, 100, stop);
    if (result.status == shogi::TsumeStatus::NoMate) return Reply::NoMate;
    if (result.status != shogi::TsumeStatus::Mate) return Reply::Unknown;
    int remaining = result.plies;
    while (remaining > 0) {
        if (!result.move.is_valid()) return Reply::Unknown;
        pv.append(QString::fromStdString(position.move_to_usi(result.move)));
        position.do_move(result.move);
        --remaining;
        result = search.solve(position, attacker, remaining, 100, stop);
        if (result.status != shogi::TsumeStatus::Mate) return Reply::Unknown;
    }
    return Reply::Mate;
}

Status run(TsumeshogiVerifier& verifier, bool unknownOnly = false)
{
    for (int queries = 0; queries < 2000 && verifier.result().status == Status::Running; ++queries) {
        const QString sfen = verifier.nextPosition();
        if (sfen.isEmpty()) continue;
        QStringList pv;
        const auto reply = unknownOnly ? Reply::Unknown : solve(sfen, pv);
        verifier.submit(reply, pv);
    }
    return verifier.result().status;
}
}

class TestTsumeshogiVerification : public QObject
{
    Q_OBJECT
    static void prepare(TsumeshogiGenerator& generator, const QString& sfen, int target = 1)
    {
        generator.m_settings.targetMoves = target;
        generator.m_settings.maxPositionsToFind = 1;
        generator.m_settings.timeoutMs = 5000;
        generator.m_phase = TsumeshogiGenerator::Phase::Searching;
        generator.m_currentSfen = sfen;
        generator.m_usi = new Usi(nullptr, nullptr, nullptr, &generator);
        generator.m_cancelFlag = makeCancelFlag();
        generator.m_elapsedTimer.start();
    }

private slots:
    void rejectsKnownAlternatives_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<int>("plies");
        QTest::newRow("second-root") << kSecond << 5;
        QTest::newRow("fourth-root") << kFourth << 5;
        QTest::newRow("fourth-middle") << after(kFourth, QStringLiteral("S*3b 4a4b")) << 3;
        QTest::newRow("fourth-last") << after(kFourth, QStringLiteral("S*3b 4a4b R*4a 4b5b")) << 1;
    }
    void rejectsKnownAlternatives()
    {
        QFETCH(QString, sfen);
        QFETCH(int, plies);
        TsumeshogiVerifier verifier;
        verifier.start(sfen, plies);
        QCOMPARE(run(verifier), Status::Multiple);
    }
    void acceptsIndependentlyCheckedSingleMove()
    {
        // 攻駒は持桂1枚だけ。王手はN*2cのみ。玉方の全応手は0（python-shogiでも確認）。
        TsumeshogiVerifier verifier;
        verifier.start(kUnique, 1);
        QCOMPARE(run(verifier, true), Status::Unique);
        QCOMPARE(verifier.result().pv, QStringList{QStringLiteral("N*2c")});
        verifier.start(kUnique, 3);
        QCOMPARE(run(verifier, true), Status::WrongLength);
    }
    void longerAlternativeIsNotDiscarded()
    {
        // S*3b（5手）とS*4b（7手）だけの証明を供給。他の手はUnknownのまま。
        std::set<QString> selected;
        for (const auto& move : {QStringLiteral("S*3b"), QStringLiteral("S*4b")}) {
            shogi::Position position;
            QVERIFY(position.set_sfen(after(kFourth, move).toStdString(), true));
            for (const auto& defense : position.generate_legal_moves()) {
                auto child = position;
                child.do_move(defense);
                selected.insert(QString::fromStdString(child.to_sfen()));
            }
        }
        TsumeshogiVerifier verifier;
        verifier.start(kFourth, 5);
        bool sawLongPv = false;
        for (int i = 0; i < 2000 && verifier.result().status == Status::Running; ++i) {
            const auto query = verifier.nextPosition();
            if (query.isEmpty()) continue;
            QStringList pv;
            const auto reply = selected.count(query) ? solve(query, pv) : Reply::Unknown;
            sawLongPv |= reply == Reply::Mate && pv.size() == 5;
            verifier.submit(reply, pv);
        }
        QVERIFY(sawLongPv);
        QCOMPARE(verifier.result().status, Status::Multiple);
    }
    void promotionAndNonPromotionAreDistinct()
    {
        // 即詰みは3b3aと3b3a+の2手（python-shogiでも全合法手を列挙して確認）。
        TsumeshogiVerifier verifier;
        verifier.start(QStringLiteral("8k/6Rnn/9/9/9/9/9/9/9 b - 1"), 1);
        QCOMPARE(run(verifier, true), Status::Multiple);
    }
    void komoringIntegration()
    {
        const QString enginePath = qEnvironmentVariable("SHOGIBOARDQ_TEST_KOMORING");
        if (enginePath.isEmpty()) QSKIP("Set SHOGIBOARDQ_TEST_KOMORING for the optional real-engine test");
        QProcess engine;
        engine.start(enginePath, {});
        QVERIFY(engine.waitForStarted());
        const auto send = [&engine](const QString& line) {
            engine.write(line.toUtf8() + '\n');
            return engine.waitForBytesWritten();
        };
        const auto read = [&engine](const QString& prefix) {
            QElapsedTimer timer;
            timer.start();
            while (timer.elapsed() < 15000) {
                while (engine.canReadLine()) {
                    const QString line = QString::fromUtf8(engine.readLine()).trimmed();
                    if (line.startsWith(prefix)) return line;
                }
                if (engine.state() == QProcess::NotRunning) break;
                engine.waitForReadyRead(50);
            }
            return QString{};
        };
        QVERIFY(send(QStringLiteral("usi")));
        QCOMPARE(read(QStringLiteral("usiok")), QStringLiteral("usiok"));
        QVERIFY(send(QStringLiteral("setoption name Threads value 1")));
        QVERIFY(send(QStringLiteral("setoption name USI_Hash value 128")));
        QVERIFY(send(QStringLiteral("setoption name PostSearchLevel value MinLength")));
        QVERIFY(send(QStringLiteral("setoption name GenerateAllLegalMoves value true")));
        QVERIFY(send(QStringLiteral("setoption name MultiPV value 1")));
        QVERIFY(send(QStringLiteral("isready")));
        QCOMPARE(read(QStringLiteral("readyok")), QStringLiteral("readyok"));
        const QStringList cases{kSecond, kThird, kFourth, kUnique};
        const Status expected[]{Status::Multiple, Status::Unique, Status::Multiple, Status::Unique};
        for (qsizetype i = 0; i < cases.size(); ++i) {
            TsumeshogiVerifier verifier;
            verifier.start(cases[i], i == 3 ? 1 : 5);
            QElapsedTimer timer;
            timer.start();
            int queries = 0;
            while (verifier.result().status == Status::Running) {
                if (timer.elapsed() >= 30000) verifier.abort();
                const QString query = verifier.nextPosition();
                if (query.isEmpty()) continue;
                ++queries;
                QVERIFY(send(QStringLiteral("position sfen ") + query));
                QVERIFY(send(QStringLiteral("go mate 1000")));
                const QString reply = read(QStringLiteral("checkmate "));
                QVERIFY2(!reply.isEmpty(), "Missing checkmate response");
                const QString value = reply.mid(10);
                if (value == QStringLiteral("nomate")) verifier.submit(Reply::NoMate);
                else if (value == QStringLiteral("timeout") || value == QStringLiteral("notimplemented"))
                    verifier.submit(Reply::Unknown);
                else verifier.submit(Reply::Mate, value.split(QLatin1Char(' '), Qt::SkipEmptyParts));
            }
            qInfo() << "Komoring case" << i << "queries" << queries << "ms" << timer.elapsed();
            QCOMPARE(verifier.result().status, expected[i]);
            if (expected[i] == Status::Unique)
                QCOMPARE(verifier.result().pv.size(), i == 3 ? 1 : 5);
        }
        QVERIFY(send(QStringLiteral("quit")));
        QVERIFY(engine.waitForFinished());
    }
    void inconclusiveAndInvalidRepliesNeverPass()
    {
        TsumeshogiVerifier verifier;
        verifier.start(kSecond, 5);
        const auto query = verifier.nextPosition();
        QVERIFY(!query.isEmpty());
        verifier.submit(Reply::Mate, {QStringLiteral("7g7f")}); // 不正なPV
        QCOMPARE(run(verifier, true), Status::Unknown);
        verifier.start(kSecond, 5);
        verifier.abort();
        QCOMPARE(verifier.result().status, Status::Unknown);
        QVERIFY(verifier.nextPosition().isEmpty());
        verifier.start(QStringLiteral("invalid"), 5);
        QCOMPARE(verifier.result().status, Status::Invalid);
    }
    void noMateAndNonCheckingMoves()
    {
        TsumeshogiVerifier verifier;
        verifier.start(QStringLiteral("k8/9/9/9/9/9/9/9/9 b - 1"), 1);
        QCOMPARE(run(verifier), Status::NoMate);
    }
    void stopBeforeVerificationDoesNotPublish()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUnique);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Verifying);
        generator.stop();
        generator.continueVerification();
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        QCOMPARE(found.size(), 0);
        QVERIFY(!generator.isRunning());
    }
    void trimmingStopPublishesOnlyVerifiedBase()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUnique);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        generator.continueVerification();
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Trimming);
        QVERIFY(generator.m_trimTestSfen != kUnique);
        generator.onCheckmateSolved({QStringLiteral("N*2c")}); // 除去後の1手PVだけでは未検証
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Verifying);
        generator.stop();
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0][0].toString(), kUnique);
    }
    void rejectedTrimKeepsVerifiedBase()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUnique);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        generator.continueVerification();
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        for (int i = 0; i < 1000 && generator.m_phase == TsumeshogiGenerator::Phase::Verifying; ++i) {
            generator.continueVerification();
            if (generator.m_verificationAwaiting) generator.onCheckmateUnknown();
        }
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Trimming);
        QCOMPARE(generator.m_trimBaseSfen, kUnique);
        QCOMPARE(generator.m_verifiedSfen, kUnique);
        QCOMPARE(found.size(), 0);
        generator.stop();
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0][0].toString(), kUnique);
    }
    void timeoutAndLateResponseDoNotCertify()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kSecond, 5);
        generator.startVerification(kSecond, TsumeshogiGenerator::Phase::Searching);
        generator.continueVerification();
        QVERIFY(generator.m_verificationAwaiting);
        generator.m_awaitingStopResponse = true;
        generator.onCheckmateSolved({QStringLiteral("N*2c")}); // stop後の遅延応答を捨てる
        QVERIFY(!generator.m_verificationAwaiting);
        QVERIFY(generator.m_verifiedSfen.isEmpty());
        generator.m_settings.timeoutMs = 0; // 検査の総予算終了
        generator.continueVerification();
        QCOMPARE(generator.m_verificationInconclusive, 1);
        QCOMPARE(found.size(), 0);
        generator.stop();
    }
    void certificateMatchesSfenAndPvAndDeduplicates()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUnique);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        generator.continueVerification();
        QVERIFY(!generator.registerFoundPosition(kSecond, {QStringLiteral("N*2c")}));
        QVERIFY(!generator.registerFoundPosition(kUnique, {QStringLiteral("N*3c")}));
        QVERIFY(generator.registerFoundPosition(kUnique, {QStringLiteral("N*2c")}));
        QVERIFY(!generator.registerFoundPosition(kUnique, {QStringLiteral("N*2c")}));
        generator.stop();
        QCOMPARE(found.size(), 1);
    }
    void allExitPathsRequireCertificate_data()
    {
        QTest::addColumn<int>("exitPath");
        QTest::newRow("normal") << 0;
        QTest::newRow("stop") << 1;
        QTest::newRow("engine-error") << 2;
        QTest::newRow("unresponsive") << 3;
    }
    void allExitPathsRequireCertificate()
    {
        QFETCH(int, exitPath);
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kSecond, 5);
        generator.m_phase = TsumeshogiGenerator::Phase::Trimming;
        generator.m_trimBaseSfen = kSecond;
        generator.m_trimBasePv = {QStringLiteral("G*9g"), QStringLiteral("8g8h"),
                                 QStringLiteral("R*8g"), QStringLiteral("8h9i"), QStringLiteral("9g9h")};
        // 別局面の合格結果や同局面の別PVでは出力できない。
        generator.m_verifiedSfen = kUnique;
        generator.m_verifiedPv = {QStringLiteral("N*2c")};
        if (exitPath == 0) generator.finishTrimmingPhase();
        if (exitPath == 1) generator.stop();
        if (exitPath == 2) generator.onEngineError(QStringLiteral("test error"));
        if (exitPath == 3) {
            generator.m_awaitingStopResponse = true;
            generator.onSafetyTimeout();
        }
        QCOMPARE(found.size(), 0);
        generator.stop();
    }
    void normalOutputAndDuplicateCheck()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUnique);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        generator.continueVerification();
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Trimming);
        // 各除去を不詰として拒否し、検証済みの元局面に収束させる。
        for (int i = 0; i < 10 && generator.isRunning(); ++i) generator.onCheckmateNoMate();
        QVERIFY(!generator.isRunning());
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0][0].toString(), kUnique);
        QVERIFY(!generator.registerFoundPosition(kUnique, {QStringLiteral("N*2c")}));
    }
};

QTEST_GUILESS_MAIN(TestTsumeshogiVerification)
#include "tst_tsumeshogi_verification.moc"
