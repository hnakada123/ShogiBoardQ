#include <QtTest>
#include <QCoreApplication>

#include "usitosfenconverter.h"
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

class TestUsiConverter : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString& name) const
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

private slots:
    // ========================================
    // Existing tests
    // ========================================

    void detectInitialSfen()
    {
        QString sfen = UsiToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_basic.usi")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.usi")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    // ========================================
    // Normal: piece move (7g7f)
    // ========================================

    void convertFile_pieceMove()
    {
        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.usi")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(moves.size() >= 1);
        QCOMPARE(moves.at(0), QStringLiteral("7g7f"));
    }

    // ========================================
    // Normal: piece drop (P*5e)
    // ========================================

    void convertFile_pieceDrop()
    {
        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_drops.usi")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // Moves: 7g7f 3c3d 8h2b+ 3a2b P*5e
        QCOMPARE(moves.size(), 5);
        QCOMPARE(moves.at(4), QStringLiteral("P*5e"));
    }

    // ========================================
    // Normal: promotion (8h2b+)
    // ========================================

    void convertFile_promotion()
    {
        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_drops.usi")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(moves.size() >= 3);
        QCOMPARE(moves.at(2), QStringLiteral("8h2b+"));
    }

    // ========================================
    // Normal: non-promotion (8h2b without +)
    // ========================================

    void convertFile_nonPromotion()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_noprom_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write("position startpos moves 7g7f 3c3d 8h2b\n");
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 3);
        QCOMPARE(moves.at(2), QStringLiteral("8h2b"));
    }

    // ========================================
    // Normal: various piece drops
    // ========================================

    void convertFile_variousDrops()
    {
        // Test each drop piece type individually
        struct DropCase {
            const char* moveStr;
            QString expected;
        };
        DropCase cases[] = {
            {"position startpos moves 7g7f 3c3d 8h2b+ 3a2b P*5e", QStringLiteral("P*5e")},
            {"position startpos moves 7g7f 3c3d 8h2b+ 3a2b L*1a", QStringLiteral("L*1a")},
            {"position startpos moves 7g7f 3c3d 8h2b+ 3a2b N*3c", QStringLiteral("N*3c")},
            {"position startpos moves 7g7f 3c3d 8h2b+ 3a2b S*5e", QStringLiteral("S*5e")},
            {"position startpos moves 7g7f 3c3d 8h2b+ 3a2b G*4d", QStringLiteral("G*4d")},
            {"position startpos moves 7g7f 3c3d 8h2b+ 3a2b B*5e", QStringLiteral("B*5e")},
            {"position startpos moves 7g7f 3c3d 8h2b+ 3a2b R*5e", QStringLiteral("R*5e")},
        };

        for (const auto& tc : cases) {
            QTemporaryFile tmp;
            tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_drop_XXXXXX.usi"));
            QVERIFY(tmp.open());
            tmp.write(tc.moveStr);
            tmp.write("\n");
            tmp.close();

            QString error;
            QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
            QVERIFY2(error.isEmpty(), qPrintable(error));
            QVERIFY(moves.size() >= 5);
            QCOMPARE(moves.last(), tc.expected);
        }
    }

    // ========================================
    // Normal: sfen position + moves
    // ========================================

    void convertFile_sfenPosition()
    {
        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_sfen.usi")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 3);
        QCOMPARE(moves.at(0), QStringLiteral("7g7f"));
        QCOMPARE(moves.at(1), QStringLiteral("3c3d"));
        QCOMPARE(moves.at(2), QStringLiteral("2g2f"));
    }

    void detectSfen_sfenPosition()
    {
        QString sfen = UsiToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_sfen.usi")));
        QCOMPARE(sfen, kHirateSfen);
    }

    // ========================================
    // Normal: terminal code (resign)
    // ========================================

    void parseWithVariations_resign()
    {
        KifParseResult result;
        QString error;
        bool ok = UsiToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("test_resign.usi")), result, &error);
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 2);
        QCOMPARE(result.mainline.usiMoves.at(0), QStringLiteral("7g7f"));
        QCOMPARE(result.mainline.usiMoves.at(1), QStringLiteral("3c3d"));
    }

    // ========================================
    // Normal: terminal code to Japanese
    // ========================================

    void terminalCodeToJapanese()
    {
        QCOMPARE(UsiToSfenConverter::terminalCodeToJapanese(QStringLiteral("resign")),
                 QStringLiteral("投了"));
        QCOMPARE(UsiToSfenConverter::terminalCodeToJapanese(QStringLiteral("break")),
                 QStringLiteral("中断"));
        QCOMPARE(UsiToSfenConverter::terminalCodeToJapanese(QStringLiteral("rep_draw")),
                 QStringLiteral("千日手"));
        QCOMPARE(UsiToSfenConverter::terminalCodeToJapanese(QStringLiteral("timeout")),
                 QStringLiteral("時間切れ"));
        QCOMPARE(UsiToSfenConverter::terminalCodeToJapanese(QStringLiteral("win")),
                 QStringLiteral("入玉勝ち"));
    }

    // ========================================
    // Error: non-existent file
    // ========================================

    void convertFile_nonExistentFile()
    {
        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(
            fixturePath(QStringLiteral("nonexistent.usi")), &error);
        QVERIFY(moves.isEmpty());
    }

    // ========================================
    // Error: empty file
    // ========================================

    void convertFile_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY(moves.isEmpty());
    }

    // ========================================
    // Error: short string (only "7g")
    // ========================================

    void convertFile_shortString()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_short_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write("position startpos moves 7g\n");
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        // "7g" is not a valid move, should be skipped
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Error: invalid drop piece (X*5e)
    // ========================================

    void convertFile_invalidDropPiece()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_baddrop_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write("position startpos moves X*5e\n");
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        // "X*5e" has invalid piece, should be skipped
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Error: out-of-range coordinate (0a1b)
    // ========================================

    void convertFile_outOfRange()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_oor_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write("position startpos moves 0a1b\n");
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        // "0a1b" has column 0 which is invalid, should be skipped
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Normal: position keyword omitted
    // ========================================

    void convertFile_noPositionKeyword()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_nopos_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write("startpos moves 7g7f 3c3d\n");
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 2);
        QCOMPARE(moves.at(0), QStringLiteral("7g7f"));
        QCOMPARE(moves.at(1), QStringLiteral("3c3d"));
    }

    // ========================================
    // Error: parseWithVariations on non-existent file
    // ========================================

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = UsiToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("nonexistent.usi")), result, &error);
        QVERIFY(!ok);
    }

    // ========================================
    // Error: only whitespace content
    // ========================================

    void convertFile_whitespaceOnly()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_ws_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write("   \n  \n  \n");
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Error: position keyword only, no moves
    // ========================================

    void convertFile_positionOnly()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_posonly_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write("position startpos\n");
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Boundary: detect SFEN from empty file
    // ========================================

    void detectInitialSfen_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.close();

        QString sfen = UsiToSfenConverter::detectInitialSfenFromFile(tmp.fileName());
        // Should return hirate or empty
        Q_UNUSED(sfen);
    }

    // ========================================
    // Normal: terminal codes
    // ========================================

    void terminalCodeToJapanese_unknown()
    {
        // Unknown code should return the code itself or a fallback
        QString result = UsiToSfenConverter::terminalCodeToJapanese(QStringLiteral("unknown_code"));
        QVERIFY(!result.isEmpty());
    }

    void terminalCodeToJapanese_empty()
    {
        QString result = UsiToSfenConverter::terminalCodeToJapanese(QString());
        // Empty input should not crash
        Q_UNUSED(result);
    }

    // ========================================
    // Boundary: multiple moves on one line
    // ========================================

    void convertFile_manyMoves()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_many_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write("position startpos moves 7g7f 3c3d 2g2f 8c8d 2f2e 8d8e 6i7h 4a3b 2e2d 2c2d 2h2d\n");
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 11);
    }

    // ========================================
    // Boundary: extractGameInfo
    // ========================================

    void extractGameInfo_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.close();

        auto info = UsiToSfenConverter::extractGameInfo(tmp.fileName());
        QVERIFY(info.isEmpty());
    }

    // ========================================
    // Error: extractMovesWithTimes on non-existent file
    // ========================================

    void extractMovesWithTimes_nonExistentFile()
    {
        QString error;
        QList<KifDisplayItem> items = UsiToSfenConverter::extractMovesWithTimes(
            fixturePath(QStringLiteral("nonexistent.usi")), &error);
        QVERIFY(items.isEmpty());
    }

    // ========================================
    // Table-driven: abnormal USI move tokens
    // ========================================

    void convertFile_abnormalMoves_data()
    {
        QTest::addColumn<QByteArray>("fileContent");
        QTest::addColumn<int>("expectedMoveCount");

        QTest::newRow("single_char_move")
            << QByteArray("position startpos moves a\n") << 0;
        QTest::newRow("three_char_move")
            << QByteArray("position startpos moves 7g7\n") << 0;
        QTest::newRow("invalid_rank_char")
            << QByteArray("position startpos moves 7z7f\n") << 0;
        QTest::newRow("double_star_drop")
            << QByteArray("position startpos moves P**5e\n") << 0;
        QTest::newRow("promotion_on_drop")
            << QByteArray("position startpos moves P*5e+\n") << 0;
        QTest::newRow("reversed_coords_format")
            << QByteArray("position startpos moves g7f7\n") << 0;
    }

    void convertFile_abnormalMoves()
    {
        QFETCH(QByteArray, fileContent);
        QFETCH(int, expectedMoveCount);

        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_abnormal_XXXXXX.usi"));
        QVERIFY(tmp.open());
        tmp.write(fileContent);
        tmp.close();

        QString error;
        QStringList moves = UsiToSfenConverter::convertFile(tmp.fileName(), &error);
        QCOMPARE(moves.size(), expectedMoveCount);
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
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, fileContent, QStringLiteral("usi")));

        QString error;
        (void)UsiToSfenConverter::convertFile(tmp.fileName(), &error);

        KifParseResult result;
        (void)UsiToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);

        (void)UsiToSfenConverter::detectInitialSfenFromFile(tmp.fileName());
        (void)UsiToSfenConverter::extractGameInfo(tmp.fileName());
        (void)UsiToSfenConverter::extractMovesWithTimes(tmp.fileName(), &error);

        QVERIFY(true);
    }

    // ========================================
    // 境界値テスト: 極端な手数 (1500手)
    // ========================================

    void longMoveSequence_doesNotCrash()
    {
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(
            tmp, KifuTestHelper::generateLongUsiContent(1500), QStringLiteral("usi")));

        QString error;
        (void)UsiToSfenConverter::convertFile(tmp.fileName(), &error);

        KifParseResult result;
        (void)UsiToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);

        QVERIFY(true);
    }
};

QTEST_MAIN(TestUsiConverter)
#include "tst_usiconverter.moc"
