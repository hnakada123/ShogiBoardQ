#include <QtTest>
#include <QCoreApplication>

#include "usentosfenconverter.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestUsenConverter : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString& name) const
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

private slots:
    void detectInitialSfen()
    {
        QString sfen = UsenToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_basic.usen")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = UsenToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.usen")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void decodeUsenMoves()
    {
        // Decode the base36-encoded moves from the USEN string
        QString usen = QStringLiteral("~0.7ku2jm6y236e5t24be9qc");
        // Extract move portion (after "~0.")
        QString moveStr = usen.mid(3);
        QStringList decoded = UsenToSfenConverter::decodeUsenMoves(usen);
        QCOMPARE(decoded.size(), 7);
        QCOMPARE(decoded, kExpectedUsiMoves);
    }

    void crossFormat_matchesKif()
    {
        // Verify USEN produces the same USI moves as other formats
        QString error;
        QStringList usenMoves = UsenToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.usen")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(usenMoves, kExpectedUsiMoves);
    }

    void detectInitialSfen_handicap()
    {
        // 駒落ち局面（四枚落ち）: 初期局面が ~ の前にエンコードされている
        QString label;
        QString sfen = UsenToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_handicap.usen")), &label);
        QCOMPARE(label, QStringLiteral("局面指定"));
        QCOMPARE(sfen, QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
    }

    void parseWithVariations_handicap()
    {
        // 駒落ちUSENの本譜をパースし、指し手が正しくデコードされることを確認
        KifParseResult res;
        QString error;
        bool ok = UsenToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("test_handicap.usen")), res, &error);
        QVERIFY2(ok, qPrintable(error));

        // baseSfen が駒落ち局面であること
        QCOMPARE(res.mainline.baseSfen,
                 QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));

        // 11手の指し手 + 投了が含まれること
        QCOMPARE(res.mainline.usiMoves.size(), 11);

        // 先頭の指し手を確認（後手番から開始: △3二銀, ▲7六歩）
        QCOMPARE(res.mainline.usiMoves.at(0), QStringLiteral("3a2b"));
        QCOMPARE(res.mainline.usiMoves.at(1), QStringLiteral("7g7f"));
    }

    // ========================================
    // Error: non-existent file
    // ========================================

    void convertFile_nonExistentFile()
    {
        QString error;
        QStringList moves = UsenToSfenConverter::convertFile(
            fixturePath(QStringLiteral("nonexistent.usen")), &error);
        QVERIFY(moves.isEmpty());
    }

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = UsenToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("nonexistent.usen")), result, &error);
        QVERIFY(!ok);
    }

    void detectInitialSfen_nonExistentFile()
    {
        QString label;
        QString sfen = UsenToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("nonexistent.usen")), &label);
        // Falls back to hirate
        QCOMPARE(sfen, kHirateSfen);
    }

    // ========================================
    // Error: empty file
    // ========================================

    void convertFile_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.usen"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        QStringList moves = UsenToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY(moves.isEmpty());
    }

    // ========================================
    // Error: decodeUsenMoves with empty string
    // ========================================

    void decodeUsenMoves_empty()
    {
        QStringList decoded = UsenToSfenConverter::decodeUsenMoves(QString());
        QCOMPARE(decoded.size(), 0);
    }

    // ========================================
    // Error: decodeUsenMoves with invalid base36
    // ========================================

    void decodeUsenMoves_invalidChars()
    {
        // Characters outside base36 range
        QStringList decoded = UsenToSfenConverter::decodeUsenMoves(QStringLiteral("~0.!!!"));
        // Should handle gracefully without crashing
        Q_UNUSED(decoded);
    }

    // ========================================
    // Error: decodeUsenMoves with truncated data
    // ========================================

    void decodeUsenMoves_truncated()
    {
        // Only 1 or 2 chars (incomplete 3-char move encoding)
        QStringList decoded = UsenToSfenConverter::decodeUsenMoves(QStringLiteral("~0.7k"));
        // Should handle incomplete data gracefully
        Q_UNUSED(decoded);
    }

    // ========================================
    // Normal: terminalCodeToJapanese
    // ========================================

    void terminalCodeToJapanese_resign()
    {
        QCOMPARE(UsenToSfenConverter::terminalCodeToJapanese(QStringLiteral("r")),
                 QStringLiteral("投了"));
    }

    void terminalCodeToJapanese_timeout()
    {
        QCOMPARE(UsenToSfenConverter::terminalCodeToJapanese(QStringLiteral("t")),
                 QStringLiteral("時間切れ"));
    }

    void terminalCodeToJapanese_unknown()
    {
        QString result = UsenToSfenConverter::terminalCodeToJapanese(QStringLiteral("xyz"));
        // Unknown code: implementation may return code itself or empty
        Q_UNUSED(result);
    }

    // ========================================
    // Boundary: extractGameInfo
    // ========================================

    void extractGameInfo_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.usen"));
        QVERIFY(tmp.open());
        tmp.close();

        auto info = UsenToSfenConverter::extractGameInfo(tmp.fileName());
        QVERIFY(info.isEmpty());
    }

    // ========================================
    // Boundary: extractMovesWithTimes on non-existent file
    // ========================================

    void extractMovesWithTimes_nonExistentFile()
    {
        QString error;
        QList<KifDisplayItem> items = UsenToSfenConverter::extractMovesWithTimes(
            fixturePath(QStringLiteral("nonexistent.usen")), &error);
        QVERIFY(items.isEmpty());
    }

    // ========================================
    // Normal: USEN with terminal code
    // ========================================

    void decodeUsenMoves_withTerminal()
    {
        // Test decoding with terminal output parameter
        QString terminal;
        QStringList decoded = UsenToSfenConverter::decodeUsenMoves(
            QStringLiteral("~0.7ku2jm6y236e5t24be9qc.r"), &terminal);
        QCOMPARE(decoded.size(), 7);
        // Terminal code should be "r" (resign)
        QCOMPARE(terminal, QStringLiteral("r"));
    }

    // ========================================
    // Table-driven: abnormal USEN encodings
    // ========================================

    void decodeUsenMoves_abnormal_data()
    {
        QTest::addColumn<QString>("usenInput");

        QTest::newRow("no_tilde_prefix")
            << QStringLiteral("0.7ku2jm");
        QTest::newRow("only_tilde")
            << QStringLiteral("~");
        QTest::newRow("tilde_dot_only")
            << QStringLiteral("~0.");
        QTest::newRow("special_characters")
            << QStringLiteral("~0.@#$%^&*()");
    }

    void decodeUsenMoves_abnormal()
    {
        QFETCH(QString, usenInput);

        // Must not crash regardless of input
        QStringList decoded = UsenToSfenConverter::decodeUsenMoves(usenInput);
        Q_UNUSED(decoded);
        QVERIFY(true);
    }
};

QTEST_MAIN(TestUsenConverter)
#include "tst_usenconverter.moc"
