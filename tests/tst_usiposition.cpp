/// @file tst_usiposition.cpp
/// @brief エンジンに探索させる局面（UsiPosition）のテスト

#include <QtTest>

#include "usiposition.h"

namespace {
const QString kHirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
}

class TestUsiPosition : public QObject
{
    Q_OBJECT

private slots:
    /// 棋譜の USI 形式の書き出しと共有する position 文字列（開始局面の手数は書かれたまま）
    void commandForKeepsWrittenStart()
    {
        QCOMPARE(UsiPosition::commandFor(QString(), {}), QStringLiteral("position startpos"));
        QCOMPARE(UsiPosition::commandFor(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"),
                                         {QStringLiteral("7g7f")}),
                 QStringLiteral("position startpos moves 7g7f"));
        const QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 5");
        QCOMPARE(UsiPosition::commandFor(sfen, {}), QStringLiteral("position sfen ") + sfen);
    }

    void emptyPosition_sendsNothing()
    {
        const UsiPosition position;
        QVERIFY(position.isEmpty());
        QVERIFY(position.command().isEmpty());
        QVERIFY(position.sfen().isEmpty());
        QVERIFY(!position.withMove(UsiMove::fromString(u"7g7f")));
    }

    void command_roundTrips_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("command");
        QTest::newRow("startpos") << QStringLiteral("position startpos") << QStringLiteral("position startpos");
        QTest::newRow("startpos-moves") << QStringLiteral("position startpos moves 7g7f 3c3d")
                                        << QStringLiteral("position startpos moves 7g7f 3c3d");
        // 平手の SFEN は startpos で送る（手数が 1 のときだけ）
        QTest::newRow("hirate-sfen") << QStringLiteral("position sfen ") + kHirate << QStringLiteral("position startpos");
        QTest::newRow("hirate-counter") << QStringLiteral("sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 41")
                                        << QStringLiteral("position sfen lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 41");
        QTest::newRow("bare-sfen") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G 1")
                                   << QStringLiteral("position sfen 4k4/9/9/9/9/9/9/9/4K4 b G 1");
        QTest::newRow("three-fields") << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b G")
                                      << QStringLiteral("position sfen 4k4/9/9/9/9/9/9/9/4K4 b G 1");
        QTest::newRow("moves-keyword-only") << QStringLiteral("position startpos moves")
                                            << QStringLiteral("position startpos");
    }

    void command_roundTrips()
    {
        QFETCH(QString, text);
        QFETCH(QString, command);
        const std::optional<UsiPosition> position = UsiPosition::parse(text);
        QVERIFY(position);
        QCOMPARE(position->command(), command);
        QCOMPARE(UsiPosition::parse(position->command()), position);
    }

    void parse_rejectsUnreadable()
    {
        QString error;
        QVERIFY(!UsiPosition::parse(u"position sfen xyz b - 1", &error));
        QVERIFY(!error.isEmpty());
        error.clear();
        // 移動元が空の手は指せない
        QVERIFY(!UsiPosition::parse(u"position startpos moves 5e5d", &error));
        QVERIFY(error.contains(QStringLiteral("5e5d")));
        QVERIFY(!UsiPosition::parse(u"position startpos moves zz"));
    }

    void withMove_tracksSideAndLastMove()
    {
        const UsiPosition start = *UsiPosition::fromSfen(u"startpos");
        QCOMPARE(start.sideToMove(), Turn::Black);
        QVERIFY(!start.lastMove());

        const UsiPosition after = *start.withMove(UsiMove::fromString(u"7g7f"));
        QCOMPARE(after.sideToMove(), Turn::White);
        QCOMPARE(after.lastMove()->toString(), QStringLiteral("7g7f"));
        QCOMPARE(after.moves(), QStringList{QStringLiteral("7g7f")});
        QCOMPARE(after.startSfen(), kHirate);
        QCOMPARE(after.sfen(), QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));
        // 元の値は変わらない
        QVERIFY(start.moves().isEmpty());
    }

    void fromSfen_keepsKnownLastMove()
    {
        // 局面だけを送る検討でも、そこに至った手を「同」の基準に持つ
        const QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2");
        const UsiPosition position = *UsiPosition::fromSfen(sfen, UsiMove::fromString(u"7g7f"));
        QCOMPARE(position.command(), QStringLiteral("position sfen ") + sfen);
        QCOMPARE(position.lastMove()->to, (BoardSquare{7, 6}));
        QVERIFY(position != *UsiPosition::fromSfen(sfen));
    }

    void equality_comparesStartAndMoves()
    {
        // 予想手での先読みの局面と、実際に指された局面が同じなら当たり
        const UsiPosition played = *UsiPosition::parse(u"position startpos moves 7g7f 3c3d");
        const UsiPosition ponder = *played.withMove(UsiMove::fromString(u"2g2f"));
        QCOMPARE(*UsiPosition::parse(u"position startpos moves 7g7f 3c3d 2g2f"), ponder);
        QVERIFY(*UsiPosition::parse(u"position startpos moves 7g7f 3c3d 6g6f") != ponder);
        // 同じ局面でも手順が違えば別の値（千日手の判断材料が違う）
        QVERIFY(*UsiPosition::parse(u"position startpos moves 2g2f 3c3d 7g7f") != ponder);
    }

    void fromMoves_replaysFromStart()
    {
        const auto position = UsiPosition::fromMoves(kHirate, {UsiMove::fromString(u"7g7f"), UsiMove::fromString(u"3c3d")});
        QVERIFY(position);
        QCOMPARE(position->command(), QStringLiteral("position startpos moves 7g7f 3c3d"));
        QVERIFY(!UsiPosition::fromMoves(kHirate, {UsiMove::fromString(u"5e5d")}));
        QVERIFY(!UsiPosition::fromMoves(QStringLiteral("bad"), {}));
    }
};

QTEST_MAIN(TestUsiPosition)
#include "tst_usiposition.moc"
