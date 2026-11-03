/// @file tst_position_collection_file.cpp
/// @brief 局面集ファイルの1行の規則（PositionCollectionFile）と最近使ったファイルの一覧（RecentFileList）のテスト

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
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

    /// 行の語の分け方は position コマンドと同じ UsiPosition::split（手数の欄・"moves" は省ける。監査 M4）
    void linesAreSplitLikePositionCommands()
    {
        PositionCollectionFile::Line line;
        const QString board = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4");
        // 手数を省いた SFEN は手数 1
        QCOMPARE(PositionCollectionFile::parseLine(board + QStringLiteral(" b G"), 1, &line),
                 PositionCollectionFile::LineKind::Position);
        QCOMPARE(line.startSfen, board + QStringLiteral(" b G 1"));
        QVERIFY(line.moves.isEmpty());
        // "moves" を省いた手も読む（SFEN だけの行の手は参考手順、position コマンドの行の手は指す）
        QCOMPARE(PositionCollectionFile::parseLine(board + QStringLiteral(" b G 1 5i4h"), 2, &line),
                 PositionCollectionFile::LineKind::Position);
        QCOMPARE(line.referenceMoves(), QStringList{QStringLiteral("5i4h")});
        QCOMPARE(PositionCollectionFile::parseLine(QStringLiteral("position sfen ") + board + QStringLiteral(" b G 5i4h"),
                                                   3, &line),
                 PositionCollectionFile::LineKind::Position);
        QVERIFY(line.movesPlayed);
        QCOMPARE(line.position(), QStringLiteral("4k4/9/9/9/9/9/9/5K3/9 w G 2"));
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

    /// 局面集ファイルの読み方と読めない理由の文は、局面集ビューアと詰将棋問題一覧で同じ
    void readFileReportsReasonsInTheDisplayLanguage()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("problems.sfen"));
        const QByteArray bytes = (QStringLiteral("startpos\r\nsfen ") + kTsume + QStringLiteral("\r\n")).toUtf8();
        {
            QFile out(path);
            QVERIFY(out.open(QIODevice::WriteOnly));
            out.write(bytes);
        }
        const PositionCollectionFile::File file = PositionCollectionFile::readFile(path);
        QVERIFY(file.ok);
        QVERIFY(file.error.isEmpty());
        QCOMPARE(file.bytes, bytes);
        QCOMPARE(PositionCollectionFile::parse(file.text).lines.size(), 2);

        const PositionCollectionFile::File missing = PositionCollectionFile::readFile(dir.filePath(QStringLiteral("none.sfen")));
        QVERIFY(!missing.ok);
        QCOMPARE(missing.error, QStringLiteral("ファイルを開けませんでした: none.sfen"));

        QVERIFY(QDir(dir.path()).mkdir(QStringLiteral("folder.sfen")));
        const PositionCollectionFile::File folder = PositionCollectionFile::readFile(dir.filePath(QStringLiteral("folder.sfen")));
        QVERIFY(!folder.ok);
        QCOMPARE(folder.error, QStringLiteral("通常のファイルではありません: folder.sfen"));
    }

    /// 開くダイアログの絞り込みは、局面集のどの拡張子も出す（ビューアだけ .usi を出さなかった）
    void fileFilterListsEveryCollectionSuffix()
    {
        const QString filter = PositionCollectionFile::fileFilter();
        for (const char* suffix : {"*.txt", "*.sfen", "*.usi"})
            QVERIFY2(filter.contains(QLatin1String(suffix)), suffix);
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
