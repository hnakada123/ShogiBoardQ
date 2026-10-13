#include <QtTest>
#include <QCoreApplication>

#include "kifdisplayitem.h"
#include "csatosfenconverter.h"
#include "kifparsetypes.h"
#include "kifu_test_helper.h"

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

class TestCsaConverter : public QObject
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

    void parse_basic()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_basic.csa")), result, &warn);
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 7);
        QCOMPARE(result.mainline.usiMoves, kExpectedUsiMoves);
    }

    void extractGameInfo()
    {
        auto info = CsaToSfenConverter::extractGameInfo(
            fixturePath(QStringLiteral("test_basic.csa")));

        bool foundSente = false;
        bool foundGote = false;
        for (const auto& item : std::as_const(info)) {
            if (item.value.contains(QStringLiteral("テスト先手")))
                foundSente = true;
            if (item.value.contains(QStringLiteral("テスト後手")))
                foundGote = true;
        }
        QVERIFY(foundSente);
        QVERIFY(foundGote);
    }

    // ========================================
    // Normal: piece move (+7776FU -> 7g7f)
    // ========================================

    void parse_pieceMove()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_basic.csa")), result, &warn);
        QVERIFY(ok);
        QVERIFY(result.mainline.usiMoves.size() >= 1);
        QCOMPARE(result.mainline.usiMoves.at(0), QStringLiteral("7g7f"));
    }

    // ========================================
    // Normal: piece drop (+0055KA -> B*5e)
    // ========================================

    void parse_pieceDrop()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_drop.csa")), result, &warn);
        QVERIFY(ok);
        // Moves: +7776FU, -3334FU, +8822UM, -3122GI, +0055KA
        QVERIFY(result.mainline.usiMoves.size() >= 5);
        QCOMPARE(result.mainline.usiMoves.at(4), QStringLiteral("B*5e"));
    }

    // ========================================
    // Normal: promotion (+8822UM -> 8h2b+)
    // ========================================

    void parse_promotion()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_promote.csa")), result, &warn);
        QVERIFY(ok);
        // Moves: +7776FU, -3334FU, +8822UM
        QVERIFY(result.mainline.usiMoves.size() >= 3);
        QCOMPARE(result.mainline.usiMoves.at(2), QStringLiteral("8h2b+"));
    }

    // ========================================
    // Normal: non-promotion (knight move without promotion)
    // ========================================

    void parse_nonPromotion()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_various_pieces.csa")), result, &warn);
        QVERIFY(ok);
        // Move 5: +8977KE (knight from 8,9 to 7,7 as KE, no promotion)
        QVERIFY(result.mainline.usiMoves.size() >= 5);
        QCOMPARE(result.mainline.usiMoves.at(4), QStringLiteral("8i7g"));
    }

    // ========================================
    // Normal: various piece moves
    // ========================================

    void parse_variousPieces()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_various_pieces.csa")), result, &warn);
        QVERIFY(ok);
        // +7776FU -> 7g7f (pawn)
        QCOMPARE(result.mainline.usiMoves.at(0), QStringLiteral("7g7f"));
        // -8384FU -> 8c8d (pawn)
        QCOMPARE(result.mainline.usiMoves.at(1), QStringLiteral("8c8d"));
        // +1716FU -> 1g1f (pawn)
        QCOMPARE(result.mainline.usiMoves.at(2), QStringLiteral("1g1f"));
        // -8485FU -> 8d8e (pawn)
        QCOMPARE(result.mainline.usiMoves.at(3), QStringLiteral("8d8e"));
        // +8977KE -> 8i7g (knight)
        QCOMPARE(result.mainline.usiMoves.at(4), QStringLiteral("8i7g"));
        // -4132KI -> 4a3b (gold)
        QCOMPARE(result.mainline.usiMoves.at(5), QStringLiteral("4a3b"));
        // +9998KY -> 9i9h (lance)
        QCOMPARE(result.mainline.usiMoves.at(6), QStringLiteral("9i9h"));
        // -6152KI -> 6a5b (gold)
        QCOMPARE(result.mainline.usiMoves.at(7), QStringLiteral("6a5b"));
        // +2726FU -> 2g2f (pawn)
        QCOMPARE(result.mainline.usiMoves.at(8), QStringLiteral("2g2f"));
        // -5142OU -> 5a4b (king)
        QCOMPARE(result.mainline.usiMoves.at(9), QStringLiteral("5a4b"));
        // +8855KA -> 8h5e (bishop)
        QCOMPARE(result.mainline.usiMoves.at(10), QStringLiteral("8h5e"));
    }

    // ========================================
    // Normal: hirate start position SFEN
    // ========================================

    void parse_hirateStartSfen()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_basic.csa")), result, &warn);
        QVERIFY(ok);
        QCOMPARE(result.mainline.baseSfen,
                 QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
    }

    // ========================================
    // Normal: handicap position (2-piece handicap)
    // ========================================

    void parse_handicapPosition()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("test_handicap_2piece.csa")), result, &warn);
        QVERIFY(ok);
        // 2-piece handicap: no rook/bishop for white, gote moves first
        QCOMPARE(result.mainline.baseSfen,
                 QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
        QVERIFY(result.mainline.usiMoves.size() >= 2);
        QCOMPARE(result.mainline.usiMoves.at(0), QStringLiteral("3c3d"));
        QCOMPARE(result.mainline.usiMoves.at(1), QStringLiteral("7g7f"));
    }

    // ========================================
    // Error: non-existent file
    // ========================================

    void parse_nonExistentFile()
    {
        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(
            fixturePath(QStringLiteral("nonexistent.csa")), result, &warn);
        QVERIFY(!ok);
    }

    // ========================================
    // Error: empty file
    // ========================================

    void parse_emptyFile()
    {
        // Create a temporary empty file
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Should still succeed (parse returns true even with no moves)
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 0);
    }

    // ========================================
    // Error: invalid piece name (+7776XX)
    // ========================================

    void parse_invalidPieceName()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badpiece_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+7776XX\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Parse succeeds but the invalid move is skipped
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 0);
        QVERIFY(!warn.isEmpty());
    }

    // ========================================
    // Error: format error (no +/- prefix)
    // ========================================

    void parse_formatError_noPrefix()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_noprefix_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n7776FU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Line without +/- is ignored (not a move line)
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 0);
    }

    // ========================================
    // Error: short token (too few chars)
    // ========================================

    void parse_shortToken()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_short_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+77FU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 0);
        QVERIFY(!warn.isEmpty());
    }

    // ========================================
    // Error: only comments (no position, no moves)
    // ========================================

    void parse_commentsOnly()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_comment_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("'This is a comment\n'Another comment\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Parser should still handle this (may fail at position parse)
        // Just verify it doesn't crash
        Q_UNUSED(ok);
        Q_UNUSED(warn);
    }

    // ========================================
    // Error: out-of-range coordinate
    // ========================================

    void parse_outOfRangeCoordinate()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_oor_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+0076FU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // CSA lexer accepts column 0 as a drop (from=00); verify parse doesn't crash
        QVERIFY(ok);
    }

    // ========================================
    // Normal: result code (TORYO)
    // ========================================

    void parse_resultCode()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_result_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+7776FU\n-3334FU\n%TORYO\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 2);
        // Display items should include the opening item + 2 moves + result
        QVERIFY(result.mainline.disp.size() >= 4);
    }

    // ========================================
    // Error: mixed valid and invalid moves
    // ========================================

    void parse_mixedValidInvalid()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_mixed_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+7776FU\n+XXYYFU\n-3334FU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        QVERIFY(ok);
        // First move should succeed, second fails, third succeeds
        QVERIFY(result.mainline.usiMoves.size() >= 1);
    }

    // ========================================
    // Boundary: extractGameInfo from non-existent file
    // ========================================

    void extractGameInfo_nonExistentFile()
    {
        auto info = CsaToSfenConverter::extractGameInfo(
            fixturePath(QStringLiteral("nonexistent.csa")));
        QVERIFY(info.isEmpty());
    }

    // ========================================
    // Boundary: extractGameInfo from empty file
    // ========================================

    void extractGameInfo_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.close();

        auto info = CsaToSfenConverter::extractGameInfo(tmp.fileName());
        QVERIFY(info.isEmpty());
    }

    // ========================================
    // Normal: CSA time token with comma-separated line
    // ========================================

    void parse_withTimeTokens_data()
    {
        QTest::addColumn<QByteArray>("separator");
        QTest::newRow("comma") << QByteArray(",");
        QTest::newRow("newline") << QByteArray("\n");
    }

    void parse_withTimeTokens()
    {
        QFETCH(QByteArray, separator);
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_time_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+7776FU" + separator + "T30\n-3334FU" + separator
                  + "T15\n+2726FU" + separator + "T45\n%TORYO" + separator + "T5\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 3);
        QCOMPARE(result.mainline.disp.size(), 5);
        QCOMPARE(result.mainline.disp[1].timeText, QStringLiteral("00:30/00:00:30"));
        QCOMPARE(result.mainline.disp[2].timeText, QStringLiteral("00:15/00:00:15"));
        QCOMPARE(result.mainline.disp[3].timeText, QStringLiteral("00:45/00:01:15"));
        QCOMPARE(result.mainline.disp[4].timeText, QStringLiteral("00:05/00:00:20"));
    }

    // ========================================
    // Error: drop with out-of-range square
    // ========================================

    void parse_dropOutOfRange()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_dropoor_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+0000FU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // CSA lexer treats from=00 as drop; verify parse doesn't crash
        QVERIFY(ok);
    }

    // ========================================
    // Table-driven: abnormal content patterns
    // ========================================

    void parse_abnormalContent_data()
    {
        QTest::addColumn<QByteArray>("fileContent");

        QTest::newRow("only_newlines")
            << QByteArray("\n\n\n\n");
        QTest::newRow("binary_garbage")
            << QByteArray("\x01\x02\x03\x04\x05\x06\x07\x08");
        QTest::newRow("very_long_move_line")
            << (QByteArray("PI\n+\n+") + QByteArray(10000, 'A') + QByteArray("\n"));
        QTest::newRow("move_with_embedded_comment")
            << QByteArray("PI\n+\n+7776FU'comment in move line\n");
        QTest::newRow("repeated_position_blocks")
            << QByteArray("PI\n+\nPI\n+\n+7776FU\n");
        QTest::newRow("empty_lines_between_moves")
            << QByteArray("PI\n+\n\n\n\n+7776FU\n\n\n");
    }

    void parse_abnormalContent()
    {
        QFETCH(QByteArray, fileContent);

        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_abnormal_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write(fileContent);
        tmp.close();

        KifParseResult result;
        QString warn;
        // Must not crash regardless of input
        (void)CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        QVERIFY(true);
    }
    // ========================================
    // 異常入力テスト: 不正なバージョン行
    // ========================================

    void parse_invalidVersion()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badver_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("V999\nPI\n+\n+7776FU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        // Must not crash; unexpected version should be handled gracefully
        (void)CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: 不正な手番指定
    // ========================================

    void parse_invalidTurnSpecifier()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badturn_XXXXXX.csa"));
        QVERIFY(tmp.open());
        // 手番が "X" (不正) の CSA ファイル
        tmp.write("PI\nX\n+7776FU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        (void)CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Must not crash
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: 不正な座標フォーマット
    // ========================================

    void parse_invalidMoveCoordinates()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_badmovecoord_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+9999FU\n-0000FU\n+AABBFU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        (void)CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Must not crash with out-of-range or non-numeric coordinates
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: 未知の特殊結果文字列
    // ========================================

    void parse_unknownSpecialMove()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_unknownspec_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+7776FU\n-3334FU\n%UNKNOWN_RESULT\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Must not crash; unknown special moves should be ignored or handled
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 2);
    }

    // ========================================
    // 異常入力テスト: 各種特殊結果の解析
    // ========================================

    void parse_specialResults_data()
    {
        QTest::addColumn<QByteArray>("specialLine");

        QTest::newRow("KACHI")             << QByteArray("%KACHI\n");
        QTest::newRow("TORYO")             << QByteArray("%TORYO\n");
        QTest::newRow("CHUDAN")            << QByteArray("%CHUDAN\n");
        QTest::newRow("SENNICHITE")        << QByteArray("%SENNICHITE\n");
        QTest::newRow("OUTE_SENNICHITE")   << QByteArray("%OUTE_SENNICHITE\n");
        QTest::newRow("ILLEGAL_MOVE")      << QByteArray("%ILLEGAL_MOVE\n");
        QTest::newRow("TIME_UP")           << QByteArray("%TIME_UP\n");
        QTest::newRow("JISHOGI")           << QByteArray("%JISHOGI\n");
    }

    void parse_specialResults()
    {
        QFETCH(QByteArray, specialLine);

        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_specresult_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+7776FU\n-3334FU\n");
        tmp.write(specialLine);
        tmp.close();

        KifParseResult result;
        QString warn;
        bool ok = CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Must not crash; special results should be handled
        QVERIFY(ok);
        QCOMPARE(result.mainline.usiMoves.size(), 2);
    }

    // ========================================
    // 異常入力テスト: 途中で切れた入力
    // ========================================

    void parse_truncatedCsaInput()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_truncated_XXXXXX.csa"));
        QVERIFY(tmp.open());
        tmp.write("PI\n+\n+77");  // Truncated mid-move
        tmp.close();

        KifParseResult result;
        QString warn;
        (void)CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Must not crash
        QVERIFY(true);
    }

    // ========================================
    // 異常入力テスト: 巨大な座標値
    // ========================================

    void parse_hugeCoordinateValues()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_hugecoord_XXXXXX.csa"));
        QVERIFY(tmp.open());
        // Coordinates exceeding expected single-digit range
        tmp.write("PI\n+\n+99999999FU\n");
        tmp.close();

        KifParseResult result;
        QString warn;
        (void)CsaToSfenConverter::parse(tmp.fileName(), result, &warn);
        // Must not crash
        QVERIFY(true);
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
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, fileContent, QStringLiteral("csa")));

        KifParseResult result;
        QString warn;
        (void)CsaToSfenConverter::parse(tmp.fileName(), result, &warn);

        (void)CsaToSfenConverter::extractGameInfo(tmp.fileName());

        QVERIFY(true);
    }

    // ========================================
    // 境界値テスト: 極端な手数 (1500手)
    // ========================================

    void longMoveSequence_loadsAllMoves()
    {
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(
            tmp, KifuTestHelper::generateLongCsaContent(1500), QStringLiteral("csa")));

        KifParseResult result;
        QString warn;
        QVERIFY(CsaToSfenConverter::parse(tmp.fileName(), result, &warn));
        QCOMPARE(result.mainline.usiMoves.size(), 1500);
        QCOMPARE(result.mainline.usiMoves.last(), QStringLiteral("5b5a"));
        QVERIFY2(warn.isEmpty(), qPrintable(warn));
    }

    // ========================================
    // 開始局面: 盤（ShogiBoard）に組み立てた局面の SFEN
    // ========================================

    void parse_startPosition_data()
    {
        QTest::addColumn<QByteArray>("csa");
        QTest::addColumn<QString>("sfen");

        QTest::newRow("rows-hirate")
            << QByteArray("P1-KY-KE-GI-KI-OU-KI-GI-KE-KY\n"
                          "P2 * -HI *  *  *  *  * -KA * \n"
                          "P3-FU-FU-FU-FU-FU-FU-FU-FU-FU\n"
                          "P4 *  *  *  *  *  *  *  *  * \n"
                          "P5 *  *  *  *  *  *  *  *  * \n"
                          "P6 *  *  *  *  *  *  *  *  * \n"
                          "P7+FU+FU+FU+FU+FU+FU+FU+FU+FU\n"
                          "P8 * +KA *  *  *  *  * +HI * \n"
                          "P9+KY+KE+GI+KI+OU+KI+GI+KE+KY\n+\n")
            << QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        // 行の上書き（成駒・空きマス）と P+/P- の持駒
        QTest::newRow("rows-promoted-and-hands")
            << QByteArray("PI\nP3-FU-FU-FU-FU-FU-FU+TO-FU-FU\nP7+FU+FU+FU+FU+FU+FU *  * +FU\n"
                          "P+00FU00KI\nP-00KA\n-\n")
            << QStringLiteral("lnsgkgsnl/1r5b1/pppppp+Ppp/9/9/9/PPPPPP2P/1B5R1/LNSGKGSNL w GPb 1");
        QTest::newRow("PI-six-piece-handicap")
            << QByteArray("PI82HI22KA11KY91KY21KE81KE\n-\n")
            << QStringLiteral("2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        // 持駒の成駒は生駒にし、玉は持駒にしない
        QTest::newRow("hand-promoted-and-king")
            << QByteArray("P-51OU\nP+59OU00TO00UM00OU\n+\n")
            << QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 b BP 1");
        // 00AL は盤上と両者の持駒にない駒（玉を除く）を、指定した側の持駒にする
        QTest::newRow("AL-white-after-board-and-hands")
            << QByteArray("P-11OU\nP+22KI\nP+00KI00FU\nP-00AL\n+\n")
            << QStringLiteral("8k/7G1/9/9/9/9/9/9/9 b GP2r2b2g4s4n4l17p 1");
        QTest::newRow("AL-black")
            << QByteArray("P-51OU\nP+00AL\nP-00HI\n+\n")
            << QStringLiteral("4k4/9/9/9/9/9/9/9/9 b R2B4G4S4N4L18Pr 1");
    }

    void parse_startPosition()
    {
        QFETCH(QByteArray, csa);
        QFETCH(QString, sfen);

        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, csa, QStringLiteral("csa")));
        KifParseResult result;
        QString warn;
        QVERIFY(CsaToSfenConverter::parse(tmp.fileName(), result, &warn));
        QCOMPARE(result.mainline.baseSfen, sfen);
    }

    // ========================================
    // 指し手: 盤の上で進め、取った駒を打つ・成駒を動かす・「同」を書く
    // ========================================

    void parse_movesFollowBoard_data()
    {
        QTest::addColumn<QByteArray>("csa");
        QTest::addColumn<QStringList>("usi");
        QTest::addColumn<QStringList>("pretty");

        QTest::newRow("capture-and-drop")
            << QByteArray("PI\n+\n+7776FU\n-3334FU\n+8822UM\n-3122GI\n+0055KA\n-4132KI\n"
                          "+5533UM\n-2233GI\n+2726FU\n-0045KA\n")
            << QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("8h2b+"),
                           QStringLiteral("3a2b"), QStringLiteral("B*5e"), QStringLiteral("4a3b"),
                           QStringLiteral("5e3c+"), QStringLiteral("2b3c"), QStringLiteral("2g2f"),
                           QStringLiteral("B*4e")}
            << QStringList{QStringLiteral("▲７六歩(77)"), QStringLiteral("△３四歩(33)"),
                           QStringLiteral("▲２二角成(88)"), QStringLiteral("△同　銀(31)"),
                           QStringLiteral("▲５五角打"), QStringLiteral("△３二金(41)"),
                           QStringLiteral("▲３三角成(55)"), QStringLiteral("△同　銀(22)"),
                           QStringLiteral("▲２六歩(27)"), QStringLiteral("△４五角打")};
        QTest::newRow("promoted-piece-moves")
            << QByteArray("PI\n+\n+7776FU\n-3334FU\n+8833UM\n-4132KI\n+3322UM\n")
            << QStringList{QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("8h3c+"),
                           QStringLiteral("4a3b"), QStringLiteral("3c2b")}
            << QStringList{QStringLiteral("▲７六歩(77)"), QStringLiteral("△３四歩(33)"),
                           QStringLiteral("▲３三角成(88)"), QStringLiteral("△３二金(41)"),
                           QStringLiteral("▲２二馬(33)")};
    }

    void parse_movesFollowBoard()
    {
        QFETCH(QByteArray, csa);
        QFETCH(QStringList, usi);
        QFETCH(QStringList, pretty);

        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, csa, QStringLiteral("csa")));
        KifParseResult result;
        QString warn;
        QVERIFY(CsaToSfenConverter::parse(tmp.fileName(), result, &warn));
        QVERIFY2(warn.isEmpty(), qPrintable(warn));
        QCOMPARE(result.mainline.usiMoves, usi);
        QStringList texts;
        for (qsizetype i = 1; i < result.mainline.disp.size(); ++i) texts.append(result.mainline.disp.at(i).prettyMove);
        QCOMPARE(texts, pretty);
    }

    // ========================================
    // 局面と合わない指し手は読まない
    // ========================================

    void parse_rejectsMoveNotFittingBoard_data()
    {
        QTest::addColumn<QByteArray>("move");

        QTest::newRow("wrong-piece") << QByteArray("+7776KY");
        QTest::newRow("empty-source") << QByteArray("+5554FU");
        QTest::newRow("opponent-piece") << QByteArray("+3334FU");
        QTest::newRow("drop-not-in-hand") << QByteArray("+0055KA");
        QTest::newRow("capture-own-piece") << QByteArray("+7969GI");
        QTest::newRow("wrong-promoted-piece") << QByteArray("+2726NY");
    }

    void parse_rejectsMoveNotFittingBoard()
    {
        QFETCH(QByteArray, move);

        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, "PI\n+\n" + move + "\n", QStringLiteral("csa")));
        KifParseResult result;
        QString warn;
        QVERIFY(CsaToSfenConverter::parse(tmp.fileName(), result, &warn));
        QVERIFY(result.mainline.usiMoves.isEmpty());
        QVERIFY2(warn.contains(QStringLiteral("does not match the position")), qPrintable(warn));
    }
};

QTEST_MAIN(TestCsaConverter)
#include "tst_csaconverter.moc"
