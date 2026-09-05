/// @file tst_kifu_file_reader.cpp
/// @brief 局面コピーと棋譜貼り付けのフォーマット判定回帰テスト

#include <QtTest>

#include "bodtextgenerator.h"
#include "kifufilereader.h"

class TestKifuFileReader : public QObject
{
    Q_OBJECT

private slots:
    void detectFormat_data()
    {
        using Format = KifuFileReader::KifuFormat;
        QTest::addColumn<QString>("text");
        QTest::addColumn<Format>("expected");

        const QString sfen = QStringLiteral(
            "lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5");
        const QString bod = BodTextGenerator::generate(sfen, 4, QStringLiteral("△８四歩(83)"));
        QTest::newRow("bod-last-move") << bod << Format::BOD;
        QTest::newRow("bod-no-last-move") << BodTextGenerator::generate(sfen, 4, {}) << Format::BOD;
        QTest::newRow("bod-with-comment") << bod + QStringLiteral("\n*候補手は▲２五歩\n") << Format::BOD;
        QTest::newRow("ki2-with-bod") << bod + QStringLiteral("\n  ▲２五歩  △８五歩\n") << Format::KI2;
        QTest::newRow("kif-with-bod") << bod + QStringLiteral("\n手数----指手---------消費時間--\n1 ２五歩(26)\n") << Format::KIF;
        QTest::newRow("kif-without-heading") << bod + QStringLiteral("\n  1 ２五歩(26)\n") << Format::KIF;
        QTest::newRow("ki2-header-comment") << QStringLiteral("先手：テスト\n*△３四歩を検討\n▲７六歩 △３四歩\n") << Format::KI2;
        QTest::newRow("sfen") << sfen << Format::SFEN;
    }

    void detectFormat()
    {
        QFETCH(QString, text);
        QFETCH(KifuFileReader::KifuFormat, expected);
        QCOMPARE(KifuFileReader::detectFormat(text), expected);
    }
};

QTEST_GUILESS_MAIN(TestKifuFileReader)
#include "tst_kifu_file_reader.moc"
