/// @file tst_tsumeshogi_retrograde.cpp
/// @brief 詰み局面からの逆算・盤面編集・余詰リペア・厳密証明・チェックポイント再開・CLI grow-tsume のテスト

#include <QtTest>

#include "clicommandspecs.h"
#include "tsumecandidatejob.h"
#include "tsumecollection.h"
#include "tsumepositionedits.h"
#include "tsumeretrogradegenerator.h"
#include "tsumeretrogradeops.h"
#include "tsumeshogisettings.h"
#include "tsumeshogiverifier.h"
#include "tsumestrictcertifier.h"

#include <position.h>
#include <tsume.h>

#include <QCoreApplication>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QSettings>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <atomic>
#include <random>

namespace {
const QString kUnique1 = QStringLiteral("7nk/7nn/9/9/9/9/9/9/9 b N 1");
const QString kUnique1Decorated = QStringLiteral("7nk/7nn/8P/9/9/9/9/9/9 b N 1");
const QString kFinalTwo3 = QStringLiteral("7k1/9/6L+S1/9/9/9/9/9/9 b 2r2b4g3s4n3l18p 1");
const QString kSurplus1 = QStringLiteral("7nk/7nn/9/9/9/9/9/9/9 b NL 1");
const QString kSurplus3 = QStringLiteral("2kg5/9/1+R2+R4/9/9/9/9/9/9 b 2b3g4s4n4l18p 1");
const QString kCook5 = QStringLiteral("5k3/9/9/3+P1B1N1/9/9/9/9/9 b RSrb4g3s3n4l17p 1");
const QString kVariation5 = QStringLiteral("3+P5/k8/9/9/1G+R6/9/9/9/9 b Sr2b3g3s4n4l17p 1");

MateAnswer solveWithHayanagi(const QString& sfen, int maxPlies = 7)
{
    shogi::Position pos;
    if (!pos.set_sfen(sfen.toStdString(), true)) return {MateAnswer::Kind::Unknown, {}};
    const auto attacker = pos.side_to_move();
    std::atomic_bool stop{false};
    shogi::TsumeSearch search;
    auto res = search.solve(pos, attacker, maxPlies, 0, stop);
    if (res.status == shogi::TsumeStatus::NoMate || res.status == shogi::TsumeStatus::Limit) {
        return {MateAnswer::Kind::NoMate, {}};
    }
    if (res.status != shogi::TsumeStatus::Mate) return {MateAnswer::Kind::Unknown, {}};
    QStringList pv;
    int remaining = res.plies;
    while (remaining > 0) {
        if (!res.move.is_valid()) return {MateAnswer::Kind::Unknown, {}};
        pv.append(QString::fromStdString(pos.move_to_usi(res.move)));
        pos.do_move(res.move);
        --remaining;
        if (remaining > 0) {
            res = search.solve(pos, attacker, remaining, 0, stop);
            if (res.status != shogi::TsumeStatus::Mate) return {MateAnswer::Kind::Unknown, {}};
        }
    }
    return {MateAnswer::Kind::Mate, pv};
}

void driveCertifier(TsumeStrictCertifier& cert, int maxSteps = 5000, int maxPlies = 7)
{
    for (int i = 0; i < maxSteps && !cert.isDone(); ++i) {
        const auto query = cert.nextQuery();
        if (!query) continue;
        cert.submit(solveWithHayanagi(query->sfen, maxPlies), 1);
    }
}

QString mockRetroEnginePath()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/mock_retro_engine");
}

QString cliExecutablePath()
{
#ifdef SHOGIBOARDQ_CLI_PATH
    return QStringLiteral(SHOGIBOARDQ_CLI_PATH);
#else
    return QCoreApplication::applicationDirPath() + QStringLiteral("/../shogiboardq-cli");
#endif
}
} // namespace

class TestTsumeshogiRetrograde : public QObject
{
    Q_OBJECT

private slots:
    void mateEndingsAreValidAndReversible()
    {
        TsumeRetrogradeOps::Limits limits;
        limits.maxAttackPieces = 12;
        limits.maxDefendPieces = 8;
        std::mt19937_64 rng(12345);

        const QStringList endings = TsumeRetrogradeOps::generateMateEndings(limits, 12, rng);
        QVERIFY2(endings.size() >= 8, qPrintable(QStringLiteral("Only generated %1 mate endings").arg(endings.size())));

        for (const QString& ending : endings) {
            QVERIFY2(TsumeRetrogradeOps::isMateEnding(ending), qPrintable(ending));
            shogi::Position pos;
            QVERIFY(pos.set_sfen(ending.toStdString(), true));
            // 攻方持駒は常にゼロ（駒余りなし収束）
            for (int kind = 1; kind <= 7; ++kind) {
                QCOMPARE(pos.hand_count(shogi::Color::Black, static_cast<shogi::PieceType>(kind)), 0);
            }
            const auto onePly = TsumeRetrogradeOps::reverseFromMateEnding(ending, limits, 6, rng);
            QVERIFY2(!onePly.isEmpty(), qPrintable(ending));
            for (const auto& cand : onePly) {
                QVERIFY(!cand.intendedFirstMove.isEmpty());
                QVERIFY(TsumeCollection::validMateLine(cand.sfen, {cand.intendedFirstMove}));
            }
        }
    }

    void predecessorsRoundtripStrictly()
    {
        TsumeRetrogradeOps::Limits limits;
        limits.maxAttackPieces = 20;
        limits.maxDefendPieces = 14;
        std::mt19937_64 rng(20261127);

        // 1. 詰み上がり局面→1手詰→3手詰候補の2段階逆算
        const QStringList endings = TsumeRetrogradeOps::generateMateEndings(limits, 6, rng);
        QVERIFY(!endings.isEmpty());
        int totalPreds = 0;
        for (const QString& ending : endings) {
            const auto onePlies = TsumeRetrogradeOps::reverseFromMateEnding(ending, limits, 3, rng);
            for (const auto& one : onePlies) {
                const auto preds = TsumeRetrogradeOps::generatePredecessors(one.sfen, limits, 12, rng);
                for (const auto& cand : preds) {
                    shogi::Position pos;
                    QVERIFY2(pos.set_sfen(cand.sfen.toStdString(), true), qPrintable(cand.sfen));
                    QCOMPARE(pos.side_to_move(), shogi::Color::Black);
                    QVERIFY(!pos.is_in_check(shogi::Color::White));
                    QVERIFY(!cand.intendedFirstMove.isEmpty());
                    QVERIFY(pos.apply_usi_move(cand.intendedFirstMove.toStdString()));
                    QVERIFY(pos.is_in_check(shogi::Color::White));
                    ++totalPreds;
                }
            }
        }
        QVERIFY2(totalPreds >= 10, qPrintable(QStringLiteral("Only got %1 predecessors").arg(totalPreds)));

        // 2. 到達不能な1手詰（kUnique1: 1〜2段目の桂で玉が完全封鎖）は安全に0件を返すこと
        const QString norm1 = TsumeRetrogradeOps::normalizeProblemSfen(kUnique1, true);
        QVERIFY(!norm1.isEmpty());
        QCOMPARE(TsumeRetrogradeOps::generatePredecessors(norm1, limits, 10, rng).size(), 0);

        // 3. 3手詰・5手詰の既知局面からの逆算（玉移動・捨駒・合駒など）
        const auto preds3 = TsumeRetrogradeOps::generatePredecessors(kFinalTwo3, limits, 30, rng);
        QVERIFY2(preds3.size() >= 5, qPrintable(QStringLiteral("Got only %1 predecessors for kFinalTwo3").arg(preds3.size())));
        const auto preds5 = TsumeRetrogradeOps::generatePredecessors(kVariation5, limits, 30, rng);
        QVERIFY2(preds5.size() >= 5, qPrintable(QStringLiteral("Got only %1 predecessors for kVariation5").arg(preds5.size())));
    }

    void cookRepairsGenerateValidCandidates()
    {
        TsumeRetrogradeOps::Limits limits;
        limits.maxAttackPieces = 20;
        limits.maxDefendPieces = 14;
        std::mt19937_64 rng(777);

        // kCook5 は S*3b と S*4b の両方で詰む余詰局面
        const auto repairs = TsumePositionEdits::generateCookRepairs(
            kCook5, QStringLiteral("S*3b"), QStringLiteral("S*4b"), limits, 20, rng);
        QVERIFY2(!repairs.isEmpty(), "Expected cook repair candidates");

        bool foundFixedOrBlocked = false;
        for (const auto& cand : repairs) {
            shogi::Position pos;
            QVERIFY(pos.set_sfen(cand.sfen.toStdString(), true));
            QVERIFY(!pos.is_in_check(shogi::Color::White));
            // 本命の初手 S*3b が依然として合法王手であること
            shogi::Position afterMain = pos;
            QVERIFY(afterMain.apply_usi_move("S*3b"));
            QVERIFY(afterMain.is_in_check(shogi::Color::White));
            // 余詰初手 S*4b が打てなくなっているか、4bに守り駒が利いている候補が存在すること
            shogi::Position afterCook = pos;
            if (!afterCook.apply_usi_move("S*4b")) {
                foundFixedOrBlocked = true;
            } else {
                const auto sq4b = shogi::square_from_usi("4b");
                if (sq4b && pos.is_square_attacked(*sq4b, shogi::Color::White)) {
                    foundFixedOrBlocked = true;
                }
            }
        }
        QVERIFY(foundFixedOrBlocked);
    }

    void sliceToTargetLengthWorks()
    {
        const MateAnswer ans5 = solveWithHayanagi(kVariation5, 5);
        QCOMPARE(ans5.kind, MateAnswer::Kind::Mate);
        const QStringList& pv5 = ans5.pv;
        QCOMPARE(pv5.size(), 5);
        QVERIFY(TsumeCollection::validMateLine(kVariation5, pv5));

        const auto sliced3 = TsumeRetrogradeOps::sliceToTargetLength(kVariation5, pv5, 3);
        QVERIFY(sliced3.has_value());
        QCOMPARE(sliced3->pv.size(), 3);
        QCOMPARE(sliced3->pv, pv5.mid(2));
        QVERIFY(TsumeCollection::validMateLine(sliced3->sfen, sliced3->pv));

        const auto sliced1 = TsumeRetrogradeOps::sliceToTargetLength(kVariation5, pv5, 1);
        QVERIFY(sliced1.has_value());
        QCOMPARE(sliced1->pv.size(), 1);
        QVERIFY(TsumeCollection::validMateLine(sliced1->sfen, sliced1->pv));

        QVERIFY(!TsumeRetrogradeOps::sliceToTargetLength(kVariation5, pv5, 4).has_value());
        QVERIFY(!TsumeRetrogradeOps::sliceToTargetLength(kVariation5, pv5, 7).has_value());
    }

    void strictCertifierEnforcesNoFinalAltNoSurplusAndMinimalPieces()
    {
        // 1. 最終手余詰あり局面：厳密（allowFinal=false）は Multiple で棄却、許容（true）は Certified
        {
            TsumeStrictCertifier::Settings s;
            s.targetMoves = 3;
            s.allowFinalMoveAlternatives = false;
            s.requireNoSurplus = true;
            TsumeStrictCertifier cert(kFinalTwo3, s);
            driveCertifier(cert);
            QCOMPARE(cert.stage(), TsumeStrictCertifier::Stage::Rejected);
            QCOMPARE(cert.rejectionReason(), TsumeStrictCertifier::RejectionReason::Multiple);
        }
        {
            TsumeStrictCertifier::Settings s;
            s.targetMoves = 3;
            s.allowFinalMoveAlternatives = true;
            s.requireNoSurplus = true;
            TsumeStrictCertifier cert(kFinalTwo3, s);
            driveCertifier(cert);
            QCOMPARE(cert.stage(), TsumeStrictCertifier::Stage::Certified);
            QVERIFY(cert.outcome().has_value());
            QCOMPARE(cert.outcome()->pv.size(), 3);
        }

        // 2. 駒余りあり局面（1手・3手）：requireNoSurplus=true で Surplus 棄却
        {
            TsumeStrictCertifier::Settings s;
            s.targetMoves = 1;
            s.allowFinalMoveAlternatives = false;
            s.requireNoSurplus = true;
            TsumeStrictCertifier cert(kSurplus1, s);
            driveCertifier(cert);
            QCOMPARE(cert.stage(), TsumeStrictCertifier::Stage::Rejected);
            QCOMPARE(cert.rejectionReason(), TsumeStrictCertifier::RejectionReason::Surplus);
        }
        {
            TsumeStrictCertifier::Settings s;
            s.targetMoves = 3;
            s.allowFinalMoveAlternatives = true;
            s.requireNoSurplus = true;
            TsumeStrictCertifier cert(kSurplus3, s);
            driveCertifier(cert);
            QCOMPARE(cert.stage(), TsumeStrictCertifier::Stage::Rejected);
            QCOMPARE(cert.rejectionReason(), TsumeStrictCertifier::RejectionReason::Surplus);
        }

        // 3. 不要駒付き局面（kUnique1Decorated）：不要駒1三歩が自動除去されて Certified
        {
            const QString normDecorated = TsumeRetrogradeOps::normalizeProblemSfen(kUnique1Decorated, true);
            TsumeStrictCertifier::Settings s;
            s.targetMoves = 1;
            s.allowFinalMoveAlternatives = false;
            s.requireNoSurplus = true;
            TsumeStrictCertifier cert(normDecorated, s);
            driveCertifier(cert);
            QCOMPARE(cert.stage(), TsumeStrictCertifier::Stage::Certified);
            QVERIFY(cert.outcome().has_value());
            QVERIFY(cert.outcome()->removedPieces >= 1);
            QCOMPARE(cert.outcome()->pv, QStringList{QStringLiteral("N*2c")});
            // 最終局面から1枚でも除くと1手詰にならないこと
            for (const QString& rem : TsumeCandidateJob::onePieceRemovedPositions(cert.outcome()->sfen)) {
                const auto ans = solveWithHayanagi(rem, 3);
                QVERIFY(ans.kind != MateAnswer::Kind::Mate || ans.pv.size() != 1);
            }
        }
    }

    void strictCertifierCheckpointResumeReachesSameVerdict()
    {
        const QString normDecorated = TsumeRetrogradeOps::normalizeProblemSfen(kUnique1Decorated, true);
        TsumeStrictCertifier::Settings s;
        s.targetMoves = 1;
        s.allowFinalMoveAlternatives = false;
        s.requireNoSurplus = true;

        TsumeStrictCertifier first(normDecorated, s);
        // 2クエリだけ進めて途中保存
        for (int i = 0; i < 2 && !first.isDone(); ++i) {
            const auto q = first.nextQuery();
            QVERIFY(q.has_value());
            first.submit(solveWithHayanagi(q->sfen, 5), 2);
        }
        QVERIFY(!first.isDone());
        const QJsonObject saved = first.toJson();

        // 復元して最後まで完走
        auto restored = TsumeStrictCertifier::fromJson(saved, s);
        QVERIFY(restored != nullptr);
        driveCertifier(*restored);
        QCOMPARE(restored->stage(), TsumeStrictCertifier::Stage::Certified);
        QVERIFY(restored->outcome().has_value());
        QCOMPARE(restored->outcome()->pv, QStringList{QStringLiteral("N*2c")});
    }

    void supports101PliesAndVerifierCacheSeeding()
    {
        // 101手（最大301手）が各コンポーネントで受理されること
        QVERIFY(TsumeshogiSettings::GeneratorLimits::isTargetMoves(101));
        QVERIFY(TsumeshogiSettings::GeneratorLimits::isTargetMoves(301));
        QVERIFY(!TsumeshogiSettings::GeneratorLimits::isTargetMoves(100));

        const OperationSpec* growSpec = CliCommandSpecs::find(QStringLiteral("grow-tsume"));
        QVERIFY(growSpec != nullptr);
        QString err;
        const auto parsed = CliCommandSpecs::parse(
            *growSpec,
            {QStringLiteral("--engine"), QStringLiteral("KomoringHeights"),
             QStringLiteral("--target-moves"), QStringLiteral("101"),
             QStringLiteral("--max-positions"), QStringLiteral("1")},
            &err);
        QVERIFY2(parsed.has_value(), qPrintable(err));
        QCOMPARE(parsed->value(QStringLiteral("target_moves")).toInt(), 101);
        QCOMPARE(parsed->value(QStringLiteral("allow_final_move_alternatives")).toBool(true), false);
        QCOMPARE(parsed->value(QStringLiteral("require_no_surplus")).toBool(false), true);
        QCOMPARE(parsed->value(QStringLiteral("strict_min_length")).toBool(false), true);

        // TsumeshogiVerifier に 101手設定と拡張上限・キャッシュ注入が効くこと
        TsumeshogiVerifier::Options opt;
        opt.allowFinalMoveAlternatives = false;
        opt.requireNoSurplus = true;
        opt.maxVisitedNodes = 200000;
        opt.maxCacheEntries = 200000;
        TsumeshogiVerifier verifier;
        verifier.start(kFinalTwo3, 101, opt);
        QCOMPARE(verifier.result().status, TsumeshogiVerifier::Status::Running);
        QVERIFY(!verifier.nextPosition().isEmpty());

        // キャッシュ注入で問い合わせ数が削減されること
        TsumeshogiVerifier v1;
        v1.start(kFinalTwo3, 3, {true, true, 10000, 10000, true});
        int queriesFirst = 0;
        while (v1.result().status == TsumeshogiVerifier::Status::Running) {
            const QString q = v1.nextPosition();
            if (q.isEmpty()) continue;
            ++queriesFirst;
            const auto ans = solveWithHayanagi(q, 5);
            v1.submit(ans.kind == MateAnswer::Kind::Mate ? TsumeshogiVerifier::Reply::Mate
                                                         : TsumeshogiVerifier::Reply::NoMate,
                      ans.pv);
        }
        QCOMPARE(v1.result().status, TsumeshogiVerifier::Status::Unique);
        QVERIFY(queriesFirst > 0);
        const auto exported = v1.exportCache();
        QVERIFY(!exported.empty());

        TsumeshogiVerifier v2;
        v2.start(kFinalTwo3, 3, {true, true, 10000, 10000, true});
        v2.seedCache(exported);
        int queriesSecond = 0;
        while (v2.result().status == TsumeshogiVerifier::Status::Running) {
            const QString q = v2.nextPosition();
            if (q.isEmpty()) continue;
            ++queriesSecond;
            const auto ans = solveWithHayanagi(q, 5);
            v2.submit(ans.kind == MateAnswer::Kind::Mate ? TsumeshogiVerifier::Reply::Mate
                                                         : TsumeshogiVerifier::Reply::NoMate,
                      ans.pv);
        }
        QCOMPARE(v2.result().status, TsumeshogiVerifier::Status::Unique);
        QCOMPARE(queriesSecond, 0);
    }

    void endToEndGrowAndResumeWithMockEngine()
    {
        const QString engine = mockRetroEnginePath();
        QVERIFY2(QFile::exists(engine), qPrintable(engine));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());
        const QString checkpoint = tempDir.filePath(QStringLiteral("grow_cp.json"));

        // 0手詰み局面から 5手詰（完全厳密：最終手余詰なし・駒余りなし・不要駒なし）を完全自動生成
        TsumeRetrogradeGenerator::Settings s;
        s.enginePath = engine;
        s.engineName = QStringLiteral("MockRetro");
        s.targetMoves = 5;
        s.maxPositionsToFind = 1;
        s.beamWidth = 8;
        s.branchFactor = 16;
        s.maxAttackPieces = 8;
        s.maxDefendPieces = 5;
        s.stepTimeoutMs = 1500;
        s.verifyQueryTimeoutMs = 3000;
        s.verifyBudgetMs = 60000;
        s.allowFinalMoveAlternatives = false;
        s.requireNoSurplus = true;
        s.strictMinLength = true;
        s.checkpointPath = checkpoint;
        s.checkpointIntervalSec = 5;
        s.rngSeed = 42;

        TsumeRetrogradeGenerator gen;
        QSignalSpy foundSpy(&gen, &TsumeRetrogradeGenerator::positionFound);
        QSignalSpy finishedSpy(&gen, &TsumeRetrogradeGenerator::finished);
        QSignalSpy errorSpy(&gen, &TsumeRetrogradeGenerator::errorOccurred);

        QVERIFY(gen.start(s));
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.size() == 1, 60000);
        QCOMPARE(errorSpy.size(), 0);
        QCOMPARE(foundSpy.size(), 1);
        const QString foundSfen = foundSpy.at(0).at(0).toString();
        const QStringList foundPv = foundSpy.at(0).at(1).toStringList();
        QCOMPARE(foundPv.size(), 5);
        QVERIFY(TsumeCollection::validMateLine(foundSfen, foundPv));
        QVERIFY(QFile::exists(checkpoint));

        // 独立検証：全主手順完全一意（最終手も含む）・駒余りなし・不要駒なし
        TsumeStrictCertifier::Settings verifyCfg;
        verifyCfg.targetMoves = 5;
        verifyCfg.allowFinalMoveAlternatives = false;
        verifyCfg.requireNoSurplus = true;
        TsumeStrictCertifier independent(foundSfen, verifyCfg);
        driveCertifier(independent, 5000, 7);
        QCOMPARE(independent.stage(), TsumeStrictCertifier::Stage::Certified);
        QVERIFY(independent.outcome().has_value());
        QCOMPARE(independent.outcome()->removedPieces, 0); // すでに不要駒ゼロであること

        // 途中保存したチェックポイントから、さらに2問目を求めて再開できること
        s.resumeFromCheckpoint = true;
        s.maxPositionsToFind = 2;
        TsumeRetrogradeGenerator resumed;
        QSignalSpy resumedFound(&resumed, &TsumeRetrogradeGenerator::positionFound);
        QSignalSpy resumedFinished(&resumed, &TsumeRetrogradeGenerator::finished);
        QSignalSpy resumedError(&resumed, &TsumeRetrogradeGenerator::errorOccurred);
        QVERIFY(resumed.start(s));
        QTRY_VERIFY_WITH_TIMEOUT(resumedFinished.size() == 1, 60000);
        QCOMPARE(resumedError.size(), 0);
        QCOMPARE(resumedFound.size(), 1);
        QCOMPARE(resumed.progress().found, 2);
        QCOMPARE(resumed.foundPositions().size(), 2);
        QCOMPARE(resumed.foundPositions().at(0).sfen, foundSfen);
        QVERIFY(resumed.foundPositions().at(1).sfen != foundSfen);
    }

    void cliGrowTsume5OutputAnd101Resume()
    {
        const QString cli = cliExecutablePath();
        const QString engine = mockRetroEnginePath();
        QVERIFY2(QFile::exists(cli), qPrintable(cli));
        QVERIFY2(QFile::exists(engine), qPrintable(engine));

        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        // テスト用設定ディレクトリに KomoringHeights 互換モックを登録
        const QString configHome = tempDir.filePath(QStringLiteral("cfg"));
        QDir().mkpath(configHome + QStringLiteral("/ShogiBoardQ"));
        {
            QSettings reg(configHome + QStringLiteral("/ShogiBoardQ/ShogiBoardQ.ini"), QSettings::IniFormat);
            reg.beginWriteArray(QStringLiteral("Engines"));
            reg.setArrayIndex(0);
            reg.setValue(QStringLiteral("name"), QStringLiteral("MockRetro"));
            reg.setValue(QStringLiteral("path"), engine);
            reg.endArray();
            reg.sync();
        }

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert(QStringLiteral("SHOGIBOARDQ_CONFIG_HOME"), configHome);
        env.insert(QStringLiteral("XDG_CONFIG_HOME"), configHome);
        env.insert(QStringLiteral("QT_QPA_PLATFORM"), QStringLiteral("offscreen"));

        // A. 5手詰を完全自動生成して --output ファイルと --checkpoint へ書き出す
        {
            const QString cp5 = tempDir.filePath(QStringLiteral("cp5.json"));
            const QString out5 = tempDir.filePath(QStringLiteral("out5.txt"));
            QProcess proc;
            proc.setProcessEnvironment(env);
            proc.start(cli, {
                QStringLiteral("grow-tsume"),
                QStringLiteral("--engine"), QStringLiteral("MockRetro"),
                QStringLiteral("--target-moves"), QStringLiteral("5"),
                QStringLiteral("--max-positions"), QStringLiteral("1"),
                QStringLiteral("--max-attack"), QStringLiteral("8"),
                QStringLiteral("--max-defend"), QStringLiteral("5"),
                QStringLiteral("--beam-width"), QStringLiteral("8"),
                QStringLiteral("--branch-factor"), QStringLiteral("16"),
                QStringLiteral("--checkpoint"), cp5,
                QStringLiteral("--output"), out5,
                QStringLiteral("--rng-seed"), QStringLiteral("77")
            });
            QVERIFY(proc.waitForStarted(5000));
            const bool finishedOk = proc.waitForFinished(60000);
            const QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
            QVERIFY2(finishedOk, qPrintable(stdoutText));
            QCOMPARE(proc.exitCode(), 0);
            QVERIFY2(stdoutText.contains(QStringLiteral("\"event\":\"position\"")), qPrintable(stdoutText));
            QVERIFY2(stdoutText.contains(QStringLiteral("\"event\":\"finished\"")), qPrintable(stdoutText));
            QVERIFY(QFile::exists(cp5));
            QFile outFile(out5);
            QVERIFY(outFile.open(QIODevice::ReadOnly | QIODevice::Text));
            const QString content = QString::fromUtf8(outFile.readAll());
            QVERIFY2(content.contains(QStringLiteral(" moves ")), qPrintable(content));
            const auto parsedCollection = TsumeCollection::parse(content);
            QCOMPARE(parsedCollection.problems.size(), 1);
            QCOMPARE(parsedCollection.problems.first().referenceMoves.size(), 5);
            QVERIFY(TsumeCollection::validMateLine(parsedCollection.problems.first().sfen,
                                                   parsedCollection.problems.first().referenceMoves));
        }

        // B. 101手目標で起動 → stop でチェックポイント保存 → --resume で再開 → stop で再保存
        const QString checkpoint = tempDir.filePath(QStringLiteral("cp101.json"));
        const QString outputFile = tempDir.filePath(QStringLiteral("out101.txt"));
        {
            QProcess proc;
            proc.setProcessEnvironment(env);
            proc.start(cli, {
                QStringLiteral("grow-tsume"),
                QStringLiteral("--engine"), QStringLiteral("MockRetro"),
                QStringLiteral("--target-moves"), QStringLiteral("101"),
                QStringLiteral("--max-positions"), QStringLiteral("1"),
                QStringLiteral("--step-timeout-ms"), QStringLiteral("200"),
                QStringLiteral("--checkpoint"), checkpoint,
                QStringLiteral("--output"), outputFile,
                QStringLiteral("--rng-seed"), QStringLiteral("101"),
                QStringLiteral("--stdin-control")
            });
            QVERIFY(proc.waitForStarted(5000));
            QTest::qWait(800);
            proc.write("stop\n");
            proc.closeWriteChannel();
            QVERIFY(proc.waitForFinished(15000));
            QCOMPARE(proc.exitCode(), 0);
            const QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
            QVERIFY2(stdoutText.contains(QStringLiteral("\"event\":\"started\"")), qPrintable(stdoutText));
            QVERIFY2(stdoutText.contains(QStringLiteral("\"target_moves\":101")), qPrintable(stdoutText));
            QVERIFY2(stdoutText.contains(QStringLiteral("\"event\":\"checkpoint\"")), qPrintable(stdoutText));
            QVERIFY2(stdoutText.contains(QStringLiteral("\"event\":\"finished\"")), qPrintable(stdoutText));
            QVERIFY(QFile::exists(checkpoint));
        }

        {
            QProcess proc;
            proc.setProcessEnvironment(env);
            proc.start(cli, {
                QStringLiteral("grow-tsume"),
                QStringLiteral("--engine"), QStringLiteral("MockRetro"),
                QStringLiteral("--target-moves"), QStringLiteral("101"),
                QStringLiteral("--max-positions"), QStringLiteral("1"),
                QStringLiteral("--step-timeout-ms"), QStringLiteral("200"),
                QStringLiteral("--checkpoint"), checkpoint,
                QStringLiteral("--resume"),
                QStringLiteral("--output"), outputFile,
                QStringLiteral("--stdin-control")
            });
            QVERIFY(proc.waitForStarted(5000));
            QTest::qWait(600);
            proc.write("stop\n");
            proc.closeWriteChannel();
            QVERIFY(proc.waitForFinished(15000));
            QCOMPARE(proc.exitCode(), 0);
            const QString stdoutText = QString::fromUtf8(proc.readAllStandardOutput());
            QVERIFY2(stdoutText.contains(QStringLiteral("\"resume\":true")), qPrintable(stdoutText));
            QVERIFY2(stdoutText.contains(QStringLiteral("\"event\":\"finished\"")), qPrintable(stdoutText));
        }
    }

    void komoringRetrogradeIntegration()
    {
        const QString enginePath = qEnvironmentVariable("SHOGIBOARDQ_TEST_KOMORING");
        if (enginePath.isEmpty()) QSKIP("Set SHOGIBOARDQ_TEST_KOMORING for the optional real-engine test");

        TsumeRetrogradeGenerator::Settings s;
        s.enginePath = enginePath;
        s.engineName = QStringLiteral("KomoringHeights");
        s.targetMoves = 7;
        s.maxPositionsToFind = 1;
        s.beamWidth = 10;
        s.branchFactor = 20;
        s.maxAttackPieces = 10;
        s.maxDefendPieces = 6;
        s.stepTimeoutMs = 3000;
        s.verifyQueryTimeoutMs = 10000;
        s.verifyBudgetMs = 120000;
        s.allowFinalMoveAlternatives = false;
        s.requireNoSurplus = true;
        s.strictMinLength = true;
        s.rngSeed = 2026;

        TsumeRetrogradeGenerator gen;
        QSignalSpy foundSpy(&gen, &TsumeRetrogradeGenerator::positionFound);
        QSignalSpy finishedSpy(&gen, &TsumeRetrogradeGenerator::finished);
        QSignalSpy errorSpy(&gen, &TsumeRetrogradeGenerator::errorOccurred);
        QVERIFY(gen.start(s));
        QTRY_VERIFY_WITH_TIMEOUT(finishedSpy.size() == 1, 180000);
        QCOMPARE(errorSpy.size(), 0);
        QCOMPARE(foundSpy.size(), 1);
        const QString foundSfen = foundSpy.at(0).at(0).toString();
        const QStringList foundPv = foundSpy.at(0).at(1).toStringList();
        QCOMPARE(foundPv.size(), 7);
        QVERIFY(TsumeCollection::validMateLine(foundSfen, foundPv));
        qInfo() << "KomoringHeights retrograde 7-ply:" << foundSfen << "moves" << foundPv.join(QLatin1Char(' '));
    }
};

QTEST_GUILESS_MAIN(TestTsumeshogiRetrograde)
#include "tst_tsumeshogi_retrograde.moc"
