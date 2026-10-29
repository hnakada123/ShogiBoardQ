#include <QtTest>
#include <QCoreApplication>

#include "kifu_file_helper.h"
#include "kiftextparser.h"
#include "ki2movenotation.h"
#include "kifmovegrammar.h"
#include "kifparsetypes.h"
#include "kifu_test_helper.h"
#include "kifuexportcommon.h"
#include "parsecommon.h"
#include "shogiposition.h"

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
        const std::optional<Position> board = Position::fromSfen(sfen);
        if (!board) return QStringLiteral("<bad sfen>");
        const auto parsed = KifMoveGrammar::parseMoveText(text);
        if (!parsed) return {};
        const auto move = Ki2MoveNotation::resolve(*parsed, *board, board->sideToMove(), previousTo);
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
        QVERIFY2(KifuFile::parse<KifuFormat::Ki2>(file.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves(), QStringList({"7g7f", "3c3d", "2g2f"}));
        QCOMPARE(result.variations.size(), 2);
        QCOMPARE(result.variations[0].startPly, 2);
        QCOMPARE(result.variations[0].line.usiMoves(), QStringList({"8c8d", "1g1f"}));
        // 変化の最初の手の前のコメントと後のしおりは、どちらも最初の手に付く
        QVERIFY(result.variations[0].line.openingComment.isEmpty());
        QCOMPARE(result.variations[0].line.entries[0].comment, QStringLiteral("分岐コメント"));
        QCOMPARE(result.variations[0].line.entries[0].bookmark, QStringLiteral("分岐しおり"));
        QCOMPARE(result.variations[1].startPly, 3);
        QCOMPARE(result.variations[1].line.usiMoves(), QStringList({"9g9f"}));
        QCOMPARE(result.variations[1].parentVariation, 0);
        QCOMPARE(result.variations[1].line.startSfen, result.variations[0].line.sfenAt(1));
        QCOMPARE(KifuFile::usiMoves<KifuFormat::Ki2>(file.fileName()), result.mainline.usiMoves());
        QCOMPARE(KifuFile::mainline<KifuFormat::Ki2>(file.fileName()).entries.size(), 3);
    }

    /// 読めない手があれば、その手の手前までを読み、警告に読んだ手数を書く（KIF・CSA と同じ）
    void failedMoveTruncatesTheRecord()
    {
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, QStringLiteral(
            "手合割：平手\n▲７六歩 △５五玉 ▲２六歩\n").toUtf8(), QStringLiteral("ki2")));
        KifParseResult result;
        QString error;
        QVERIFY(KifuFile::parse<KifuFormat::Ki2>(file.fileName(), result, &error));
        QVERIFY2(error.contains(QStringLiteral("cannot parse move 2")), qPrintable(error));
        QVERIFY2(error.contains(QStringLiteral("read up to move 1")), qPrintable(error));
        QCOMPARE(result.mainline.usiMoves(), QStringList({"7g7f"}));
        QCOMPARE(result.mainline.entries.size(), 1);
    }

    void blockedOriginsNeedUnambiguousNotation()
    {
        const QString sfen = QStringLiteral("4k4/9/9/9/9/9/2P3P2/1B5B1/4K4 b - 1");
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５五角")), QString());
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５五角左")), QStringLiteral("8h5e"));
        QCOMPARE(resolveText(sfen, QStringLiteral("▲５五角右")), QStringLiteral("2h5e"));
        // 存在しない修飾語に合う駒や、王の打ち手を補わない。
        QCOMPARE(resolveText(kHirateSfen, QStringLiteral("▲５五銀右")), QString());
        QCOMPARE(resolveText(kHirateSfen, QStringLiteral("▲５五玉")), QString());
    }

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
        QVERIFY2(KifuFile::parse<KifuFormat::Ki2>(fixturePath(file), result, &error), qPrintable(error));
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
        QString sfen = KifuFile::initialSfen<KifuFormat::Ki2>(
            fixturePath(QStringLiteral("test_basic.ki2")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = KifuFile::usiMoves<KifuFormat::Ki2>(
            fixturePath(QStringLiteral("test_basic.ki2")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void extractMovesWithTimes()
    {
        QString error;
        const RecordLine line = KifuFile::mainline<KifuFormat::Ki2>(
            fixturePath(QStringLiteral("test_basic.ki2")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // 7手（終局の行はない）。表記は手から作り直すので、KI2 でも移動元が付く
        QCOMPARE(line.entries.size(), 7);
        QVERIFY(!line.endsWithTerminal());
        QCOMPARE(line.entries[0].displayText, QStringLiteral("▲７六歩(77)"));
    }

    // ========================================
    // Error: non-existent file
    // ========================================

    void convertFile_nonExistentFile()
    {
        QString error;
        QStringList moves = KifuFile::usiMoves<KifuFormat::Ki2>(
            fixturePath(QStringLiteral("nonexistent.ki2")), &error);
        QVERIFY(moves.isEmpty());
    }

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = KifuFile::parse<KifuFormat::Ki2>(
            fixturePath(QStringLiteral("nonexistent.ki2")), result, &error);
        QVERIFY(!ok);
        QVERIFY(!error.isEmpty());
        QCOMPARE(result.mainline.usiMoves().size(), 0);
    }

    void detectInitialSfen_nonExistentFile()
    {
        StartKind kind = StartKind::Handicap;
        QString sfen = KifuFile::initialSfen<KifuFormat::Ki2>(
            fixturePath(QStringLiteral("nonexistent.ki2")), &kind);
        // Falls back to hirate
        QCOMPARE(sfen,
                 QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        QCOMPARE(kind, StartKind::Default);
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
        QStringList moves = KifuFile::usiMoves<KifuFormat::Ki2>(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    void extractMovesWithTimes_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.ki2"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        const RecordLine line = KifuFile::mainline<KifuFormat::Ki2>(tmp.fileName(), &error);
        // 指し手の行はない
        QVERIFY(line.entries.isEmpty());
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
        QStringList moves = KifuFile::usiMoves<KifuFormat::Ki2>(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    // ========================================
    // Boundary: extractGameInfo
    // ========================================

    void extractGameInfo_basic()
    {
        auto info = KifuFile::gameInfo<KifuFormat::Ki2>(
            fixturePath(QStringLiteral("test_basic.ki2")));
        const auto values = KifuFile::toMap(info);
        QCOMPARE(values.value(QStringLiteral("先手")), QStringLiteral("テスト先手"));
        QCOMPARE(values.value(QStringLiteral("後手")), QStringLiteral("テスト後手"));
    }

    void extractGameInfo_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.ki2"));
        QVERIFY(tmp.open());
        tmp.close();

        auto info = KifuFile::gameInfo<KifuFormat::Ki2>(tmp.fileName());
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
        (void)KifuFile::usiMoves<KifuFormat::Ki2>(tmp.fileName(), &error);

        KifParseResult result;
        const bool success = KifuFile::parse<KifuFormat::Ki2>(tmp.fileName(), result, &error);

        (void)KifuFile::initialSfen<KifuFormat::Ki2>(tmp.fileName());
        (void)KifuFile::gameInfo<KifuFormat::Ki2>(tmp.fileName());
        (void)KifuFile::mainline<KifuFormat::Ki2>(tmp.fileName(), &error);

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
        (void)KifuFile::usiMoves<KifuFormat::Ki2>(tmp.fileName(), &error);

        KifParseResult result;
        QVERIFY2(KifuFile::parse<KifuFormat::Ki2>(tmp.fileName(), result, &error), qPrintable(error));

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
        KifParseResult fromFile;
        QString error;
        QVERIFY2(KifuFile::parse<KifuFormat::Ki2>(file.fileName(), fromFile, &error), qPrintable(error));
        const KifuParseOutcome outcome = KifTextParser::parseKi2(text);
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const KifParseResult& fromText = outcome.record;
        QCOMPARE(fromText.mainline.usiMoves(), fromFile.mainline.usiMoves());
        QCOMPARE(fromText.variations.size(), 1);
        QCOMPARE(fromText.variations[0].line.usiMoves(), QStringList({QStringLiteral("8c8d")}));
        // 対局情報と開始局面も同じ解析で得る
        QCOMPARE(outcome.gameInfo.size(), 2);
        QCOMPARE(outcome.record.mainline.startSfen, kHirateSfen);
        QCOMPARE(fromText.mainline.startSfen, kHirateSfen);
    }

    /// 結果行が名指しした側を終局の行に記録し、書き出すと同じ結果行に戻る
    void resultLine_namedSideRoundTrips_data()
    {
        QTest::addColumn<QString>("resultLine");
        QTest::addColumn<int>("terminal");
        QTest::addColumn<int>("mover");
        // 3手目の後は後手番
        QTest::newRow("先手の反則負け") << QStringLiteral("まで3手で先手の反則負け")
                                         << int(TerminalType::IllegalLoss) << int(Turn::Black);
        QTest::newRow("後手の反則負け") << QStringLiteral("まで3手で後手の反則負け")
                                         << int(TerminalType::IllegalLoss) << int(Turn::White);
        QTest::newRow("先手の反則勝ち") << QStringLiteral("まで3手で先手の反則勝ち")
                                         << int(TerminalType::IllegalWin) << int(Turn::Black);
        QTest::newRow("先手の入玉勝ち") << QStringLiteral("まで3手で先手の入玉勝ち")
                                         << int(TerminalType::DeclarationWin) << int(Turn::Black);
        QTest::newRow("先手の勝ち") << QStringLiteral("まで3手で先手の勝ち")
                                     << int(TerminalType::Resign) << int(Turn::White);
        QTest::newRow("時間切れにより後手の勝ち") << QStringLiteral("まで3手で時間切れにより後手の勝ち")
                                                   << int(TerminalType::Timeout) << int(Turn::Black);
        QTest::newRow("千日手") << QStringLiteral("まで3手で千日手")
                                << int(TerminalType::Repetition) << int(Turn::White);
    }

    void resultLine_namedSideRoundTrips()
    {
        QFETCH(QString, resultLine);
        QFETCH(int, terminal);
        QFETCH(int, mover);
        const KifuParseOutcome outcome = KifTextParser::parseKi2(
            QStringLiteral("手合割：平手\n▲７六歩 △３四歩 ▲２二角成\n") + resultLine + QStringLiteral("\n"));
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const RecordLine& line = outcome.record.mainline;
        QVERIFY(line.endsWithTerminal());
        const RecordEntry& ending = line.entries.last();
        QCOMPARE(int(ending.terminal), terminal);
        QCOMPARE(int(ending.mover), mover);
        QCOMPARE(KifuExportCommon::buildEndingLine(3, ending.terminal, ending.mover, false), resultLine);
    }

    // 変化の開始局面は KIF と同じ規則で探す（直前の変化が届かなければ本譜）
    void variationBase_fallsBackToMainline()
    {
        const KifuParseOutcome outcome = KifTextParser::parseKi2(QStringLiteral(
            "▲７六歩 △３四歩 ▲２六歩 △８四歩 ▲２五歩\n変化：3手\n▲６六歩\n変化：5手\n▲６八銀\n"));
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const KifParseResult& result = outcome.record;
        QCOMPARE(result.variations.size(), 2);
        QCOMPARE(result.variations[1].parentVariation, -1);
        QCOMPARE(result.variations[1].line.startSfen, result.mainline.sfenAt(4));
        QCOMPARE(result.variations[1].line.usiMoves(), QStringList({QStringLiteral("7i6h")}));
    }
};

QTEST_MAIN(TestKi2Converter)
#include "tst_ki2converter.moc"
