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

    /// position コマンド・局面の文字列の語の分け方は split だけ（parse・sfenOf・棋譜の USI 形式の読み込みが使う）
    void split_readsEveryWrittenForm()
    {
        const QString board = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4");
        // 接頭辞と "moves" は省け、大文字と小文字を区別しない。区切りは空白・タブのどちらでもよい
        auto words = UsiPosition::split(u"POSITION Startpos\t7g7f  3c3d");
        QVERIFY(words);
        QCOMPARE(words->start, kHirate);
        QCOMPARE(words->moves, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));

        // 手数を省いた SFEN は書かれたまま。手数 1 を補うのは completeStart
        words = UsiPosition::split(QStringLiteral("sfen %1 b G moves 5h5b").arg(board));
        QVERIFY(words);
        QCOMPARE(words->start, board + QStringLiteral(" b G"));
        QCOMPARE(words->completeStart(), board + QStringLiteral(" b G 1"));
        QCOMPARE(words->moves, QStringList({QStringLiteral("5h5b")}));

        // 数字だけの4つ目の語が手数
        words = UsiPosition::split(QStringLiteral("%1 w - 12 resign").arg(board));
        QVERIFY(words);
        QCOMPARE(words->start, board + QStringLiteral(" w - 12"));
        QCOMPARE(words->completeStart(), words->start);
        QCOMPARE(words->moves, QStringList({QStringLiteral("resign")}));

        // 開始局面の欄が3つに足りない
        QVERIFY(!UsiPosition::split(u""));
        QVERIFY(!UsiPosition::split(u"position"));
        QVERIFY(!UsiPosition::split(QStringLiteral("position sfen %1 b moves 5h5b").arg(board)));

        // parse も同じ読み方（"moves" を省いた手も指す）
        const auto played = UsiPosition::parse(u"startpos 7g7f");
        QVERIFY(played);
        QCOMPARE(played->moves(), QStringList({QStringLiteral("7g7f")}));
    }

    /// 局面の文字列の SFEN（接頭辞を除き、startpos は平手。指し手を含む・分けられない文字列はそのまま）
    void sfenOf_stripsPrefixesOnly()
    {
        QCOMPARE(UsiPosition::sfenOf(u" startpos "), kHirate);
        QCOMPARE(UsiPosition::sfenOf(QStringLiteral("position sfen ") + kHirate), kHirate);
        QCOMPARE(UsiPosition::sfenOf(QStringLiteral("sfen 9/9/9/9/9/9/9/9/9 b -")), QStringLiteral("9/9/9/9/9/9/9/9/9 b -"));
        QCOMPARE(UsiPosition::sfenOf(u" startpos moves 7g7f"), QStringLiteral("startpos moves 7g7f"));
        QCOMPARE(UsiPosition::sfenOf(u"hello"), QStringLiteral("hello"));
        QCOMPARE(UsiPosition::sfenOf(u""), QString());
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

    void alongMoves_sharesTheLineAndKeepsEachPosition()
    {
        // 手順の各局面は手の列を共有するが、値としてはそれぞれの手数までの局面（セキュリティ監査 S6）
        const QList<UsiMove> moves = {UsiMove::fromString(u"7g7f"), UsiMove::fromString(u"3c3d"),
                                      UsiMove::fromString(u"2g2f")};
        const auto positions = UsiPosition::alongMoves(kHirate, moves, 1, 3);
        QVERIFY(positions);
        QCOMPARE(positions->size(), 3);
        for (int ply = 1; ply <= 3; ++ply) {
            const UsiPosition& position = positions->at(ply - 1);
            QCOMPARE(position, *UsiPosition::fromMoves(kHirate, moves.mid(0, ply)));
            QCOMPARE(position.moves().size(), ply);
            QCOMPARE(*position.lastMove(), moves.at(ply - 1));
        }
        QCOMPARE(positions->at(0).command(), QStringLiteral("position startpos moves 7g7f"));
        // 共有している後ろの手は、別の手を足しても他の局面に影響しない
        const auto other = positions->at(0).withMove(UsiMove::fromString(u"8c8d"));
        QVERIFY(other);
        QCOMPARE(other->command(), QStringLiteral("position startpos moves 7g7f 8c8d"));
        QCOMPARE(positions->at(2).command(), QStringLiteral("position startpos moves 7g7f 3c3d 2g2f"));
        QVERIFY(!UsiPosition::alongMoves(kHirate, moves, 2, 4));
        QVERIFY(!UsiPosition::alongMoves(kHirate, {UsiMove::fromString(u"5e5d")}, 0, 1));
    }

    void fromMoves_longLineIsLinear()
    {
        // 玉の往復を20万手。手ごとに手順を複製すると手数の二乗の時間とメモリになっていた
        const QList<UsiMove> cycle = {UsiMove::fromString(u"5i5h"), UsiMove::fromString(u"5a5b"),
                                      UsiMove::fromString(u"5h5i"), UsiMove::fromString(u"5b5a")};
        QList<UsiMove> moves;
        for (int i = 0; i < 200000; ++i) moves << cycle.at(i % 4);
        const auto position = UsiPosition::fromMoves(kHirate, moves);
        QVERIFY(position);
        QCOMPARE(position->moves().size(), 200000);
    }
};

QTEST_MAIN(TestUsiPosition)
#include "tst_usiposition.moc"
