/// @file tst_joseki_move_picker.cpp
/// @brief 対局で指す定跡手の選び方（JosekiMovePicker）のテスト

#include <QtTest>
#include <QRandomGenerator>

#include "josekibook.h"
#include "josekimovepicker.h"
#include "sfenutils.h"

namespace {

JosekiMove bookMove(const QString &usi, int value, int depth, int frequency)
{
    JosekiMove move;
    move.move = usi;
    move.nextMove = std::nullopt;
    move.value = value;
    move.depth = depth;
    move.frequency = frequency;
    return move;
}

QStringList movesOf(const QList<JosekiMove> &moves)
{
    QStringList usi;
    for (const JosekiMove &move : moves) usi.append(move.move);
    return usi;
}

} // namespace

class TestJosekiMovePicker : public QObject
{
    Q_OBJECT

private:
    static QString hirate() { return SfenUtils::hirateSfen(); }

    /// 平手初期局面に 7g7f（値30・深さ20・頻度60）、2g2f（値10・深さ4・頻度30）、
    /// 5g5f（値-200・深さ20・頻度10）、指せない手 5e5d（頻度100）を登録した定跡
    static JosekiBook sampleBook()
    {
        JosekiBook book;
        book.putMove(hirate(), bookMove(QStringLiteral("7g7f"), 30, 20, 60));
        book.putMove(hirate(), bookMove(QStringLiteral("2g2f"), 10, 4, 30));
        book.putMove(hirate(), bookMove(QStringLiteral("5g5f"), -200, 20, 10));
        book.putMove(hirate(), bookMove(QStringLiteral("5e5d"), 0, 20, 100));
        return book;
    }

    static JosekiPlayOptions enabled()
    {
        JosekiPlayOptions options;
        options.enabled = true;
        return options;
    }

private slots:
    /// 指せない手は使わず、深さ・出現頻度・評価値・比率の下限で絞る（比率は指せない手も含めた合計に対する割合）
    void candidates_filterByConditions()
    {
        const QList<JosekiMove> moves = sampleBook().movesAt(hirate());
        JosekiPlayOptions options = enabled();
        QCOMPARE(movesOf(JosekiMovePicker::candidates(moves, hirate(), options)),
                 (QStringList{QStringLiteral("7g7f"), QStringLiteral("2g2f"), QStringLiteral("5g5f")}));

        options.minDepth = 10;
        QCOMPARE(movesOf(JosekiMovePicker::candidates(moves, hirate(), options)),
                 (QStringList{QStringLiteral("7g7f"), QStringLiteral("5g5f")}));
        options.minDepth = 0;
        options.useMinValue = true;
        options.minValue = 0;
        QCOMPARE(movesOf(JosekiMovePicker::candidates(moves, hirate(), options)),
                 (QStringList{QStringLiteral("7g7f"), QStringLiteral("2g2f")}));
        options.useMinValue = false;
        options.minFrequency = 31;
        QCOMPARE(movesOf(JosekiMovePicker::candidates(moves, hirate(), options)), QStringList{QStringLiteral("7g7f")});
        options.minFrequency = 0;
        options.minRatioPercent = 15.0;  // 30 / 200 = 15%（下限ちょうどは使う）
        QCOMPARE(movesOf(JosekiMovePicker::candidates(moves, hirate(), options)),
                 (QStringList{QStringLiteral("7g7f"), QStringLiteral("2g2f")}));
    }

    /// 使わない設定・手数を超えた・定跡の無い局面・条件に合う手が無いときは選ばない
    void pick_returnsNothingWhenNotApplicable()
    {
        const JosekiBook book = sampleBook();
        QRandomGenerator random(1);
        JosekiPlayOptions options = enabled();
        options.enabled = false;
        QVERIFY(!JosekiMovePicker::pick(book, hirate(), options, random));

        options = enabled();
        options.maxPly = 1;
        QVERIFY(JosekiMovePicker::pick(book, hirate(), options, random));
        const QString ply2 = hirate().left(hirate().lastIndexOf(QLatin1Char(' '))) + QStringLiteral(" 2");
        QVERIFY(!JosekiMovePicker::pick(book, ply2, options, random));

        QVERIFY(!JosekiMovePicker::pick(book, QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b - 1"), enabled(), random));
        options = enabled();
        options.minDepth = 100;
        QVERIFY(!JosekiMovePicker::pick(book, hirate(), options, random));
    }

    /// 「先頭の手」は条件に合う手のうち表の先頭。比率は指せない手も含めた合計に対する割合
    void pick_firstMove()
    {
        QRandomGenerator random(1);
        JosekiPlayOptions options = enabled();
        options.method = JosekiPlayOptions::FirstMove;
        options.minDepth = 10;
        const std::optional<JosekiPick> picked = JosekiMovePicker::pick(sampleBook(), hirate(), options, random);
        QVERIFY(picked);
        QCOMPARE(picked->move.move, QStringLiteral("7g7f"));
        QCOMPARE(picked->ratioPercent, 30.0);
    }

    /// 「評価値で選ぶ」は最善の評価値から差の範囲の手だけを選ぶ
    void pick_byValueStaysWithinDifference()
    {
        QRandomGenerator random(7);
        JosekiPlayOptions options = enabled();
        options.method = JosekiPlayOptions::ByValue;
        options.valueDiff = 30;
        QSet<QString> seen;
        for (int i = 0; i < 200; ++i) seen.insert(JosekiMovePicker::pick(sampleBook(), hirate(), options, random)->move.move);
        QCOMPARE(seen, (QSet<QString>{QStringLiteral("7g7f"), QStringLiteral("2g2f")}));
        options.valueDiff = 0;
        seen.clear();
        for (int i = 0; i < 50; ++i) seen.insert(JosekiMovePicker::pick(sampleBook(), hirate(), options, random)->move.move);
        QCOMPARE(seen, QSet<QString>{QStringLiteral("7g7f")});
    }

    /// 「出現頻度に比例」は頻度の割合で選び、頻度0の手は選ばない。すべて0なら同じ確率
    void pick_byFrequency()
    {
        QRandomGenerator random(11);
        const JosekiPlayOptions options = enabled();
        QHash<QString, int> counts;
        constexpr int kTrials = 6000;
        for (int i = 0; i < kTrials; ++i) ++counts[JosekiMovePicker::pick(sampleBook(), hirate(), options, random)->move.move];
        // 7g7f:2g2f:5g5f = 60:30:10
        QVERIFY2(qAbs(counts.value(QStringLiteral("7g7f")) - kTrials * 0.6) < kTrials * 0.05, qPrintable(QString::number(counts.value(QStringLiteral("7g7f")))));
        QVERIFY(qAbs(counts.value(QStringLiteral("2g2f")) - kTrials * 0.3) < kTrials * 0.05);
        QVERIFY(qAbs(counts.value(QStringLiteral("5g5f")) - kTrials * 0.1) < kTrials * 0.05);
        QVERIFY(!counts.contains(QStringLiteral("5e5d")));

        JosekiBook zero;
        zero.putMove(hirate(), bookMove(QStringLiteral("7g7f"), 0, 0, 0));
        zero.putMove(hirate(), bookMove(QStringLiteral("2g2f"), 0, 0, 0));
        zero.putMove(hirate(), bookMove(QStringLiteral("2h7h"), 0, 0, 5));
        QSet<QString> seen;
        for (int i = 0; i < 200; ++i) seen.insert(JosekiMovePicker::pick(zero, hirate(), options, random)->move.move);
        QCOMPARE(seen, QSet<QString>{QStringLiteral("2h7h")});
        zero.removeMove(hirate(), QStringLiteral("2h7h"));
        seen.clear();
        for (int i = 0; i < 200; ++i) seen.insert(JosekiMovePicker::pick(zero, hirate(), options, random)->move.move);
        QCOMPARE(seen, (QSet<QString>{QStringLiteral("7g7f"), QStringLiteral("2g2f")}));
    }

    /// 範囲外の値は範囲に収める（設定ファイルを手で書き換えた場合など）
    void options_normalized()
    {
        JosekiPlayOptions options;
        options.maxPly = 0;
        options.method = 9;
        options.valueDiff = -5;
        options.minRatioPercent = 150.0;
        options.minFrequency = -1;
        const JosekiPlayOptions normalized = options.normalized();
        QCOMPARE(normalized.maxPly, 1);
        QCOMPARE(normalized.method, int(JosekiPlayOptions::ByFrequency));
        QCOMPARE(normalized.valueDiff, 0);
        QCOMPARE(normalized.minRatioPercent, 100.0);
        QCOMPARE(normalized.minFrequency, 0);
    }
};

QTEST_MAIN(TestJosekiMovePicker)
#include "tst_joseki_move_picker.moc"
