#include <QtTest>
#include <QCoreApplication>

#include "kifu_file_helper.h"
#include "usentosfenconverter.h"
#include "usencodec.h"
#include "kifparsetypes.h"
#include "terminalcodes.h"
#include "kifu_test_helper.h"

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

    /// 棋譜の "~" より後を読み、本譜の手を USI の文字列にする（読めなければ空）
    static QStringList mainMoves(const QString& usen, QString* terminal = nullptr, QString* error = nullptr)
    {
        const std::optional<QList<UsenLine>> lines = UsenCodec::decodeLines(usen, error);
        if (terminal) *terminal = lines ? lines->constFirst().terminal : QString();
        QStringList moves;
        if (lines) {
            for (const UsiMove& move : lines->constFirst().moves) moves.append(move.toString());
        }
        return moves;
    }

private slots:
    /// 符号表は読み書きで1つ（UsenCodec）。盤上の手・成り・7種の駒打ちが往復すること
    void codecMoveRoundTrip_data()
    {
        QTest::addColumn<QString>("usi");
        for (const char* move : {"7g7f", "1a9i", "9i1a+", "8h2b+", "P*5e", "L*1a", "N*9i", "S*5e", "G*5e", "B*5e", "R*5e"})
            QTest::newRow(move) << QString::fromLatin1(move);
    }
    void codecMoveRoundTrip()
    {
        QFETCH(QString, usi);
        const QString code = UsenCodec::encodeMove(UsiMove::fromString(usi));
        QCOMPARE(code.size(), 3);
        QCOMPARE(UsenCodec::decodeMove(code).value_or(UsiMove{}).toString(), usi);
    }
    void codecRejectsUnknownCodes()
    {
        // 玉は打てない。盤の外のマスは符号にできない
        UsiMove kingDrop;
        kingDrop.drop = Piece::BlackKing;
        kingDrop.to = BoardSquare{5, 5};
        QVERIFY(UsenCodec::encodeMove(kingDrop).isEmpty());
        QVERIFY(UsenCodec::encodeMove(UsiMove{}).isEmpty());
        QVERIFY(!UsenCodec::decodeMove(u"zzz"));
        QVERIFY(!UsenCodec::decodeMove(u"7k"));
    }
    void codecPositionRoundTrip()
    {
        const QString sfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSG+KGSNL w 2P 1");
        const QString usen = UsenCodec::encodePosition(sfen + QStringLiteral(" "));
        QCOMPARE(usen, QStringLiteral("lnsgkgsnl_1r5b1_ppppppppp_9_9_9_PPPPPPPPP_1B5R1_LNSGzKGSNL.w.2P"));
        QCOMPARE(UsenCodec::decodePosition(usen), sfen);
        QVERIFY(UsenCodec::encodePosition(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"))
                    .isEmpty());
    }

    void invalidMovesRejectWholeRecord_data()
    {
        QTest::addColumn<QString>("text");
        QTest::newRow("invalid-main") << QStringLiteral("~0.7kuzzz.r");
        QTest::newRow("truncated-main") << QStringLiteral("~0.7ku2j.r");
        QTest::newRow("invalid-fork") << QStringLiteral("~0.7ku2jm.r~1.zzz.r");
        QTest::newRow("truncated-fork") << QStringLiteral("~0.7ku2jm.r~1.2j.r");
    }

    void invalidMovesRejectWholeRecord()
    {
        QFETCH(QString, text);
        QTemporaryFile file;
        QVERIFY(KifuTestHelper::writeToTempFile(file, text.toUtf8(), QStringLiteral("usen")));
        KifParseResult result;
        QString error;
        QVERIFY(!KifuFile::parse<UsenToSfenConverter>(file.fileName(), result, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(result.mainline.usiMoves().isEmpty());
        QVERIFY(result.variations.isEmpty());
        QVERIFY(KifuFile::usiMoves<UsenToSfenConverter>(file.fileName()).isEmpty());
        const RecordLine line = KifuFile::mainline<UsenToSfenConverter>(file.fileName());
        QVERIFY(line.isEmpty());
        QVERIFY(line.entries.isEmpty());
    }

    void detectInitialSfen()
    {
        QString sfen = KifuFile::initialSfen<UsenToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.usen")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = KifuFile::usiMoves<UsenToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.usen")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void decodeUsenMoves()
    {
        // Decode the base36-encoded moves from the USEN string
        QString usen = QStringLiteral("~0.7ku2jm6y236e5t24be9qc");
        const QStringList decoded = mainMoves(usen);
        QCOMPARE(decoded.size(), 7);
        QCOMPARE(decoded, kExpectedUsiMoves);
    }

    void crossFormat_matchesKif()
    {
        // Verify USEN produces the same USI moves as other formats
        QString error;
        QStringList usenMoves = KifuFile::usiMoves<UsenToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.usen")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(usenMoves, kExpectedUsiMoves);
    }

    void detectInitialSfen_handicap()
    {
        // 駒落ち局面（四枚落ち）: 初期局面が ~ の前にエンコードされている
        StartKind kind = StartKind::Handicap;
        QString sfen = KifuFile::initialSfen<UsenToSfenConverter>(
            fixturePath(QStringLiteral("test_handicap.usen")), &kind);
        QCOMPARE(kind, StartKind::Specified);
        QCOMPARE(sfen, QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));
    }

    void parseWithVariations_handicap()
    {
        // 駒落ちUSENの本譜をパースし、指し手が正しくデコードされることを確認
        KifParseResult res;
        QString error;
        bool ok = KifuFile::parse<UsenToSfenConverter>(
            fixturePath(QStringLiteral("test_handicap.usen")), res, &error);
        QVERIFY2(ok, qPrintable(error));

        // 開始局面が駒落ち局面であること
        QCOMPARE(res.mainline.startSfen,
                 QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));

        // 11手の指し手 + 投了が含まれること
        QCOMPARE(res.mainline.usiMoves().size(), 11);

        // 先頭の指し手を確認（後手番から開始: △3二銀, ▲7六歩）
        QCOMPARE(res.mainline.usiMoves().at(0), QStringLiteral("3a2b"));
        QCOMPARE(res.mainline.usiMoves().at(1), QStringLiteral("7g7f"));
    }

    // ========================================
    // Error: non-existent file
    // ========================================

    void convertFile_nonExistentFile()
    {
        QString error;
        QStringList moves = KifuFile::usiMoves<UsenToSfenConverter>(
            fixturePath(QStringLiteral("nonexistent.usen")), &error);
        QVERIFY(moves.isEmpty());
    }

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = KifuFile::parse<UsenToSfenConverter>(
            fixturePath(QStringLiteral("nonexistent.usen")), result, &error);
        QVERIFY(!ok);
    }

    void detectInitialSfen_nonExistentFile()
    {
        StartKind kind = StartKind::Handicap;
        QString sfen = KifuFile::initialSfen<UsenToSfenConverter>(
            fixturePath(QStringLiteral("nonexistent.usen")), &kind);
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
        QStringList moves = KifuFile::usiMoves<UsenToSfenConverter>(tmp.fileName(), &error);
        QVERIFY(moves.isEmpty());
    }

    // ========================================
    // Error: decodeUsenMoves with empty string
    // ========================================

    void decodeUsenMoves_empty()
    {
        QString error;
        QVERIFY(mainMoves(QString(), nullptr, &error).isEmpty());
        QVERIFY(!error.isEmpty());
    }

    // ========================================
    // Error: decodeUsenMoves with invalid base36
    // ========================================

    void decodeUsenMoves_invalidChars()
    {
        // Characters outside base36 range
        QVERIFY(mainMoves(QStringLiteral("~0.!!!")).isEmpty());
    }

    // ========================================
    // Error: decodeUsenMoves with truncated data
    // ========================================

    void decodeUsenMoves_truncated()
    {
        // Only 1 or 2 chars (incomplete 3-char move encoding)
        QString error;
        QVERIFY(mainMoves(QStringLiteral("~0.7k"), nullptr, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("Incomplete")));
    }

    // ========================================
    // Normal: terminalFromCode
    // ========================================

    void terminalFromCode()
    {
        QCOMPARE(TerminalCodes::fromUsen(QStringLiteral("r")), TerminalType::Resign);
        QCOMPARE(TerminalCodes::fromUsen(QStringLiteral("t")), TerminalType::Timeout);
        QCOMPARE(TerminalCodes::fromUsen(QStringLiteral("xyz")), TerminalType::Error);
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

        auto info = KifuFile::gameInfo<UsenToSfenConverter>(tmp.fileName());
        QVERIFY(info.isEmpty());
    }

    // ========================================
    // Boundary: extractMovesWithTimes on non-existent file
    // ========================================

    void extractMovesWithTimes_nonExistentFile()
    {
        QString error;
        const RecordLine line = KifuFile::mainline<UsenToSfenConverter>(
            fixturePath(QStringLiteral("nonexistent.usen")), &error);
        QVERIFY(line.isEmpty());
        QVERIFY(line.entries.isEmpty());
    }

    // ========================================
    // Normal: USEN with terminal code
    // ========================================

    void decodeUsenMoves_withTerminal()
    {
        // Test decoding with terminal output parameter
        QString terminal;
        const QStringList decoded = mainMoves(QStringLiteral("~0.7ku2jm6y236e5t24be9qc.r"), &terminal);
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
        QString error;
        QVERIFY(mainMoves(usenInput, nullptr, &error).isEmpty());
        // "~0." は手の無い本譜として読める
        if (usenInput != QStringLiteral("~0.")) QVERIFY(!error.isEmpty());
    }
    // ========================================
    // Strict API: 正常入力
    // ========================================

    /// 本譜と変化は "~" で1回だけ分けて読む（オフセットはそのまま、変化の終局コードも読む）
    void decodeLinesSplitsMainAndVariations()
    {
        QString error;
        const auto lines = UsenCodec::decodeLines(QStringLiteral("~0.7ku2jm.r~1.6y2.p"), &error);
        QVERIFY2(lines, qPrintable(error));
        QCOMPARE(lines->size(), 2);
        QCOMPARE(lines->at(0).offset, 0);
        QCOMPARE(lines->at(0).moves.size(), 2);
        QCOMPARE(lines->at(0).terminal, QStringLiteral("r"));
        QCOMPARE(lines->at(1).offset, 1);
        QCOMPARE(lines->at(1).moves.size(), 1);
        QCOMPARE(lines->at(1).terminal, QStringLiteral("p"));
        // 変化の手が読めなければ全体を読まない
        QVERIFY(!UsenCodec::decodeLines(QStringLiteral("~0.7ku~1.!!!"), &error));
        QVERIFY(error.contains(QStringLiteral("Move 1")));
    }

    void decodeUsenMovesStrict_validInput()
    {
        QString error;
        QCOMPARE(mainMoves(QStringLiteral("~0.7ku2jm6y236e5t24be9qc"), nullptr, &error), kExpectedUsiMoves);
        QVERIFY(error.isEmpty());
    }

    // ========================================
    // Strict API: 不正入力でエラー情報が取れる
    // ========================================

    void decodeUsenMovesStrict_invalidInput()
    {
        // 不正なbase36があれば、部分的な指し手列も下流へ渡さない。
        // 続く "7ku" は正常 (7g7f)
        QString error;
        QVERIFY(mainMoves(QStringLiteral("~0.!!!7ku"), nullptr, &error).isEmpty());
        QVERIFY(error.contains(QStringLiteral("Move 1")));
    }

    // ========================================
    // Strict API: 全て不正な入力
    // ========================================

    void decodeUsenMovesStrict_allInvalid()
    {
        QString error;
        QVERIFY(mainMoves(QStringLiteral("~0.!!!@@@"), nullptr, &error).isEmpty());
        // 最初に読めなかった手だけを知らせる
        QVERIFY(error.contains(QStringLiteral("Move 1")));
    }

    // ========================================
    // Strict API: 終局コード付き
    // ========================================

    void decodeUsenMovesStrict_withTerminal()
    {
        QString terminal;
        QString error;
        QCOMPARE(mainMoves(QStringLiteral("~0.7ku2jm.r"), &terminal, &error).size(), 2);
        QVERIFY(error.isEmpty());
        QCOMPARE(terminal, QStringLiteral("r"));
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
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, fileContent, QStringLiteral("usen")));

        QString error;
        (void)KifuFile::usiMoves<UsenToSfenConverter>(tmp.fileName(), &error);

        KifParseResult result;
        const bool success = KifuFile::parse<UsenToSfenConverter>(tmp.fileName(), result, &error);

        (void)KifuFile::initialSfen<UsenToSfenConverter>(tmp.fileName());
        (void)KifuFile::gameInfo<UsenToSfenConverter>(tmp.fileName());
        (void)KifuFile::mainline<UsenToSfenConverter>(tmp.fileName(), &error);

        KifuTestHelper::verifyParsedRecord(success, result, error);
    }

    // ========================================
    // 境界値テスト: 極端なデータ長 (1500トリプレット)
    // ========================================

    void longMoveSequence_doesNotCrash()
    {
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(
            tmp, KifuTestHelper::generateLongUsenContent(1500), QStringLiteral("usen")));

        QString error;
        (void)KifuFile::usiMoves<UsenToSfenConverter>(tmp.fileName(), &error);

        KifParseResult result;
        QVERIFY2(KifuFile::parse<UsenToSfenConverter>(tmp.fileName(), result, &error), qPrintable(error));

        QCOMPARE(result.mainline.moveCount(), 1500);
        KifuTestHelper::verifyParsedRecord(true, result, error);
    }
};

QTEST_MAIN(TestUsenConverter)
#include "tst_usenconverter.moc"
