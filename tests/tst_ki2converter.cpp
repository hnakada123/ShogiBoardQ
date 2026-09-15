#include <QtTest>
#include <QCoreApplication>

#include "ki2tosfenconverter.h"
#include "ki2lexer.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "kifu_test_helper.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestKi2Converter : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString& name) const
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

private slots:
    void promotedMinorPiece_data()
    {
        QTest::addColumn<QString>("name");
        QTest::addColumn<QString>("piece");
        QTest::addColumn<int>("plainFile");
        QTest::addColumn<int>("plainRank");
        QTest::newRow("silver") << QStringLiteral("成銀") << QStringLiteral("S") << 6 << 6;
        QTest::newRow("knight") << QStringLiteral("成桂") << QStringLiteral("N") << 6 << 7;
        QTest::newRow("lance") << QStringLiteral("成香") << QStringLiteral("L") << 5 << 7;
    }

    void promotedMinorPiece()
    {
        QFETCH(QString, name);
        QFETCH(QString, piece);
        QFETCH(int, plainFile);
        QFETCH(int, plainRank);

        QString board[9][9];
        board[5][5] = QStringLiteral("+") + piece; // ４六の成駒
        QMap<Piece, int> blackHands, whiteHands;
        int prevFile = 0, prevRank = 0;
        const QString move = QStringLiteral("▲５五") + name;
        QCOMPARE(Ki2Lexer::convertKi2MoveToUsi(move, board, blackHands, whiteHands,
                                             true, prevFile, prevRank), QStringLiteral("4f5e"));

        // 同じ移動先へ動ける生駒があっても、成駒を選ぶ。
        board[4][4].clear();
        board[5][5] = QStringLiteral("+") + piece;
        board[plainRank - 1][9 - plainFile] = piece;
        QCOMPARE(Ki2Lexer::convertKi2MoveToUsi(move, board, blackHands, whiteHands,
                                             true, prevFile, prevRank), QStringLiteral("4f5e"));
    }

    void detectInitialSfen()
    {
        QString sfen = Ki2ToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_basic.ki2")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = Ki2ToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.ki2")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void extractMovesWithTimes()
    {
        QString error;
        QList<KifDisplayItem> items = Ki2ToSfenConverter::extractMovesWithTimes(
            fixturePath(QStringLiteral("test_basic.ki2")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // Returns opening item (ply 0) + 7 moves = 8
        QCOMPARE(items.size(), 8);
    }

    // ========================================
    // Error: non-existent file
    // ========================================

    void convertFile_nonExistentFile()
    {
        QString error;
        QStringList moves = Ki2ToSfenConverter::convertFile(
            fixturePath(QStringLiteral("nonexistent.ki2")), &error);
        QVERIFY(moves.isEmpty());
    }

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = Ki2ToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("nonexistent.ki2")), result, &error);
        // KI2 converter reads file internally; non-existent file returns ok=true with 0 moves
        Q_UNUSED(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 0);
    }

    void detectInitialSfen_nonExistentFile()
    {
        QString label;
        QString sfen = Ki2ToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("nonexistent.ki2")), &label);
        // Falls back to hirate
        QCOMPARE(sfen,
                 QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        QCOMPARE(label, QStringLiteral("平手(既定)"));
    }

    // ========================================
    // Error: empty file
    // ========================================

    void convertFile_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.ki2"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        QStringList moves = Ki2ToSfenConverter::convertFile(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    void extractMovesWithTimes_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.ki2"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        QList<KifDisplayItem> items = Ki2ToSfenConverter::extractMovesWithTimes(
            tmp.fileName(), &error);
        // Should return at most the opening item
        QVERIFY(items.size() <= 1);
    }

    // ========================================
    // Error: header only (no moves)
    // ========================================

    void convertFile_headerOnly()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_header_XXXXXX.ki2"));
        QVERIFY(tmp.open());
        tmp.write("先手：テスト先手\n"
                  "後手：テスト後手\n"
                  "手合割：平手\n");
        tmp.close();

        QString error;
        QStringList moves = Ki2ToSfenConverter::convertFile(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Boundary: extractGameInfo
    // ========================================

    void extractGameInfo_basic()
    {
        auto info = Ki2ToSfenConverter::extractGameInfo(
            fixturePath(QStringLiteral("test_basic.ki2")));
        const auto values = KifuParseCommon::toGameInfoMap(info);
        QCOMPARE(values.value(QStringLiteral("先手")), QStringLiteral("テスト先手"));
        QCOMPARE(values.value(QStringLiteral("後手")), QStringLiteral("テスト後手"));
    }

    void extractGameInfo_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.ki2"));
        QVERIFY(tmp.open());
        tmp.close();

        auto info = Ki2ToSfenConverter::extractGameInfo(tmp.fileName());
        QVERIFY(info.isEmpty());
    }
    // ========================================
    // 境界値テスト: 全APIに対する異常入力耐性
    // ========================================

    void boundaryInput_doesNotCrash_data()
    {
        QTest::addColumn<QByteArray>("fileContent");
        KifuTestHelper::addBoundaryInputRows();
    }

    void boundaryInput_doesNotCrash()
    {
        QFETCH(QByteArray, fileContent);

        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, fileContent, QStringLiteral("ki2")));

        QString error;
        (void)Ki2ToSfenConverter::convertFile(tmp.fileName(), &error);

        KifParseResult result;
        (void)Ki2ToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);

        (void)Ki2ToSfenConverter::detectInitialSfenFromFile(tmp.fileName());
        (void)Ki2ToSfenConverter::extractGameInfo(tmp.fileName());
        (void)Ki2ToSfenConverter::extractMovesWithTimes(tmp.fileName(), &error);

        QVERIFY(true);
    }

    // ========================================
    // 境界値テスト: 極端な手数 (1500手)
    // ========================================

    void longMoveSequence_doesNotCrash()
    {
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(
            tmp, KifuTestHelper::generateLongKi2Content(1500), QStringLiteral("ki2")));

        QString error;
        (void)Ki2ToSfenConverter::convertFile(tmp.fileName(), &error);

        KifParseResult result;
        (void)Ki2ToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);

        QVERIFY(true);
    }
};

QTEST_MAIN(TestKi2Converter)
#include "tst_ki2converter.moc"
