#include <QtTest>
#include <QCoreApplication>

#include "jkftosfenconverter.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestJkfConverter : public QObject
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
        QString sfen = JkfToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_basic.jkf")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.jkf")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void mapPresetToSfen_hirate()
    {
        QString sfen = JkfToSfenConverter::mapPresetToSfen(QStringLiteral("HIRATE"));
        QCOMPARE(sfen, kHirateSfen);
    }

    // ========================================
    // Error: non-existent file
    // ========================================

    void convertFile_nonExistentFile()
    {
        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(
            fixturePath(QStringLiteral("nonexistent.jkf")), &error);
        QVERIFY(moves.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = JkfToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("nonexistent.jkf")), result, &error);
        QVERIFY(!ok);
        QVERIFY(!error.isEmpty());
    }

    void detectInitialSfen_nonExistentFile()
    {
        QString label;
        QString sfen = JkfToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("nonexistent.jkf")), &label);
        // Falls back to hirate
        QCOMPARE(sfen, kHirateSfen);
        QCOMPARE(label, QStringLiteral("平手(既定)"));
    }

    // ========================================
    // Error: empty file (not valid JSON)
    // ========================================

    void convertFile_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY(moves.isEmpty());
        // Empty file is not valid JSON
        QVERIFY(!error.isEmpty());
    }

    // ========================================
    // Error: invalid JSON
    // ========================================

    void convertFile_invalidJson()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badjson_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.write("{invalid json content!!!");
        tmp.close();

        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY(moves.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void parseWithVariations_invalidJson()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badjson_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.write("not json at all");
        tmp.close();

        KifParseResult result;
        QString error;
        bool ok = JkfToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        QVERIFY(!ok);
        QVERIFY(!error.isEmpty());
    }

    // ========================================
    // Error: valid JSON but not an object (array)
    // ========================================

    void convertFile_jsonArray()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_array_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.write("[1, 2, 3]");
        tmp.close();

        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY(moves.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    // ========================================
    // Error: valid JSON object but no "moves" key
    // ========================================

    void convertFile_noMovesKey()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_nomoves_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.write("{\"header\": {\"先手\": \"テスト\"}}");
        tmp.close();

        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY(moves.isEmpty());
        QVERIFY(!error.isEmpty());
    }

    void parseWithVariations_noMovesKey()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_nomoves_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.write("{\"header\": {\"先手\": \"テスト\"}}");
        tmp.close();

        KifParseResult result;
        QString error;
        bool ok = JkfToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        QVERIFY(!ok);
    }

    // ========================================
    // Normal: empty moves array
    // ========================================

    void convertFile_emptyMovesArray()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_emptymoves_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.write("{\"moves\": []}");
        tmp.close();

        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Normal: moves with special (resign)
    // ========================================

    void convertFile_specialResign()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_resign_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.write("{\"moves\": [{}, {\"move\": {\"to\": {\"x\": 7, \"y\": 6}, \"from\": {\"x\": 7, \"y\": 7}, \"piece\": \"FU\", \"color\": 0}}, {\"special\": \"TORYO\"}]}");
        tmp.close();

        QString error;
        QStringList moves = JkfToSfenConverter::convertFile(tmp.fileName(), &error);
        // Only real moves, not the special
        QCOMPARE(moves.size(), 1);
    }

    // ========================================
    // Boundary: extractGameInfo
    // ========================================

    void extractGameInfo_nonExistentFile()
    {
        auto info = JkfToSfenConverter::extractGameInfo(
            fixturePath(QStringLiteral("nonexistent.jkf")));
        QVERIFY(info.isEmpty());
    }

    void extractGameInfo_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.close();

        auto info = JkfToSfenConverter::extractGameInfo(tmp.fileName());
        QVERIFY(info.isEmpty());
    }

    void extractGameInfo_noHeader()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_noheader_XXXXXX.jkf"));
        QVERIFY(tmp.open());
        tmp.write("{\"moves\": []}");
        tmp.close();

        auto info = JkfToSfenConverter::extractGameInfo(tmp.fileName());
        QVERIFY(info.isEmpty());
    }

    // ========================================
    // Normal: mapPresetToSfen for various handicaps
    // ========================================

    void mapPresetToSfen_unknown()
    {
        // Unknown preset should return some fallback
        QString sfen = JkfToSfenConverter::mapPresetToSfen(QStringLiteral("UNKNOWN_PRESET"));
        // Should not crash; may return hirate or empty
        Q_UNUSED(sfen);
    }
};

QTEST_MAIN(TestJkfConverter)
#include "tst_jkfconverter.moc"
