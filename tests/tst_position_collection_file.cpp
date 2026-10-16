/// @file tst_position_collection_file.cpp
/// @brief 局面集ファイルの1行の規則（PositionCollectionFile）と最近使ったファイルの一覧（RecentFileList）のテスト

#include <QtTest>

#include "positioncollectionfile.h"
#include "recentfilelist.h"
#include "sfenutils.h"
#include "tsumecollection.h"

namespace {
const QString kHirate = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
const QString kTsume = QStringLiteral("9/9/6R1+R/5k3/9/7+S1/9/9/9 b 2b4g3s4n4l18p 1");
} // namespace

class TestPositionCollectionFile : public QObject
{
    Q_OBJECT

private slots:
    void skipsBlankAndCommentLines()
    {
        using PositionCollectionFile::LineKind;
        QCOMPARE(PositionCollectionFile::parseLine(QString(), 1, nullptr), LineKind::Skipped);
        QCOMPARE(PositionCollectionFile::parseLine(QStringLiteral("  \r"), 1, nullptr), LineKind::Skipped);
        QCOMPARE(PositionCollectionFile::parseLine(QStringLiteral("# 詰将棋"), 1, nullptr), LineKind::Skipped);
        QCOMPARE(PositionCollectionFile::parseLine(QChar(0xfeff) + QStringLiteral("#x"), 1, nullptr),
                 LineKind::Skipped);
    }

    void positionCommandMovesArePlayed()
    {
        PositionCollectionFile::Line line;
        QCOMPARE(PositionCollectionFile::parseLine(QStringLiteral("position startpos moves 7g7f 3c3d"), 4, &line),
                 PositionCollectionFile::LineKind::Position);
        QCOMPARE(line.lineNumber, 4);
        QCOMPARE(line.startSfen, SfenUtils::hirateSfen());
        QVERIFY(line.movesPlayed);
        QVERIFY(line.referenceMoves().isEmpty());
        QCOMPARE(line.position(),
                 QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"));
    }

    void sfenMovesAreReference()
    {
        PositionCollectionFile::Line line;
        QCOMPARE(PositionCollectionFile::parseLine(QStringLiteral("sfen\t") + kHirate + QStringLiteral(" moves 2g2f"),
                                                   1, &line),
                 PositionCollectionFile::LineKind::Position);
        QVERIFY(!line.movesPlayed);
        QCOMPARE(line.referenceMoves(), QStringList{QStringLiteral("2g2f")});
        QCOMPARE(line.position(), kHirate);
    }

    void rejectsMalformedLines_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("short") << QStringLiteral("lnsgkgsnl/1r5b1 b -");
        QTest::newRow("garbage") << kHirate + QStringLiteral(" garbage");
        QTest::newRow("not a move") << kHirate + QStringLiteral(" moves resign");
        QTest::newRow("bad turn") << QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL x - 1");
        QTest::newRow("bad pieces") << QStringLiteral("abc def ghi jkl");
    }

    void rejectsMalformedLines()
    {
        QFETCH(QString, text);
        QCOMPARE(PositionCollectionFile::parseLine(text, 1, nullptr), PositionCollectionFile::LineKind::Invalid);
    }

    void parseKeepsOrderAndInvalidLineNumbers()
    {
        const auto contents = PositionCollectionFile::parse(
            QStringLiteral("# header\n") + kHirate + QStringLiteral("\ninvalid\n\nposition startpos\n"));
        QCOMPARE(contents.lines.size(), 2);
        QCOMPARE(contents.lines.at(0).lineNumber, 2);
        QCOMPARE(contents.lines.at(1).lineNumber, 5);
        QCOMPARE(contents.invalidLines, QList<int>{3});
    }

    /// 局面集ビューアと同じ規則で読むので、詰将棋問題一覧も position コマンドの指し手は指した後の局面にする
    void tsumeCollectionUsesSameRules()
    {
        const auto result = TsumeCollection::parse(
            kTsume + QStringLiteral(" moves 3c5c+ 4d4e 1c4c P*4d 4c4d\nposition sfen ") + kTsume
            + QStringLiteral("\n"));
        QCOMPARE(result.problems.size(), 2);
        QCOMPARE(result.problems.at(0).referenceMoves.size(), 5);
        QCOMPARE(result.problems.at(1).sfen, kTsume);
        QVERIFY(result.problems.at(1).referenceMoves.isEmpty());
    }

    void recentFileListOrderAndLimit()
    {
        RecentFileList list(3);
        list.setFiles({QStringLiteral("a"), QStringLiteral("a"), QString(), QStringLiteral("b"),
                       QStringLiteral("c"), QStringLiteral("d")});
        QCOMPARE(list.files(), QStringList({QStringLiteral("a"), QStringLiteral("b"), QStringLiteral("c")}));
        list.add(QStringLiteral("c"));
        QCOMPARE(list.files(), QStringList({QStringLiteral("c"), QStringLiteral("a"), QStringLiteral("b")}));
        list.add(QStringLiteral("e"));
        QCOMPARE(list.files(), QStringList({QStringLiteral("e"), QStringLiteral("c"), QStringLiteral("a")}));
        QVERIFY(list.remove(QStringLiteral("c")));
        QVERIFY(!list.remove(QStringLiteral("zzz")));
        list.clear();
        QVERIFY(list.isEmpty());
    }
};

QTEST_MAIN(TestPositionCollectionFile)
#include "tst_position_collection_file.moc"
