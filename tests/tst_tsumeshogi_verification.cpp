#include <QtTest>
#include <QSignalSpy>
#include <QProcess>

#include "tsumeshogigenerator.h"
#include "tsumeshogiverifier.h"
#include "tsumeshogicandidatescreener.h"
#include "usi.h"
#include <position.h>
#include <tsume.h>

#include <atomic>
#include <iterator>
#include <set>

namespace {
const QString kUnique = QStringLiteral("7nk/7nn/9/9/9/9/9/9/9 b N 1");
// kUnique に1三歩を足した局面。N*2c の即詰は変わらず、1二歩成などの王手が加わるので検査に問い合わせが要る。
// 1二桂と1三歩の除去は内蔵探索の事前選別を通ってエンジンに送られる（他の除去は選別で却下される）
const QString kUniqueDecorated = QStringLiteral("7nk/7nn/8P/9/9/9/9/9/9 b N 1");
const QString kSecond = QStringLiteral("9/9/9/R8/9/9/1k2+S4/9/3+N5 b RG2b3g3s3n4l18p 1");
const QString kThird = QStringLiteral("9/9/9/9/9/9/7+S1/5G2k/9 b RSr2b3g2s4n4l18p 1");
const QString kFourth = QStringLiteral("5k3/9/9/3+P1B1N1/9/9/9/9/9 b RSrb4g3s3n4l17p 1");
// 初手 3c3b+ だけが詰む3手詰。2a1a の後は 3b2b と 2c2b の2通りで詰む（KomoringHeights 1.1.0 で照合済み）。
const QString kFinalTwo = QStringLiteral("7k1/9/6L+S1/9/9/9/9/9/9 b 2r2b4g3s4n3l18p 1");
// 初手 7e7b だけが詰む5手詰。8b への合駒が最長抵抗（あと3手）で、9b9c は1手で詰む変化だが詰手が2通りある
// （変化別詰）。KomoringHeights 1.1.0 で最終手許容なら合格と照合済み。
const QString kVariation = QStringLiteral("3+P5/k8/9/9/1G+R6/9/9/9/9 b Sr2b3g3s4n4l17p 1");
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

// 採択方針の検査専用の代用エンジン。深さ5で詰みが見つからない局面を不詰として答える。
// 実エンジンでの照合は komoringIntegration() で行う。
Reply scripted(const QString& sfen, QStringList& pv)
{
    const auto reply = solve(sfen, pv);
    return reply == Reply::Unknown ? Reply::NoMate : reply;
}

Status runScripted(TsumeshogiVerifier& verifier)
{
    for (int queries = 0; queries < 2000 && verifier.result().status == Status::Running; ++queries) {
        const QString sfen = verifier.nextPosition();
        if (sfen.isEmpty()) continue;
        QStringList pv;
        const auto reply = scripted(sfen, pv);
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

    /// 検査中の問い合わせに一律の応答を返して検査を終える
    static void answerVerification(TsumeshogiGenerator& generator, Reply reply)
    {
        for (int i = 0; i < 1000 && generator.m_phase == TsumeshogiGenerator::Phase::Verifying; ++i) {
            generator.continueVerification();
            if (!generator.m_verificationAwaiting) continue;
            if (reply == Reply::NoMate) generator.onCheckmateNoMate();
            else generator.onCheckmateUnknown();
        }
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
        // 根の局面は最終手でも複数解を許容しない（1手詰の初手）
        verifier.start(QStringLiteral("8k/6Rnn/9/9/9/9/9/9/9 b - 1"), 1, {true});
        QCOMPARE(run(verifier, true), Status::Multiple);
    }
    void finalMoveAlternativesFollowOption()
    {
        TsumeshogiVerifier verifier;
        verifier.start(kFinalTwo, 3, {true});
        QCOMPARE(runScripted(verifier), Status::Unique);
        QCOMPARE(verifier.result().pv.size(), 3);
        QCOMPARE(verifier.result().pv[0], QStringLiteral("3c3b+"));
        QCOMPARE(verifier.result().pv[1], QStringLiteral("2a1a"));
        verifier.start(kFinalTwo, 3, {false});
        QCOMPARE(runScripted(verifier), Status::Multiple);
        // 既定値は許容
        verifier.start(kFinalTwo, 3);
        QCOMPARE(runScripted(verifier), Status::Unique);
    }
    void acceptsAlternativesInShorterVariations()
    {
        // 変化 9b9c には即詰が2手あるが、主手順ではないので余詰と数えない
        shogi::Position variation;
        QVERIFY(variation.set_sfen(after(kVariation, QStringLiteral("7e7b 9b9c")).toStdString(), true));
        int mates = 0;
        for (const auto& move : variation.generate_checking_moves()) {
            auto next = variation;
            next.do_move(move);
            if (next.generate_legal_moves().empty()) ++mates;
        }
        QVERIFY(mates >= 2);
        TsumeshogiVerifier verifier;
        verifier.start(kVariation, 5, {true});
        QCOMPARE(runScripted(verifier), Status::Unique);
        QCOMPARE(verifier.result().pv.size(), 5);
        QCOMPARE(verifier.result().pv[0], QStringLiteral("7e7b"));
    }
    void batchCountsGeneratedPositionsAndKeepsOnlyCandidates()
    {
        TsumeshogiGenerator generator;
        QSignalSpy progress(&generator, &TsumeshogiGenerator::progressUpdated);
        prepare(generator, kUnique, 3);
        generator.m_settings.posGenSettings.maxAttackPieces = 4;
        generator.startBatchGeneration();
        QVERIFY(generator.m_batchWatcher.isRunning() || generator.m_batchWatcher.isFinished());
        generator.m_batchWatcher.waitForFinished();
        generator.onBatchReady();
        // 進捗には候補以外も含む生成局面数を通知し、キューには事前選別を通った局面だけを積む
        QVERIFY(generator.m_generatedCount >= 1);
        QCOMPARE(progress.size(), 1);
        QCOMPARE(progress[0][0].toInt(), generator.m_generatedCount);
        QVERIFY(generator.m_positionQueue.size() <= 8);
        const std::atomic_bool noStop{false};
        TsumeshogiCandidateScreener::Limits limits;
        limits.timeLimitMs = limits.alternativeTimeLimitMs = 3000;
        limits.alternativeExtraPlies = 2;
        for (const QString& sfen : std::as_const(generator.m_positionQueue)) {
            const auto verdict = TsumeshogiCandidateScreener::screen(sfen, 3, limits, noStop);
            QVERIFY2(verdict == TsumeshogiCandidateScreener::Verdict::Candidate
                         || verdict == TsumeshogiCandidateScreener::Verdict::Unknown, qPrintable(sfen));
        }
        generator.stop();
    }
    void trimmingSkipsRemovalsRejectedByScreening()
    {
        // kFinalTwo は初手 3c3b+ の3手詰。香か成銀を除くと3手で詰まないので、除去候補はエンジンに送られない
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kFinalTwo, 3);
        generator.m_settings.allowFinalMoveAlternatives = true;
        generator.m_settings.maxPositionsToFind = 0; // 出力後も探索を続け、Usi を保持したまま確認する
        generator.onCheckmateSolved({QStringLiteral("3c3b+"), QStringLiteral("2a1a"), QStringLiteral("3b2b")});
        int queries = 0;
        for (int i = 0; i < 1000 && generator.m_phase == TsumeshogiGenerator::Phase::Verifying; ++i) {
            generator.continueVerification();
            if (!generator.m_verificationAwaiting) continue;
            ++queries;
            QStringList pv;
            const QString query = generator.m_usi->positions.last().mid(QStringLiteral("position sfen ").size());
            if (scripted(query, pv) == Reply::Mate) generator.onCheckmateSolved(pv);
            else generator.onCheckmateNoMate();
        }
        // 香・成銀のどちらを除いても3手で詰まないので、除去局面は1つもエンジンに送られずに出力へ進む
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Searching);
        QCOMPARE(static_cast<int>(generator.m_usi->positions.size()), queries);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found[0][0].toString(), kFinalTwo);
        generator.stop();
    }
    void generatorPassesFinalMoveOption_data()
    {
        QTest::addColumn<bool>("allow");
        QTest::newRow("allow") << true;
        QTest::newRow("strict") << false;
    }
    void generatorPassesFinalMoveOption()
    {
        QFETCH(bool, allow);
        TsumeshogiGenerator generator;
        QSignalSpy stats(&generator, &TsumeshogiGenerator::verificationStatsUpdated);
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kFinalTwo, 3);
        generator.m_settings.allowFinalMoveAlternatives = allow;
        generator.onCheckmateSolved({QStringLiteral("3c3b+"), QStringLiteral("2a1a"), QStringLiteral("3b2b")});
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Verifying);
        for (int i = 0; i < 1000 && generator.m_phase == TsumeshogiGenerator::Phase::Verifying; ++i) {
            generator.continueVerification();
            if (!generator.m_verificationAwaiting) continue;
            QStringList pv;
            const QString query = generator.m_usi->positions.last().mid(QStringLiteral("position sfen ").size());
            if (scripted(query, pv) == Reply::Mate) generator.onCheckmateSolved(pv);
            else generator.onCheckmateNoMate();
        }
        if (allow) {
            // 香・成銀の除去は内蔵探索で却下されるので、検査合格後にそのまま出力されて上限1で終了する
            QVERIFY(!generator.isRunning());
            QCOMPARE(found.size(), 1);
            QCOMPARE(found[0][0].toString(), kFinalTwo);
            QCOMPARE(stats.size(), 0);
        } else {
            QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Searching);
            QVERIFY(generator.m_verifiedSfen.isEmpty());
            QCOMPARE(stats.size(), 1);
            QCOMPARE(stats[0][0].toInt(), 1);
            QCOMPARE(stats[0][1].toInt(), 0);
        }
        generator.stop();
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
        struct Case { QString sfen; int plies; bool allowFinal; Status expected; };
        const Case cases[]{
            {kSecond, 5, true, Status::Multiple},
            {kThird, 5, true, Status::Unique},
            {kFourth, 5, true, Status::Multiple},
            {kUnique, 1, true, Status::Unique},
            {kFinalTwo, 3, true, Status::Unique},
            {kFinalTwo, 3, false, Status::Multiple},
            {kVariation, 5, true, Status::Unique},
        };
        for (std::size_t i = 0; i < std::size(cases); ++i) {
            TsumeshogiVerifier verifier;
            verifier.start(cases[i].sfen, cases[i].plies, {cases[i].allowFinal});
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
            QCOMPARE(verifier.result().status, cases[i].expected);
            if (cases[i].expected == Status::Unique)
                QCOMPARE(verifier.result().pv.size(), cases[i].plies);
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
    void trimmingStopDoesNotPublishIncompleteBase()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUniqueDecorated);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        answerVerification(generator, Reply::NoMate); // 1二歩成などの別の王手を不詰として検査を通す
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Trimming);
        // 2一桂・2二桂の除去は内蔵探索で却下され、最初にエンジンへ送られるのは1二桂を除いた局面
        const int verificationQueries = static_cast<int>(generator.m_usi->positions.size()) - 1;
        QVERIFY(verificationQueries >= 1);
        QVERIFY(generator.m_trimTestSfen != kUniqueDecorated);
        generator.onCheckmateSolved({QStringLiteral("N*2c")}); // 除去後の1手PVだけでは未検証
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Verifying);
        generator.stop();
        QCOMPARE(found.size(), 0);
    }
    void inconclusiveTrimRejectsEntireCandidate()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUniqueDecorated);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        answerVerification(generator, Reply::NoMate);
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Trimming);
        generator.onCheckmateSolved({QStringLiteral("N*2c")}); // 1二桂を除いた局面の候補PV
        answerVerification(generator, Reply::Unknown);            // 除去後の検査は判定不能
        // 「除去不能」と解釈せず、局面全体を採択から外して次の候補へ進む。
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Searching);
        QCOMPARE(generator.m_verificationInconclusive, 1);
        QVERIFY(!generator.m_trimComplete);
        QCOMPARE(generator.m_trimBaseSfen, kUniqueDecorated);
        QCOMPARE(generator.m_verifiedSfen, kUniqueDecorated);
        QCOMPARE(found.size(), 0);
        generator.stop();
        QCOMPARE(found.size(), 0);
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
    void untrimmedCertificateNeverPublishes()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUniqueDecorated);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        answerVerification(generator, Reply::NoMate);
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Trimming);
        QVERIFY(!generator.registerFoundPosition(kSecond, {QStringLiteral("N*2c")}));
        QVERIFY(!generator.registerFoundPosition(kUniqueDecorated, {QStringLiteral("N*3c")}));
        // 詰みの証明が同じでも、除去候補を全て確認する前は出力できない。
        QVERIFY(!generator.registerFoundPosition(kUniqueDecorated, {QStringLiteral("N*2c")}));
        generator.stop();
        QCOMPARE(found.size(), 0);
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
    void provenMateCannotBeRejectedByContradictoryNoMate()
    {
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUniqueDecorated);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        answerVerification(generator, Reply::NoMate);
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Trimming);
        QVERIFY(generator.m_trimProvenInTarget);
        generator.onCheckmateNoMate();
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Searching);
        QCOMPARE(generator.m_verificationInconclusive, 1);
        QCOMPARE(found.size(), 0);
        QVERIFY(!generator.registerFoundPosition(kUniqueDecorated, {QStringLiteral("N*2c")}));
        generator.stop();
    }
    void unresolvedTrimmingNeverPublishes_data()
    {
        QTest::addColumn<int>("failure");
        QTest::newRow("timeout") << 0;
        QTest::newRow("different-pv-length") << 1;
        QTest::newRow("late-response") << 2;
        QTest::newRow("engine-error") << 3;
        QTest::newRow("stop") << 4;
    }
    void unresolvedTrimmingNeverPublishes()
    {
        QFETCH(int, failure);
        TsumeshogiGenerator generator;
        QSignalSpy found(&generator, &TsumeshogiGenerator::positionFound);
        prepare(generator, kUniqueDecorated);
        generator.onCheckmateSolved({QStringLiteral("N*2c")});
        answerVerification(generator, Reply::NoMate);
        QCOMPARE(generator.m_phase, TsumeshogiGenerator::Phase::Trimming);
        QCOMPARE(generator.m_verifiedSfen, kUniqueDecorated);
        if (failure == 0) generator.onCheckmateUnknown();
        if (failure == 1) generator.onCheckmateSolved({QStringLiteral("N*2c"), QStringLiteral("1a1b"), QStringLiteral("2c1a+")});
        if (failure == 2) {
            generator.m_awaitingStopResponse = true;
            generator.onCheckmateNoMate();
        }
        if (failure == 3) generator.onEngineError(QStringLiteral("test error"));
        if (failure == 4) generator.stop();
        QCOMPARE(found.size(), 0);
        QVERIFY(!generator.m_trimComplete);
        generator.stop();
        QCOMPARE(found.size(), 0);
    }
    void problem703RemovalPreservesMateLine()
    {
        const QString original = QStringLiteral("9/9/5N3/9/9/9/9/8+R/5k1L1 b Gr2b3g4s3n3l18p 1");
        const QString noKnight = QStringLiteral("9/9/9/9/9/9/9/8+R/5k1L1 b Gr2b3g4s4n3l18p 1");
        const QString noLance = QStringLiteral("9/9/5N3/9/9/9/9/8+R/5k3 b Gr2b3g4s3n4l18p 1");
        const QString neither = QStringLiteral("9/9/9/9/9/9/9/8+R/5k3 b Gr2b3g4s4n4l18p 1");
        const auto removals = TsumeshogiGenerator::onePieceRemovedPositions(original);
        QVERIFY(removals.contains(noKnight));
        QVERIFY(removals.contains(noLance));
        QVERIFY(TsumeshogiGenerator::onePieceRemovedPositions(noKnight).contains(neither));
        for (const auto& sfen : {original, noKnight, noLance, neither}) {
            shogi::Position position;
            QVERIFY(position.set_sfen(sfen.toStdString(), true));
            const auto moves = QStringLiteral("G*4h 4i5i 4h5h 5i6i 5h6h 6i7i 6h7h 7i8i 7h8h 8i9i 8h9h 9i8i 1h8h")
                                   .split(QLatin1Char(' '));
            for (const auto& move : moves) {
                const bool attacking = position.side_to_move() == shogi::Color::Black;
                QVERIFY(position.apply_usi_move(move.toStdString()));
                if (attacking) QVERIFY(position.is_in_check(shogi::Color::White));
            }
            QVERIFY(position.generate_legal_moves().empty());
        }
    }
};

QTEST_GUILESS_MAIN(TestTsumeshogiVerification)
#include "tst_tsumeshogi_verification.moc"
