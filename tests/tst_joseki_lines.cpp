/// @file tst_joseki_lines.cpp
/// @brief 定跡を棋譜の変化に展開する処理と、手順が定跡を外れた手を探す処理（JosekiLines）のテスト

#include <QtTest>

#include "josekibook.h"
#include "josekilines.h"
#include "sfenreplay.h"
#include "sfenutils.h"

namespace {

JosekiMove bookMove(const QString &usi, int frequency)
{
    JosekiMove move;
    move.move = usi;
    move.nextMove = std::nullopt;
    move.frequency = frequency;
    return move;
}

QString after(const QString &sfen, const QStringList &moves)
{
    QString position = sfen;
    for (const QString &move : moves) position = SfenReplay::positionAfter(position, move);
    return position;
}

QStringList movesOf(const RecordLine &line)
{
    return line.usiMoves();
}

} // namespace

class TestJosekiLines : public QObject
{
    Q_OBJECT

private:
    static QString hirate() { return SfenUtils::hirateSfen(); }

    /// ▲７六歩（頻度6）・▲２六歩（頻度3）・指せない ▲５四歩、どちらの後も △３四歩。
    /// ▲７六歩 △３四歩 ▲２六歩 と ▲２六歩 △３四歩 ▲７六歩 は同じ局面に合流し、その局面に △８四歩
    static JosekiBook sampleBook()
    {
        JosekiBook book;
        book.putMove(hirate(), bookMove(QStringLiteral("7g7f"), 6));
        book.putMove(hirate(), bookMove(QStringLiteral("2g2f"), 3));
        book.putMove(hirate(), bookMove(QStringLiteral("5e5d"), 1));
        book.putMove(after(hirate(), {"7g7f"}), bookMove(QStringLiteral("3c3d"), 1));
        book.putMove(after(hirate(), {"2g2f"}), bookMove(QStringLiteral("3c3d"), 1));
        book.putMove(after(hirate(), {"7g7f", "3c3d"}), bookMove(QStringLiteral("2g2f"), 1));
        book.putMove(after(hirate(), {"2g2f", "3c3d"}), bookMove(QStringLiteral("7g7f"), 1));
        book.putMove(after(hirate(), {"7g7f", "3c3d", "2g2f"}), bookMove(QStringLiteral("8c8d"), 1));
        return book;
    }

    static JosekiLines::ExpandResult expand(const JosekiLines::ExpandOptions &options)
    {
        return JosekiLines::expand(sampleBook(), hirate(), 1, std::nullopt, options);
    }

private slots:
    /// 表の順にたどり、指せない手は使わず、別の手順で同じ局面に着いたらそこで止める
    void expand_stopsAtTranspositions()
    {
        const JosekiLines::ExpandResult result = expand({});
        QCOMPARE(result.lines.size(), 2);
        QCOMPARE(movesOf(result.lines.at(0)), (QStringList{"7g7f", "3c3d", "2g2f", "8c8d"}));
        QCOMPARE(movesOf(result.lines.at(1)), (QStringList{"2g2f", "3c3d", "7g7f"}));
        QCOMPARE(result.moveCount, 7);
        QCOMPARE(result.transpositions, 1);
        QVERIFY(!result.truncated);
        // 手順は棋譜の行（表記・指した後の局面）として作る
        const RecordLine &first = result.lines.constFirst();
        QCOMPARE(first.startPly, 1);
        QCOMPARE(first.startSfen.section(QLatin1Char(' '), 0, 2), hirate().section(QLatin1Char(' '), 0, 2));
        QCOMPARE(first.entries.at(0).displayText, QStringLiteral("▲７六歩(77)"));
        QCOMPARE(first.entries.at(2).sfen.section(QLatin1Char(' '), 0, 2),
                 after(hirate(), {"7g7f", "3c3d", "2g2f"}).section(QLatin1Char(' '), 0, 2));
    }

    /// 手数・1局面の手の数・比率の下限・手の数の上限で絞る
    void expand_limits()
    {
        JosekiLines::ExpandOptions options;
        options.maxPlies = 2;
        JosekiLines::ExpandResult result = expand(options);
        QCOMPARE(result.lines.size(), 2);
        QCOMPARE(movesOf(result.lines.at(1)), (QStringList{"2g2f", "3c3d"}));
        QCOMPARE(result.moveCount, 4);

        options = {};
        options.maxMovesPerPosition = 1;
        result = expand(options);
        QCOMPARE(result.lines.size(), 1);
        QCOMPARE(result.moveCount, 4);

        // 比率は指せない手も含めた合計に対する割合（▲２六歩は 3/10 = 30%）
        options = {};
        options.minRatioPercent = 30.0;
        QCOMPARE(expand(options).lines.size(), 2);
        options.minRatioPercent = 30.1;
        QCOMPARE(expand(options).lines.size(), 1);

        options = {};
        options.maxMoves = 2;
        result = expand(options);
        QVERIFY(result.truncated);
        QCOMPARE(result.moveCount, 2);
        QCOMPARE(movesOf(result.lines.constFirst()), (QStringList{"7g7f", "3c3d"}));
    }

    /// 開始局面の手数と、開始局面に至った手（「同」の基準）を使う
    void expand_fromMiddleOfRecord()
    {
        JosekiBook book;
        const QString start = after(hirate(), {"7g7f", "3c3d", "8h2b+"});
        book.putMove(start, bookMove(QStringLiteral("3a2b"), 1));
        const JosekiLines::ExpandResult result =
            JosekiLines::expand(book, start, 4, UsiMove::parse(QStringLiteral("8h2b+")), {});
        QCOMPARE(result.lines.size(), 1);
        QCOMPARE(result.lines.constFirst().startPly, 4);
        QCOMPARE(result.lines.constFirst().entries.constFirst().displayText, QStringLiteral("△同　銀(31)"));
        QVERIFY(JosekiLines::expand(book, hirate(), 1, std::nullopt, {}).lines.isEmpty());
    }

    /// 手順が定跡を外れた最初の手（すべて定跡にあれば nullopt）
    void firstDeviation()
    {
        const JosekiBook book = sampleBook();
        const QStringList moves{"7g7f", "3c3d", "2g2f", "4c4d", "8c8d"};
        QStringList sfens{hirate()};
        for (const QString &move : moves) sfens << SfenReplay::positionAfter(sfens.constLast(), move);
        QCOMPARE(JosekiLines::firstDeviation(book, sfens, moves), std::optional<int>(3));
        QCOMPARE(JosekiLines::firstDeviation(book, sfens, moves.mid(0, 3)), std::nullopt);
        QCOMPARE(JosekiLines::firstDeviation(book, sfens, {"6g6f"}), std::optional<int>(0));
        QCOMPARE(JosekiLines::firstDeviation(book, sfens, {}), std::nullopt);
    }
};

QTEST_MAIN(TestJosekiLines)
#include "tst_joseki_lines.moc"
