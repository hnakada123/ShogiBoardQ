#include <QtTest>
#include <QCoreApplication>

#include "kifu_file_helper.h"
#include "ki2tosfenconverter.h"
#include "ki2lexer.h"
#include "ki2movenotation.h"
#include "kifdisplayitem.h"
#include "kifparsetypes.h"
#include "kifu_test_helper.h"
#include "parsecommon.h"
#include "shogiboard.h"

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

    /// 局面の上で KI2 の指し手の文字列を指し手にする（決まらなければ空文字列）
    static QString resolveText(const QString& sfen, const QString& text, BoardSquare previousTo = {})
    {
        ShogiBoard board;
        if (!board.trySetSfen(sfen)) return QStringLiteral("<bad sfen>");
        const auto parsed = Ki2Lexer::parseMoveText(text);
        if (!parsed) return {};
        const auto move = Ki2MoveNotation::resolve(*parsed, board, board.currentPlayer(), previousTo);
        return move ? move->toString() : QString();
    }

private slots:
    void variationsAreSeparateAndNested()
    {
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, QStringLiteral(
            "手合割：平手\n▲７六歩 △３四歩 ▲２六歩\n"
            "変化：2手\n*分岐コメント\n△８四歩\n&分岐しおり\n▲１六歩\n"
            "変化：3手\n▲９六歩\n").toUtf8(), QStringLiteral("ki2")));
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<Ki2ToSfenConverter>(file.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves(), QStringList({"7g7f", "3c3d", "2g2f"}));
        QCOMPARE(result.variations.size(), 2);
        QCOMPARE(result.variations[0].startPly, 2);
        QCOMPARE(result.variations[0].line.usiMoves(), QStringList({"8c8d", "1g1f"}));
        QCOMPARE(result.variations[0].line.displayItems()[0].comment, QStringLiteral("分岐コメント"));
        QCOMPARE(result.variations[0].line.displayItems()[0].bookmark, QStringLiteral("分岐しおり"));
        QCOMPARE(result.variations[1].startPly, 3);
        QCOMPARE(result.variations[1].line.usiMoves(), QStringList({"9g9f"}));
        QCOMPARE(result.variations[1].line.baseSfen, result.variations[0].line.sfenList()[1]);
        QCOMPARE(KifuFile::usiMoves<Ki2ToSfenConverter>(file.fileName()), result.mainline.usiMoves());
        QCOMPARE(KifuFile::displayItems<Ki2ToSfenConverter>(file.fileName()).size(), 4);
    }

    void failedMoveRejectsWholeRecord()
    {
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, QStringLiteral(
            "手合割：平手\n▲７六歩 △５五銀 ▲２六歩\n").toUtf8(), QStringLiteral("ki2")));
        KifParseResult result;
        QString error;
        QVERIFY(!KifuFile::parse<Ki2ToSfenConverter>(file.fileName(), result, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(result.mainline.displayItems().isEmpty());
        QVERIFY(result.mainline.usiMoves().isEmpty());
        QVERIFY(KifuFile::usiMoves<Ki2ToSfenConverter>(file.fileName()).isEmpty());
        QVERIFY(KifuFile::displayItems<Ki2ToSfenConverter>(file.fileName()).isEmpty());
    }

    /// 成駒の名前（成銀・成桂・成香）は成駒の移動。同じマスへ動ける生駒があっても成駒を選ぶ
    void promotedMinorPiece_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<QString>("text");
        // ４六の成駒と、同じ５五へ動ける生駒（６六の銀・６七の桂・５七の香）
        QTest::newRow("silver") << QStringLiteral("8k/9/9/9/9/3S1+S3/9/9/4K4 b - 1") << QStringLiteral("▲５五成銀");
        QTest::newRow("knight") << QStringLiteral("8k/9/9/9/9/5+N3/3N5/9/4K4 b - 1") << QStringLiteral("▲５五成桂");
        QTest::newRow("lance") << QStringLiteral("8k/9/9/9/9/5+L3/4L4/9/4K4 b - 1") << QStringLiteral("▲５五成香");
        QTest::newRow("silver-one-letter") << QStringLiteral("8k/9/9/9/9/3S1+S3/9/9/4K4 b - 1") << QStringLiteral("▲５五全");
        QTest::newRow("knight-one-letter") << QStringLiteral("8k/9/9/9/9/5+N3/3N5/9/4K4 b - 1") << QStringLiteral("▲５五圭");
        QTest::newRow("lance-one-letter") << QStringLiteral("8k/9/9/9/9/5+L3/4L4/9/4K4 b - 1") << QStringLiteral("▲５五杏");
    }

    void promotedMinorPiece()
    {
        QFETCH(QString, sfen);
        QFETCH(QString, text);
        QCOMPARE(resolveText(sfen, text), QStringLiteral("4f5e"));
    }

    /// 表記の揺れ（同・生・行・竜／龍・算用数字の段・半角数字）も同じ手として読む
    void notationVariants_data()
    {
        QTest::addColumn<QString>("sfen");
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("usi");
        const QString silvers = QStringLiteral("k8/9/9/S1S6/9/9/9/9/4K4 b - 1");
        QTest::newRow("promote") << silvers << QStringLiteral("▲８三銀左成") << QStringLiteral("9d8c+");
        QTest::newRow("not-promote") << silvers << QStringLiteral("▲８三銀右不成") << QStringLiteral("7d8c");
        QTest::newRow("nama") << silvers << QStringLiteral("▲８三銀右生") << QStringLiteral("7d8c");
        QTest::newRow("unspecified-is-not-promote") << silvers << QStringLiteral("▲８三銀左") << QStringLiteral("9d8c");
        QTest::newRow("arabic-rank") << silvers << QStringLiteral("▲83銀左成") << QStringLiteral("9d8c+");
        QTest::newRow("zenkaku-rank") << silvers << QStringLiteral("☗８３銀右") << QStringLiteral("7d8c");
        // ８八の飛は８七へ上がり、９七の飛は８七へ寄る
        const QString rooks = QStringLiteral("8k/9/9/9/9/9/R8/1R7/4K4 b - 1");
        QTest::newRow("gyo") << rooks << QStringLiteral("▲８七飛行") << QStringLiteral("8h8g");
        QTest::newRow("yoru") << rooks << QStringLiteral("▲８七飛寄") << QStringLiteral("9g8g");
        QTest::newRow("ryu") << QStringLiteral("8k/4+R4/7+R1/9/9/9/9/9/4K4 b - 1")
                             << QStringLiteral("▲４三竜寄") << QStringLiteral("2c4c");
        // 桂は行き所のない段へは成るしかない（成を書いていなくても成る手）
        QTest::newRow("forced-promotion") << QStringLiteral("8k/9/9/4N4/9/9/9/9/4K4 b - 1")
                                          << QStringLiteral("▲６二桂") << QStringLiteral("5d6b+");
    }

    void notationVariants()
    {
        QFETCH(QString, sfen);
        QFETCH(QString, text);
        QFETCH(QString, usi);
        QCOMPARE(resolveText(sfen, text), usi);
    }

    /// 「同」は直前の手の移動先。「７六同歩」のように移動先と「同」を両方書いても読む
    void sameSquare()
    {
        const QString sfen = QStringLiteral("8k/9/9/9/9/4p4/4P4/9/4K4 b - 1");
        QCOMPARE(resolveText(sfen, QStringLiteral("▲同　歩"), BoardSquare{5, 6}), QStringLiteral("5g5f"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲同歩"), BoardSquare{5, 6}), QStringLiteral("5g5f"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５六同歩"), BoardSquare{5, 6}), QStringLiteral("5g5f"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲同　歩")), QString());  // 直前の手がない
    }

    /// ピンされた駒は動ける駒に数えない。合法な候補が1つなら、修飾語や「打」を省いても読める
    void pinnedPieceIsNotACandidate()
    {
        // ５八の金は５一の飛にピンされている。４七へ動けるのは３八の金だけ
        const QString sfen = QStringLiteral("4r3k/9/9/9/9/9/9/4G1G2/4K4 b G 1");
        QCOMPARE(resolveText(sfen, QStringLiteral("▲４七金")), QStringLiteral("3h4g"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲４七金右")), QStringLiteral("3h4g"));  // 旧版の書き出し
        QCOMPARE(resolveText(sfen, QStringLiteral("▲４七金打")), QStringLiteral("G*4g"));
        // ６七へ動ける盤上の金はピンされた金だけなので、「打」がなくても駒打ち
        QCOMPARE(resolveText(sfen, QStringLiteral("▲６七金")), QStringLiteral("G*6g"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲６七金打")), QStringLiteral("G*6g"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５七金")), QStringLiteral("5h5g"));  // ピンの筋の上は動ける
    }

    /// 合法な候補が2つ以上あるのに修飾語で絞れない手は読まない
    void ambiguousMoveIsRejected()
    {
        const QString sfen = QStringLiteral("8k/9/3GGG3/9/9/9/9/9/4K4 b - 1");
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５二金")), QString());
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５二金上")), QString());
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５二金直")), QStringLiteral("5c5b"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５二金右")), QStringLiteral("4c5b"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５二金左")), QStringLiteral("6c5b"));
    }

    /// 反則手（二歩・王手放置）を書いた棋譜も、駒の動きだけで決めて読む
    void illegalMovesAreReadByPieceMovement()
    {
        QCOMPARE(resolveText(QStringLiteral("4k4/9/9/9/9/9/4P4/9/4K4 b P 1"), QStringLiteral("▲５五歩")),
                 QStringLiteral("P*5e"));  // 二歩
        QCOMPARE(resolveText(QStringLiteral("4k4/9/9/9/9/4P4/9/9/4K4 b P 1"), QStringLiteral("▲５五歩打")),
                 QStringLiteral("P*5e"));  // 二歩（盤上の歩も５五へ動ける）
        QCOMPARE(resolveText(QStringLiteral("4k4/9/9/9/4r4/9/9/9/4K3G b - 1"), QStringLiteral("▲１八金")),
                 QStringLiteral("1i1h"));  // 王手放置
    }

    /// 旧版（盤を文字で持っていた版）が書き出した KI2 も、同じ手順として読む
    void legacyExports_data()
    {
        QTest::addColumn<QString>("file");
        QTest::addColumn<QStringList>("mainline");
        QTest::addColumn<QStringList>("variations");
        QTest::newRow("golds") << QStringLiteral("test_ki2_legacy_golds.ki2")
            << QStringList({"6c5b", "1a2b", "2i2h"})
            << QStringList({"5c5b", "4c5b", "7i8h", "8i8h", "9i8h", "9h8h", "8g8h", "1g2h", "3i2h", "3g2h"});
        QTest::newRow("dragons") << QStringLiteral("test_ki2_legacy_dragons.ki2")
            << QStringList({"5e3e", "9b8a", "9i7g"})
            << QStringList({"1e3e", "5i7g", "9d8c+", "7d8c", "9d8c", "7d8c+"});
        // 旧版はピンされた金も数えて、不要な「右」「打」を付けていた
        QTest::newRow("pins") << QStringLiteral("test_ki2_legacy_pins.ki2")
            << QStringList({"3h4g", "1a2a", "G*6g"})
            << QStringList({"G*4g", "5h5g", "G*6g"});
    }

    void legacyExports()
    {
        QFETCH(QString, file);
        QFETCH(QStringList, mainline);
        QFETCH(QStringList, variations);
        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<Ki2ToSfenConverter>(fixturePath(file), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves(), mainline);
        QStringList firstMoves;
        for (const KifVariation& variation : std::as_const(result.variations)) {
            QCOMPARE(variation.startPly, 1);
            QCOMPARE(variation.line.usiMoves().size(), 1);
            firstMoves << variation.line.usiMoves().first();
        }
        QCOMPARE(firstMoves, variations);
    }

    void detectInitialSfen()
    {
        QString sfen = KifuFile::initialSfen<Ki2ToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.ki2")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = KifuFile::usiMoves<Ki2ToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.ki2")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void extractMovesWithTimes()
    {
        QString error;
        QList<KifDisplayItem> items = KifuFile::displayItems<Ki2ToSfenConverter>(
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
        QStringList moves = KifuFile::usiMoves<Ki2ToSfenConverter>(
            fixturePath(QStringLiteral("nonexistent.ki2")), &error);
        QVERIFY(moves.isEmpty());
    }

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = KifuFile::parse<Ki2ToSfenConverter>(
            fixturePath(QStringLiteral("nonexistent.ki2")), result, &error);
        QVERIFY(!ok);
        QVERIFY(!error.isEmpty());
        QCOMPARE(result.mainline.usiMoves().size(), 0);
    }

    void detectInitialSfen_nonExistentFile()
    {
        QString label;
        QString sfen = KifuFile::initialSfen<Ki2ToSfenConverter>(
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
        QStringList moves = KifuFile::usiMoves<Ki2ToSfenConverter>(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    void extractMovesWithTimes_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.ki2"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        QList<KifDisplayItem> items = KifuFile::displayItems<Ki2ToSfenConverter>(
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
        QStringList moves = KifuFile::usiMoves<Ki2ToSfenConverter>(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Boundary: extractGameInfo
    // ========================================

    void extractGameInfo_basic()
    {
        auto info = KifuFile::gameInfo<Ki2ToSfenConverter>(
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

        auto info = KifuFile::gameInfo<Ki2ToSfenConverter>(tmp.fileName());
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
        (void)KifuFile::usiMoves<Ki2ToSfenConverter>(tmp.fileName(), &error);

        KifParseResult result;
        const bool success = KifuFile::parse<Ki2ToSfenConverter>(tmp.fileName(), result, &error);

        (void)KifuFile::initialSfen<Ki2ToSfenConverter>(tmp.fileName());
        (void)KifuFile::gameInfo<Ki2ToSfenConverter>(tmp.fileName());
        (void)KifuFile::displayItems<Ki2ToSfenConverter>(tmp.fileName(), &error);

        KifuTestHelper::verifyParsedRecord(success, result, error);
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
        (void)KifuFile::usiMoves<Ki2ToSfenConverter>(tmp.fileName(), &error);

        KifParseResult result;
        QVERIFY2(KifuFile::parse<Ki2ToSfenConverter>(tmp.fileName(), result, &error), qPrintable(error));

        QCOMPARE(result.mainline.moveCount(), 1500);
        KifuTestHelper::verifyParsedRecord(true, result, error);
    }
    // テキスト版の入口（貼り付けた棋譜を一時ファイルなしで読む）
    void parseText_matchesFile()
    {
        const QString text = QStringLiteral(
            "手合割：平手\n先手：A\n▲７六歩 △３四歩 ▲２六歩\n変化：2手\n△８四歩\n");
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, text.toUtf8(), QStringLiteral("ki2")));
        KifParseResult fromFile, fromText;
        QString error;
        QVERIFY2(KifuFile::parse<Ki2ToSfenConverter>(file.fileName(), fromFile, &error), qPrintable(error));
        QVERIFY2(Ki2ToSfenConverter::parseText(text, fromText, &error), qPrintable(error));
        QCOMPARE(fromText.mainline.usiMoves(), fromFile.mainline.usiMoves());
        QCOMPARE(fromText.variations.size(), 1);
        QCOMPARE(fromText.variations[0].line.usiMoves(), QStringList({QStringLiteral("8c8d")}));
        QCOMPARE(Ki2ToSfenConverter::extractGameInfoFromText(text).size(), 2);
        QCOMPARE(Ki2ToSfenConverter::detectInitialSfenFromText(text), kHirateSfen);
    }

    // 変化の開始局面は KIF と同じ規則で探す（直前の変化が届かなければ本譜）
    void variationBase_fallsBackToMainline()
    {
        KifParseResult result;
        QString error;
        QVERIFY2(Ki2ToSfenConverter::parseText(QStringLiteral(
            "▲７六歩 △３四歩 ▲２六歩 △８四歩 ▲２五歩\n変化：3手\n▲６六歩\n変化：5手\n▲６八銀\n"),
            result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 2);
        QCOMPARE(result.variations[1].line.baseSfen, result.mainline.sfenList().at(4));
        QCOMPARE(result.variations[1].line.usiMoves(), QStringList({QStringLiteral("7i6h")}));
    }
};

QTEST_MAIN(TestKi2Converter)
#include "tst_ki2converter.moc"
