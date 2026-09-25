/// @file tst_tsumeshogi_screener.cpp
/// @brief 詰将棋局面生成の事前選別（TsumeshogiCandidateScreener）テスト

#include <QtTest>

#include "threadtypes.h"
#include "tsumeshogicandidatescreener.h"
#include "tsumeshogipositiongenerator.h"

#include <atomic>

namespace {
using Verdict = TsumeshogiCandidateScreener::Verdict;
using Limits = TsumeshogiCandidateScreener::Limits;

// 王手が N*2c の1種類しかなく応手が0の1手詰
const QString kUnique = QStringLiteral("7nk/7nn/9/9/9/9/9/9/9 b N 1");
// 初手 3c3b+ だけが詰む3手詰（KomoringHeights 1.1.0 で照合済み）
const QString kFinalTwo = QStringLiteral("7k1/9/6L+S1/9/9/9/9/9/9 b 2r2b4g3s4n3l18p 1");
// 即詰みが 3b3a と 3b3a+ の2手ある1手詰
const QString kTwoMates = QStringLiteral("8k/6Rnn/9/9/9/9/9/9/9 b - 1");
// 初手 2d2c は3手、P*1c は5手で詰む（KomoringHeights 1.1.0 で照合済み）
const QString kLongerAlternative = QStringLiteral("5S2p/8k/9/7G1/7R1/9/9/9/9 b Pr2b3g3s4n4l16p 1");
// 初手 4d1d は3手、G*2b は7手で詰む（KomoringHeights 1.1.0 で照合済み）
const QString kMuchLongerAlternative = QStringLiteral("6S2/8k/6r2/5+R3/9/9/9/9/9 b G2b3g3s4n4l18p 1");

const std::atomic_bool kNoStop{false};

Limits generous(int extraPlies = 0)
{
    Limits limits;
    limits.timeLimitMs = 3000;
    limits.alternativeTimeLimitMs = 3000;
    limits.alternativeExtraPlies = extraPlies;
    return limits;
}
}

class TestTsumeshogiScreener : public QObject
{
    Q_OBJECT

private slots:
    void acceptsExactLengthUniqueFirstMove()
    {
        QCOMPARE(TsumeshogiCandidateScreener::screen(kUnique, 1, generous(), kNoStop), Verdict::Candidate);
        QCOMPARE(TsumeshogiCandidateScreener::screen(kFinalTwo, 3, generous(), kNoStop), Verdict::Candidate);
        // 最終手の複数解は初手の一意性に影響しない（採否は TsumeshogiVerifier が決める）
        QCOMPARE(TsumeshogiCandidateScreener::screen(kFinalTwo, 3, generous(2), kNoStop), Verdict::Candidate);
    }
    void rejectsShorterOrMissingMate()
    {
        // 1手で詰む局面は3手詰の候補にしない（エンジンも1手のPVを返す）
        QCOMPARE(TsumeshogiCandidateScreener::screen(kUnique, 3, generous(), kNoStop), Verdict::NotMateInTarget);
        QCOMPARE(TsumeshogiCandidateScreener::screen(QStringLiteral("k8/9/9/9/9/9/9/9/9 b - 1"), 1, generous(), kNoStop),
                 Verdict::NotMateInTarget);
        // 目標手数以内に詰みがない
        QCOMPARE(TsumeshogiCandidateScreener::screen(kFinalTwo, 1, generous(), kNoStop), Verdict::NotMateInTarget);
    }
    void rejectsMultipleFirstMoves()
    {
        QCOMPARE(TsumeshogiCandidateScreener::screen(kTwoMates, 1, generous(), kNoStop), Verdict::MultipleFirstMoves);
    }
    void longerAlternativesFollowExtraPlies()
    {
        // 上乗せなしでは5手の別初手を見つけないので候補のまま。2手上乗せで余詰として除外する
        QCOMPARE(TsumeshogiCandidateScreener::screen(kLongerAlternative, 3, generous(0), kNoStop), Verdict::Candidate);
        QCOMPARE(TsumeshogiCandidateScreener::screen(kLongerAlternative, 3, generous(2), kNoStop),
                 Verdict::MultipleFirstMoves);
        // 7手の別初手は2手上乗せでは届かず、4手上乗せで見つかる
        QCOMPARE(TsumeshogiCandidateScreener::screen(kMuchLongerAlternative, 3, generous(2), kNoStop), Verdict::Candidate);
        QCOMPARE(TsumeshogiCandidateScreener::screen(kMuchLongerAlternative, 3, generous(4), kNoStop),
                 Verdict::MultipleFirstMoves);
    }
    void unknownForInvalidInputAndTimeouts()
    {
        QCOMPARE(TsumeshogiCandidateScreener::screen(QStringLiteral("invalid"), 3, generous(), kNoStop), Verdict::Unknown);
        QCOMPARE(TsumeshogiCandidateScreener::screen(kFinalTwo, 2, generous(), kNoStop), Verdict::Unknown);
        QCOMPARE(TsumeshogiCandidateScreener::screen(kFinalTwo, 65, generous(), kNoStop), Verdict::Unknown);
        // 中断フラグが立っていれば探索せずに Unknown
        const std::atomic_bool stop{true};
        QCOMPARE(TsumeshogiCandidateScreener::screen(kFinalTwo, 3, generous(), stop), Verdict::Unknown);
    }
    void batchReturnsOnlyScreenedCandidates()
    {
        TsumeshogiPositionGenerator::Settings settings;
        settings.maxAttackPieces = 4;
        settings.maxDefendPieces = 1;
        settings.attackRange = 3;
        Limits limits;
        limits.timeLimitMs = 100;
        limits.alternativeTimeLimitMs = 100;
        limits.alternativeExtraPlies = 2;
        const auto batch = TsumeshogiCandidateScreener::generateBatch(settings, 3, 2, 300, limits, makeCancelFlag());
        QVERIFY(batch.generated >= 1);
        QVERIFY(batch.generated <= 300);
        QVERIFY(batch.candidates.size() <= 2);
        QVERIFY(batch.candidates.size() == 2 || batch.generated == 300);
        for (const QString& sfen : batch.candidates) {
            const auto verdict = TsumeshogiCandidateScreener::screen(sfen, 3, generous(2), kNoStop);
            QVERIFY2(verdict == Verdict::Candidate || verdict == Verdict::Unknown, qPrintable(sfen));
        }
    }
    void batchStopsOnCancel()
    {
        TsumeshogiPositionGenerator::Settings settings;
        auto cancel = makeCancelFlag();
        cancel->store(true);
        const auto batch = TsumeshogiCandidateScreener::generateBatch(settings, 3, 8, 2000, Limits{}, cancel);
        QCOMPARE(batch.generated, 0);
        QVERIFY(batch.candidates.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestTsumeshogiScreener)
#include "tst_tsumeshogi_screener.moc"
