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
        QTest::newRow("usi-short") << QStringLiteral("startpos moves 7g7f") << Format::USI;
        QTest::newRow("usi-sfen") << QStringLiteral("sfen ") + sfen + QStringLiteral(" moves 2g2f") << Format::USI;
        QTest::newRow("sfen-no-ply") << sfen.left(sfen.lastIndexOf(QLatin1Char(' '))) << Format::USI;
        QTest::newRow("usi-comment") << QStringLiteral("# comment\nstartpos moves 7g7f") << Format::USI;
        QTest::newRow("json-array") << QStringLiteral("[{\"move\":{}}]") << Format::JKF;
        QTest::newRow("usi-position") << QStringLiteral("position startpos moves 7g7f 3c3d") << Format::USI;
        QTest::newRow("jkf-object") << QStringLiteral("{\"header\":{},\"moves\":[{}]}") << Format::JKF;
        QTest::newRow("csa-header") << QStringLiteral("V2.2\nPI\n+\n+7776FU\nT1") << Format::CSA;
        QTest::newRow("csa-moves") << QStringLiteral("+7776FU\n-3334FU") << Format::CSA;
        QTest::newRow("usen-custom-position") << QStringLiteral("1nsgkgsn1_9_ppppppppp_9_9_9_PPPPPPPPP_1B5R1_LNSGKGSNL.w.-~0.09k7ku.r") << Format::USEN;
        QTest::newRow("usen") << QStringLiteral("~0.7ku.r~1.0e4.r") << Format::USEN;
        QTest::newRow("kif-tilde") << QStringLiteral("*https://example.org/~user\n手数----指手---------\n1 ７六歩(77)") << Format::KIF;
        QTest::newRow("csa-tilde") << QStringLiteral("'comment ~\nPI\n+\n+7776FU") << Format::CSA;
    }

    void detectFormat()
    {
        QFETCH(QString, text);
        QFETCH(KifuFileReader::KifuFormat, expected);
        QCOMPARE(KifuFileReader::detectFormat(text), expected);
    }

    void temporaryFilesAreUniqueAndRemoved()
    {
        QString path1, path2;
        {
            auto first = KifuFileReader::createTempFile(KifuFileReader::KifuFormat::KIF, QStringLiteral("先手：一人目"));
            auto second = KifuFileReader::createTempFile(KifuFileReader::KifuFormat::KIF, QStringLiteral("先手：二人目"));
            QVERIFY(first);
            QVERIFY(second);
            path1 = first->fileName();
            path2 = second->fileName();
            QVERIFY(path1 != path2);
            QFile file1(path1), file2(path2);
            QVERIFY(file1.open(QIODevice::ReadOnly));
            QVERIFY(file2.open(QIODevice::ReadOnly));
            QCOMPARE(QString::fromUtf8(file1.readAll()), QStringLiteral("先手：一人目"));
            QCOMPARE(QString::fromUtf8(file2.readAll()), QStringLiteral("先手：二人目"));
        }
        QVERIFY(!QFile::exists(path1));
        QVERIFY(!QFile::exists(path2));
    }
};

QTEST_GUILESS_MAIN(TestKifuFileReader)
#include "tst_kifu_file_reader.moc"
