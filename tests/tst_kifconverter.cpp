#include <QtTest>
#include <QCoreApplication>

#include "kiftosfenconverter.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "kifubranchtree.h"
#include "kifubranchtreebuilder.h"
#include "kifubranchnode.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestKifConverter : public QObject
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
        QString sfen = KifToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("test_basic.kif")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = KifToSfenConverter::convertFile(
            fixturePath(QStringLiteral("test_basic.kif")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void extractMovesWithTimes()
    {
        QString error;
        QList<KifDisplayItem> items = KifToSfenConverter::extractMovesWithTimes(
            fixturePath(QStringLiteral("test_basic.kif")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // Returns 8 items: opening item (ply 0) + 7 moves
        QCOMPARE(items.size(), 8);

        // Second item (index 1) should be first move containing "７六歩"
        QVERIFY(items[1].prettyMove.contains(QStringLiteral("７六歩")));
    }

    void parseWithVariations_basic()
    {
        KifParseResult result;
        QString error;
        bool ok = KifToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("test_basic.kif")), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(result.mainline.usiMoves.size(), 7);
        QCOMPARE(result.variations.size(), 0);
    }

    void parseWithVariations_branch()
    {
        KifParseResult result;
        QString error;
        bool ok = KifToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("test_branch.kif")), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(result.mainline.usiMoves.size(), 7);
        QCOMPARE(result.variations.size(), 2);

        // First variation starts at ply 3
        QCOMPARE(result.variations[0].startPly, 3);
        // Second variation starts at ply 5
        QCOMPARE(result.variations[1].startPly, 5);
    }

    void extractGameInfo()
    {
        auto info = KifToSfenConverter::extractGameInfo(
            fixturePath(QStringLiteral("test_basic.kif")));
        QVERIFY(!info.isEmpty());

        // Should have player names
        bool foundSente = false;
        bool foundGote = false;
        for (const auto& item : std::as_const(info)) {
            if (item.key.contains(QStringLiteral("先手")))
                foundSente = true;
            if (item.key.contains(QStringLiteral("後手")))
                foundGote = true;
        }
        QVERIFY(foundSente);
        QVERIFY(foundGote);
    }

    void comments_extraction()
    {
        QString error;
        QList<KifDisplayItem> items = KifToSfenConverter::extractMovesWithTimes(
            fixturePath(QStringLiteral("test_comments.kif")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        // Check that comments were extracted (at least one non-empty comment)
        bool hasComment = false;
        for (const auto& item : std::as_const(items)) {
            if (!item.comment.isEmpty()) {
                hasComment = true;
                break;
            }
        }
        QVERIFY(hasComment);
    }

    // ========================================
    // Error: non-existent file
    // ========================================

    void convertFile_nonExistentFile()
    {
        QString error;
        QStringList moves = KifToSfenConverter::convertFile(
            fixturePath(QStringLiteral("nonexistent.kif")), &error);
        QVERIFY(moves.isEmpty());
    }

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = KifToSfenConverter::parseWithVariations(
            fixturePath(QStringLiteral("nonexistent.kif")), result, &error);
        QVERIFY(!ok);
    }

    void detectInitialSfen_nonExistentFile()
    {
        QString label;
        QString sfen = KifToSfenConverter::detectInitialSfenFromFile(
            fixturePath(QStringLiteral("nonexistent.kif")), &label);
        // Falls back to hirate
        QCOMPARE(sfen, kHirateSfen);
        QCOMPARE(label, QStringLiteral("平手(既定)"));
    }

    // ========================================
    // Error: empty file
    // ========================================

    void convertFile_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        QStringList moves = KifToSfenConverter::convertFile(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    void extractMovesWithTimes_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        QList<KifDisplayItem> items = KifToSfenConverter::extractMovesWithTimes(
            tmp.fileName(), &error);
        // Should return at least the opening item
        QVERIFY(items.size() <= 1);
    }

    // ========================================
    // Error: header only (no moves)
    // ========================================

    void convertFile_headerOnly()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_header_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write("# ---- Kifu for Windows V7 V7.70 棋譜ファイル ----\n"
                  "先手：テスト先手\n"
                  "後手：テスト後手\n"
                  "手合割：平手\n"
                  "手数----指手---------消費時間--\n");
        tmp.close();

        QString error;
        QStringList moves = KifToSfenConverter::convertFile(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Normal: terminal keyword (投了)
    // ========================================

    void convertFile_terminalResign()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_resign_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write("手合割：平手\n"
                  "手数----指手---------消費時間--\n"
                  "   1 ７六歩(77)   ( 0:01/00:00:01)\n"
                  "   2 ３四歩(33)   ( 0:01/00:00:01)\n"
                  "   3 投了\n");
        tmp.close();

        QString error;
        QStringList moves = KifToSfenConverter::convertFile(tmp.fileName(), &error);
        // Terminal keywords are not included as moves
        QCOMPARE(moves.size(), 2);
    }

    // ========================================
    // Boundary: extractGameInfo from empty file
    // ========================================

    void extractGameInfo_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.close();

        auto info = KifToSfenConverter::extractGameInfo(tmp.fileName());
        QVERIFY(info.isEmpty());
    }

    // ========================================
    // Boundary: extractGameInfoMap
    // ========================================

    void extractGameInfoMap_basic()
    {
        auto m = KifToSfenConverter::extractGameInfoMap(
            fixturePath(QStringLiteral("test_basic.kif")));
        // Should have key-value pairs
        QVERIFY(!m.isEmpty());
    }

    // KIF → parseWithVariations → KifuBranchTreeBuilder → node comments テスト
    void realKif_commentsTransferToBranchTree()
    {
        const QString path = fixturePath(QStringLiteral("test_kiou_comments.kif"));
        if (!QFile::exists(path)) {
            QSKIP("test_kiou_comments.kif not found in fixtures");
        }

        // 1. パース
        KifParseResult result;
        QString error;
        bool ok = KifToSfenConverter::parseWithVariations(path, result, &error);
        QVERIFY2(ok, qPrintable(QStringLiteral("Parse failed: ") + error));
        QVERIFY2(!result.mainline.disp.isEmpty(), "No display items parsed");

        // 2. KifDisplayItem にコメントがあることを確認
        int dispCommentCount = 0;
        for (const auto& item : std::as_const(result.mainline.disp)) {
            if (!item.comment.isEmpty()) {
                ++dispCommentCount;
            }
        }
        qWarning("[TEST] KifDisplayItem comment count: %d / %lld total items",
                 dispCommentCount, static_cast<long long>(result.mainline.disp.size()));
        QVERIFY2(dispCommentCount > 0,
                 "No comments found in KifDisplayItem list");

        // 3. 初期SFENを取得
        QString initialSfen = KifToSfenConverter::detectInitialSfenFromFile(path);
        if (initialSfen.isEmpty()) {
            initialSfen = kHirateSfen;
        }

        // 4. KifuBranchTree を構築
        KifuBranchTree tree;
        KifuBranchTreeBuilder::buildFromKifParseResult(&tree, result, initialSfen);
        QVERIFY2(tree.nodeCount() > 0, "Branch tree is empty after build");

        // 5. ツリーノードのコメントを確認
        QList<BranchLine> lines = tree.allLines();
        QVERIFY2(!lines.isEmpty(), "No branch lines");

        int treeCommentCount = 0;
        const BranchLine& mainLine = lines.at(0);
        for (KifuBranchNode* node : std::as_const(mainLine.nodes)) {
            if (!node->comment().isEmpty()) {
                ++treeCommentCount;
            }
        }
        qWarning("[TEST] KifuBranchNode comment count: %d / %lld main line nodes",
                 treeCommentCount, static_cast<long long>(mainLine.nodes.size()));
        QVERIFY2(treeCommentCount > 0,
                 qPrintable(QStringLiteral(
                     "No comments found in KifuBranchNode tree! "
                     "dispCommentCount=%1 but treeCommentCount=0")
                     .arg(dispCommentCount)));
    }
    // ========================================
    // 異常入力テスト: 不正な駒名
    // ========================================

    void convertFile_invalidPieceName()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badpiece_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write("手合割：平手\n"
                  "手数----指手---------消費時間--\n"
                  "   1 ７六虎(77)   ( 0:01/00:00:01)\n"
                  "   2 ３四歩(33)   ( 0:01/00:00:01)\n");
        tmp.close();

        QString error;
        QStringList moves = KifToSfenConverter::convertFile(tmp.fileName(), &error);
        // Must not crash; invalid piece may be skipped or produce an error
        Q_UNUSED(moves);
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: 不正な座標 (0九, 10一 等)
    // ========================================

    void convertFile_invalidCoordinate()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badcoord_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write("手合割：平手\n"
                  "手数----指手---------消費時間--\n"
                  "   1 ０九歩(77)   ( 0:01/00:00:01)\n"
                  "   2 10一歩(33)   ( 0:01/00:00:01)\n");
        tmp.close();

        QString error;
        QStringList moves = KifToSfenConverter::convertFile(tmp.fileName(), &error);
        // Must not crash; invalid coordinates should be handled gracefully
        Q_UNUSED(moves);
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: 極端に長い行
    // ========================================

    void convertFile_extremelyLongLine()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_longline_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write("手合割：平手\n"
                  "手数----指手---------消費時間--\n");
        // Write a line with 100,000 characters
        QByteArray longLine = "   1 " + QByteArray(100000, 'X') + "\n";
        tmp.write(longLine);
        tmp.close();

        QString error;
        QStringList moves = KifToSfenConverter::convertFile(tmp.fileName(), &error);
        // Must not crash (buffer overflow protection)
        Q_UNUSED(moves);
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: 不正な分岐記述
    // ========================================

    void parseWithVariations_malformedBranch()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badbranch_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write("手合割：平手\n"
                  "手数----指手---------消費時間--\n"
                  "   1 ７六歩(77)   ( 0:01/00:00:01)\n"
                  "   2 ３四歩(33)   ( 0:01/00:00:01)\n"
                  "\n"
                  "変化：99手\n"
                  "  99 ５五角打   ( 0:01/00:00:01)\n");
        tmp.close();

        KifParseResult result;
        QString error;
        // Must not crash even with a branch referencing a non-existent ply
        (void)KifToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: 途中で切れた入力 (EOF)
    // ========================================

    void convertFile_truncatedInput()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_truncated_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write("手合割：平手\n"
                  "手数----指手---------消費時間--\n"
                  "   1 ７六歩(77)   ( 0:01/00:00:01)\n"
                  "   2 ３四歩(33)   ( 0:01/");
        // Intentionally truncated mid-line
        tmp.close();

        QString error;
        QStringList moves = KifToSfenConverter::convertFile(tmp.fileName(), &error);
        // Must not crash even with truncated input
        Q_UNUSED(moves);
        QVERIFY(true);
    }

    // ========================================
    // テーブル駆動: 異常入力パターン
    // ========================================

    void convertFile_abnormalContent_data()
    {
        QTest::addColumn<QByteArray>("fileContent");

        QTest::newRow("only_newlines")
            << QByteArray("\n\n\n\n");
        QTest::newRow("binary_garbage")
            << QByteArray("\x01\x02\x03\x04\x05\x06\x07\x08");
        QTest::newRow("null_bytes")
            << QByteArray("\x00\x00\x00\x00", 4);
        QTest::newRow("repeated_headers")
            << QByteArray("手合割：平手\n手合割：六枚落ち\n手合割：二枚落ち\n");
        QTest::newRow("move_number_overflow")
            << QByteArray("手合割：平手\n手数----指手---------消費時間--\n"
                          "9999999 ７六歩(77)   ( 0:01/00:00:01)\n");
        QTest::newRow("mixed_encoding_attempt")
            << QByteArray("手合割：平手\n"
                          "\x82\xe6\x82\xf1\n"  // Shift_JIS fragment
                          "手数----指手---------消費時間--\n");
    }

    void convertFile_abnormalContent()
    {
        QFETCH(QByteArray, fileContent);

        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_abnormal_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write(fileContent);
        tmp.close();

        QString error;
        // Must not crash regardless of input
        (void)KifToSfenConverter::convertFile(tmp.fileName(), &error);
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: parseWithVariations で空の変化ブロック
    // ========================================

    void parseWithVariations_emptyVariationBlock()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_emptyvar_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.write("手合割：平手\n"
                  "手数----指手---------消費時間--\n"
                  "   1 ７六歩(77)   ( 0:01/00:00:01)\n"
                  "   2 ３四歩(33)   ( 0:01/00:00:01)\n"
                  "\n"
                  "変化：2手\n");
        // Empty variation block (no moves after the header)
        tmp.close();

        KifParseResult result;
        QString error;
        (void)KifToSfenConverter::parseWithVariations(tmp.fileName(), result, &error);
        // Must not crash
        QVERIFY(true);
    }
};

QTEST_MAIN(TestKifConverter)
#include "tst_kifconverter.moc"
