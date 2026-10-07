/// @file tst_sennichite_detector.cpp
/// @brief 千日手・連続王手の千日手の判定（SennichiteDetector）の単体テスト

#include <QtTest>

#include "sennichitedetector.h"

namespace {

/// 4局面の繰り返し（cycle）を start から順に並べ、同じ局面が4回目に出た時点までの SFEN 履歴を作る。
/// cycle の各要素は手数を除いた "盤面 手番 持駒"。
QStringList buildRecord(const QStringList& cycle, int startIndex, int plies)
{
    QStringList record;
    for (int ply = 0; ply <= plies; ++ply) {
        const QString& key = cycle.at((startIndex + ply) % cycle.size());
        record.append(key + QStringLiteral(" ") + QString::number(ply + 1));
    }
    return record;
}

// 先手の飛車が 2五↔1五 で毎手王手をかけ、後手玉が 1一↔2一 に逃げる（先手番の局面から）
const QStringList kBlackChecks = {
    QStringLiteral("8k/9/9/9/7R1/9/9/9/4K4 b -"),
    QStringLiteral("8k/9/9/9/8R/9/9/9/4K4 w -"),   // ▲１五飛 王手
    QStringLiteral("7k1/9/9/9/8R/9/9/9/4K4 b -"),  // △２一玉
    QStringLiteral("7k1/9/9/9/7R1/9/9/9/4K4 w -"), // ▲２五飛 王手
};

// 後手の飛車が 8五↔9五 で毎手王手をかけ、先手玉が 9九↔8九 に逃げる（後手番の局面から）
const QStringList kWhiteChecks = {
    QStringLiteral("4k4/9/9/9/1r7/9/9/9/K8 w -"),
    QStringLiteral("4k4/9/9/9/r8/9/9/9/K8 b -"),   // △９五飛 王手
    QStringLiteral("4k4/9/9/9/r8/9/9/9/1K7 w -"),  // ▲８九玉
    QStringLiteral("4k4/9/9/9/1r7/9/9/9/1K7 b -"), // △８五飛 王手
};

// 双方の玉が往復するだけ（王手なし）
const QStringList kKingsShuffle = {
    QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b -"),
    QStringLiteral("4k4/9/9/9/9/9/9/4K4/9 w -"),
    QStringLiteral("9/4k4/9/9/9/9/9/4K4/9 b -"),
    QStringLiteral("9/4k4/9/9/9/9/9/9/4K4 w -"),
};

} // namespace

class TestSennichiteDetector : public QObject
{
    Q_OBJECT

private slots:
    void check_data()
    {
        QTest::addColumn<QStringList>("record");
        QTest::addColumn<int>("expected");

        using R = SennichiteDetector::Result;
        // 12手目で開始局面が4回目。先手の王手を後手が逃げた手で成立する
        QTest::newRow("black-checks-completed-by-escape") << buildRecord(kBlackChecks, 0, 12)
                                                          << int(R::ContinuousCheckByP1);
        // 王手をかけた局面から始めると、先手の王手の手で成立する
        QTest::newRow("black-checks-completed-by-check") << buildRecord(kBlackChecks, 1, 12)
                                                         << int(R::ContinuousCheckByP1);
        QTest::newRow("white-checks-completed-by-escape") << buildRecord(kWhiteChecks, 0, 12)
                                                          << int(R::ContinuousCheckByP2);
        QTest::newRow("white-checks-completed-by-check") << buildRecord(kWhiteChecks, 1, 12)
                                                         << int(R::ContinuousCheckByP2);
        QTest::newRow("no-checks-is-draw") << buildRecord(kKingsShuffle, 0, 12) << int(R::Draw);
        // 3回目まではまだ千日手ではない
        QTest::newRow("third-occurrence") << buildRecord(kBlackChecks, 0, 8) << int(R::None);
        QTest::newRow("not-repeated-yet") << buildRecord(kBlackChecks, 0, 11) << int(R::None);
    }

    void check()
    {
        QFETCH(QStringList, record);
        QFETCH(int, expected);
        QCOMPARE(int(SennichiteDetector::check(record)), expected);
    }

    void positionKey_dropsMoveNumber()
    {
        QCOMPARE(SennichiteDetector::positionKey(QStringLiteral("8k/9/9/9/7R1/9/9/9/4K4 b - 13")),
                 QStringLiteral("8k/9/9/9/7R1/9/9/9/4K4 b -"));
    }
};

QTEST_MAIN(TestSennichiteDetector)
#include "tst_sennichite_detector.moc"
