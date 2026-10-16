#include <QtTest>
#include <QCoreApplication>

#include "kifu_file_helper.h"
#include "kiftosfenconverter.h"
#include "kifparsetypes.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifubranchtreebuilder.h"
#include "kifubranchnode.h"
#include "kifu_test_helper.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

static const QStringList kExpectedUsiMoves = {
    QStringLiteral("7g7f"), QStringLiteral("3c3d"), QStringLiteral("2g2f"),
    QStringLiteral("8c8d"), QStringLiteral("2f2e"), QStringLiteral("8d8e"),
    QStringLiteral("6i7h")
};

/// 表記の並びと局面列（[0] が開始局面、[i] が i 手目の後）から、startPly 手目から始まる手順を作る
static RecordLine testLine(const QStringList& texts, const QStringList& sfens, int startPly = 1)
{
    RecordLine line;
    line.startSfen = sfens.first();
    line.startPly = startPly;
    for (qsizetype i = 0; i < texts.size(); ++i) {
        const QString& text = texts.at(i);
        const QString sfen = sfens.value(i + 1, sfens.last());
        // テスト用の手順は表記だけで書くので、終局語の行を終局にする
        const TerminalType terminal = TerminalTypes::findWord(text);
        const Turn side = text.startsWith(QStringLiteral("△")) ? Turn::White : Turn::Black;
        RecordEntry entry = terminal != TerminalType::None
            ? RecordEntry::ending(terminal, side)
            : RecordEntry::played({}, text, sfen);
        entry.displayText = text;
        entry.sfen = sfen;
        line.entries.append(entry);
    }
    return line;
}

class TestKifConverter : public QObject
{
    Q_OBJECT

private:
    QString fixturePath(const QString& name) const
    {
        return QCoreApplication::applicationDirPath() + QStringLiteral("/fixtures/") + name;
    }

private slots:
    void invalidVariationDoesNotReplaceExistingRecord_data()
    {
        QTest::addColumn<int>("parent");
        QTest::addColumn<int>("startPly");
        QTest::newRow("self-parent") << 0 << 1;
        QTest::newRow("unknown-parent") << 5 << 1;
        QTest::newRow("negative-parent") << -2 << 1;
        QTest::newRow("zero-ply") << -1 << 0;
        QTest::newRow("past-end") << -1 << 9;
    }
    void invalidVariationDoesNotReplaceExistingRecord()
    {
        QFETCH(int, parent);
        QFETCH(int, startPly);
        KifuBranchTree tree;
        tree.setRootSfen(QStringLiteral("original"));
        const int rootId = tree.root()->nodeId();
        QSignalSpy changed(&tree, &KifuBranchTree::reset);
        KifParseResult result;
        KifVariation variation;
        variation.parentVariation = parent;
        variation.startPly = startPly;
        result.variations.append(variation);
        QVERIFY(!KifuBranchTreeBuilder::buildFromKifParseResult(&tree, result, QStringLiteral("replacement")));
        QCOMPARE(tree.root()->nodeId(), rootId);
        QCOMPARE(tree.root()->sfen(), QStringLiteral("original"));
        QCOMPARE(changed.count(), 0);
    }

    void detectInitialSfen()
    {
        QString sfen = KifuFile::initialSfen<KifToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.kif")));
        QCOMPARE(sfen, kHirateSfen);
    }

    void detectInitialSfen_handicapNames_data()
    {
        QTest::addColumn<QString>("handicap");
        QTest::addColumn<QString>("expectedSfen");

        QTest::newRow("even") << QStringLiteral("平手")
            << QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        QTest::newRow("lance") << QStringLiteral("香落ち")
            << QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("bishop") << QStringLiteral("角落ち")
            << QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("rook") << QStringLiteral("飛車落ち")
            << QStringLiteral("lnsgkgsnl/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("two") << QStringLiteral("二枚落ち")
            << QStringLiteral("lnsgkgsnl/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("three") << QStringLiteral("三枚落ち")
            << QStringLiteral("lnsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("four") << QStringLiteral("四枚落ち")
            << QStringLiteral("1nsgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("five") << QStringLiteral("五枚落ち")
            << QStringLiteral("2sgkgsn1/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("six") << QStringLiteral("六枚落ち")
            << QStringLiteral("2sgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("left-seven") << QStringLiteral("左七枚落ち")
            << QStringLiteral("2sgkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("right-seven") << QStringLiteral("右七枚落ち")
            << QStringLiteral("3gkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("eight") << QStringLiteral("八枚落ち")
            << QStringLiteral("3gkg3/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("ten") << QStringLiteral("十枚落ち")
            << QStringLiteral("4k4/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("rook-lance") << QStringLiteral("飛香落ち")
            << QStringLiteral("lnsgkgsn1/7b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("right-lance") << QStringLiteral("右香落ち")
            << QStringLiteral("1nsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
        QTest::newRow("left-five-piece") << QStringLiteral("左五枚落ち")
            << QStringLiteral("1nsgkgs2/9/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");
    }

    void detectInitialSfen_handicapNames()
    {
        QFETCH(QString, handicap);
        QFETCH(QString, expectedSfen);

        QTemporaryFile tmp;
        const QByteArray content = QStringLiteral("手合割：　%1　\n").arg(handicap).toUtf8();
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, content, QStringLiteral("kif")));

        QString detectedLabel;
        QCOMPARE(KifuFile::initialSfen<KifToSfenConverter>(tmp.fileName(), &detectedLabel),
                 expectedSfen);
        QCOMPARE(detectedLabel, handicap);

        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifToSfenConverter>(tmp.fileName(), result, &error),
                 qPrintable(error));
        QCOMPARE(result.mainline.startSfen, expectedSfen);
    }

    void convertFile_sevenMoves()
    {
        QString error;
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.kif")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QCOMPARE(moves.size(), 7);
        QCOMPARE(moves, kExpectedUsiMoves);
    }

    void extractMovesWithTimes()
    {
        QString error;
        const RecordLine line = KifuFile::mainline<KifToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.kif")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        // 7手（終局の行はない）
        QCOMPARE(line.entries.size(), 7);
        QVERIFY(!line.endsWithTerminal());

        // 初手は「７六歩」で、消費時間は正規化した表記になる
        QVERIFY(line.entries[0].displayText.contains(QStringLiteral("７六歩")));
        QCOMPARE(line.entries[0].time.toText(), QStringLiteral("00:10/00:00:10"));
    }

    void parseWithVariations_basic()
    {
        KifParseResult result;
        QString error;
        bool ok = KifuFile::parse<KifToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.kif")), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(result.mainline.usiMoves().size(), 7);
        QCOMPARE(result.variations.size(), 0);
    }

    void parseWithVariations_branch()
    {
        KifParseResult result;
        QString error;
        bool ok = KifuFile::parse<KifToSfenConverter>(
            fixturePath(QStringLiteral("test_branch.kif")), result, &error);
        QVERIFY2(ok, qPrintable(error));
        QCOMPARE(result.mainline.usiMoves().size(), 7);
        QCOMPARE(result.variations.size(), 2);

        // First variation starts at ply 3
        QCOMPARE(result.variations[0].startPly, 3);
        // Second variation starts at ply 5
        QCOMPARE(result.variations[1].startPly, 5);
    }

    void extractGameInfo()
    {
        auto info = KifuFile::gameInfo<KifToSfenConverter>(
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

    void comments_preserveParagraphs()
    {
        const QString path = fixturePath(QStringLiteral("test_synthetic_comments.kif"));
        QString error;
        const RecordLine line = KifuFile::mainline<KifToSfenConverter>(path, &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));
        QVERIFY(!line.entries.isEmpty());
        // 開始局面のコメントと初手のコメントの段落（空行）が残る
        QVERIFY(line.openingComment.contains(QStringLiteral("\n\n【テスト用リンク｜コメント表示】")));
        QVERIFY(line.entries.at(0).comment.contains(QStringLiteral("\n\n【初手の確認】")));
    }

    void comments_extraction()
    {
        QString error;
        const RecordLine line = KifuFile::mainline<KifToSfenConverter>(
            fixturePath(QStringLiteral("test_comments.kif")), &error);
        QVERIFY2(error.isEmpty(), qPrintable(error));

        // 開始局面か指し手のどこかにコメントが読めている
        bool hasComment = !line.openingComment.isEmpty();
        for (const RecordEntry& entry : line.entries) {
            if (!entry.comment.isEmpty()) {
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
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(
            fixturePath(QStringLiteral("nonexistent.kif")), &error);
        QVERIFY(moves.isEmpty());
    }

    void parseWithVariations_nonExistentFile()
    {
        KifParseResult result;
        QString error;
        bool ok = KifuFile::parse<KifToSfenConverter>(
            fixturePath(QStringLiteral("nonexistent.kif")), result, &error);
        QVERIFY(!ok);
    }

    void detectInitialSfen_nonExistentFile()
    {
        QString label;
        QString sfen = KifuFile::initialSfen<KifToSfenConverter>(
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
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);
        QCOMPARE(moves.size(), 0);
    }

    void extractMovesWithTimes_emptyFile()
    {
        QTemporaryFile tmp;
        tmp.setFileTemplate(QDir::tempPath() + QStringLiteral("/test_empty_XXXXXX.kif"));
        QVERIFY(tmp.open());
        tmp.close();

        QString error;
        const RecordLine line = KifuFile::mainline<KifToSfenConverter>(tmp.fileName(), &error);
        // 指し手の行はない
        QVERIFY(line.entries.isEmpty());
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
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);
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
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);
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

        auto info = KifuFile::gameInfo<KifToSfenConverter>(tmp.fileName());
        QVERIFY(info.isEmpty());
    }

    // ========================================
    // Boundary: extractGameInfoMap
    // ========================================

    void extractGameInfoMap_basic()
    {
        auto m = KifuFile::gameInfoMap<KifToSfenConverter>(
            fixturePath(QStringLiteral("test_basic.kif")));
        // Should have key-value pairs
        QVERIFY(!m.isEmpty());
    }

    // KIF → parseWithVariations → KifuBranchTreeBuilder → node comments テスト
    void syntheticKif_commentsTransferToBranchTree()
    {
        const QString path = fixturePath(QStringLiteral("test_synthetic_comments.kif"));
        QVERIFY2(QFile::exists(path), "test_synthetic_comments.kif not found in fixtures");

        // 1. パース
        KifParseResult result;
        QString error;
        bool ok = KifuFile::parse<KifToSfenConverter>(path, result, &error);
        QVERIFY2(ok, qPrintable(QStringLiteral("Parse failed: ") + error));
        QCOMPARE(result.mainline.usiMoves().size(), 96);
        QVERIFY2(!result.mainline.entries.isEmpty(), "No record entries parsed");

        // 2. 本譜（開始局面と各行）にコメントがあることを確認
        int dispCommentCount = result.mainline.openingComment.isEmpty() ? 0 : 1;
        for (const RecordEntry& entry : std::as_const(result.mainline.entries)) {
            if (!entry.comment.isEmpty()) {
                ++dispCommentCount;
            }
        }
        qWarning("[TEST] RecordLine comment count: %d / %lld total entries",
                 dispCommentCount, result.mainline.entries.size());
        QVERIFY2(dispCommentCount > 0,
                 "No comments found in the parsed mainline");

        // 3. 初期SFENを取得
        QString initialSfen = KifuFile::initialSfen<KifToSfenConverter>(path);
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
                 treeCommentCount, mainLine.nodes.size());
        QVERIFY2(treeCommentCount > 0,
                 qPrintable(QStringLiteral(
                     "No comments found in KifuBranchNode tree! "
                     "dispCommentCount=%1 but treeCommentCount=0")
                     .arg(dispCommentCount)));
    }
    // 本譜と変化（どちらも終局の行は直前の局面を持つ）の
    // どちらの終局手ノードも、親と同じ局面SFENを持つこと
    void buildFromKifParseResult_terminalNodeCarriesParentSfen()
    {
        KifParseResult res;
        res.mainline = testLine({ QStringLiteral("▲７六歩"), QStringLiteral("△３四歩"), QStringLiteral("▲投了") },
                                { QStringLiteral("b0 b - 1"), QStringLiteral("b1 w - 2"), QStringLiteral("b2 b - 3") });
        KifVariation var;
        var.startPly = 2;
        var.line = testLine({ QStringLiteral("△８四歩"), QStringLiteral("▲中断") },
                            { QStringLiteral("b1 w - 2"), QStringLiteral("v2 b - 3"), QStringLiteral("v2 b - 3") }, 2);
        res.variations = { var };

        KifuBranchTree tree;
        KifuBranchTreeBuilder::buildFromKifParseResult(&tree, res, QStringLiteral("b0 b - 1"));

        const QList<BranchLine> lines = tree.allLines();
        QCOMPARE(lines.size(), 2);

        KifuBranchNode* mainTerminal = lines.at(0).nodes.last();
        QVERIFY(mainTerminal->isTerminal());
        QCOMPARE(mainTerminal->sfen(), QStringLiteral("b2 b - 3"));
        QCOMPARE(mainTerminal->sfen(), mainTerminal->parent()->sfen());

        KifuBranchNode* varTerminal = lines.at(1).nodes.last();
        QVERIFY(varTerminal->isTerminal());
        QCOMPARE(varTerminal->sfen(), QStringLiteral("v2 b - 3"));
        QCOMPARE(varTerminal->sfen(), varTerminal->parent()->sfen());

        // 終局手ノードの SFEN が埋まるので、行→局面の取り出しに空が混ざらない
        QVERIFY(!tree.sfenListForLine(0).contains(QString()));
        QVERIFY(!tree.sfenListForLine(1).contains(QString()));
    }

    // ツリー構築中は知らせず、構築完了時に作り直し（reset）を1回だけ知らせること
    //（以前はノード追加ごとに知らせ、表示側がそのたびに全再構築していた）
    void buildFromKifParseResult_emitsResetOnce()
    {
        KifParseResult res;
        res.mainline = testLine({ QStringLiteral("▲７六歩"), QStringLiteral("△３四歩"), QStringLiteral("▲２六歩") },
                                { QStringLiteral("b0 b - 1"), QStringLiteral("b1 w - 2"),
                                  QStringLiteral("b2 b - 3"), QStringLiteral("b3 w - 4") });
        KifVariation var;
        var.startPly = 3;
        var.line = testLine({ QStringLiteral("▲６六歩"), QStringLiteral("△８四歩") },
                            { QStringLiteral("b2 b - 3"), QStringLiteral("v3 w - 4"), QStringLiteral("v4 b - 5") }, 3);
        res.variations = { var };

        KifuBranchTree tree;
        QSignalSpy spy(&tree, &KifuBranchTree::reset);
        QSignalSpy appended(&tree, &KifuBranchTree::nodeAppended);
        KifuBranchTreeBuilder::buildFromKifParseResult(&tree, res, QStringLiteral("b0 b - 1"));

        QCOMPARE(tree.nodeCount(), 6);   // root + 本譜3 + 変化2
        QCOMPARE(spy.count(), 1);
        QCOMPARE(appended.count(), 0);
    }

    // 変化が「別の変化の投了直前の局面」から分岐するケース。
    // 投了ノードは親と同じ局面を持つため、以前は findBySfen が投了ノードを返して
    // 変化が丸ごと失われることがあった（QHash の走査順に依存）。
    void buildFromKifParseResult_branchFromPositionBeforeTerminal()
    {
        KifParseResult res;
        res.mainline = testLine({ QStringLiteral("▲７六歩"), QStringLiteral("△３四歩"), QStringLiteral("▲６六歩") },
                                { QStringLiteral("b0 b - 1"), QStringLiteral("b1 w - 2"),
                                  QStringLiteral("b2 b - 3"), QStringLiteral("b3 w - 4") });

        // 変化A: 本譜3手目の代わりに▲２六歩を指し、投了で終わる
        //（終局の行は直前の局面を持つ）
        KifVariation varA;
        varA.startPly = 3;
        varA.line = testLine({ QStringLiteral("▲２六歩"), QStringLiteral("△投了") },
                             { QStringLiteral("b2 b - 3"), QStringLiteral("a3 w - 4"), QStringLiteral("a3 w - 4") }, 3);

        // 変化B: 変化A の3手目の局面（投了の直前）から4手目で分岐
        KifVariation varB;
        varB.startPly = 4;
        varB.parentVariation = 0;
        varB.line = testLine({ QStringLiteral("△８四歩") }, { QStringLiteral("a3 w - 4"), QStringLiteral("b4 b - 5") }, 4);

        res.variations = { varA, varB };

        for (int i = 0; i < 20; ++i) {
            KifuBranchTree tree;
            KifuBranchTreeBuilder::buildFromKifParseResult(&tree, res, QStringLiteral("b0 b - 1"));

            QCOMPARE(tree.lineCount(), 3);
            KifuBranchNode* a3 = TestTree::findBySfen(tree, QStringLiteral("a3 w - 4"), 3);
            QVERIFY(a3 != nullptr);
            QCOMPARE(a3->displayText(), QStringLiteral("▲２六歩"));
            QCOMPARE(a3->childCount(), 2);
            QVERIFY(a3->childAt(0)->isTerminal());
            QCOMPARE(a3->childAt(1)->displayText(), QStringLiteral("△８四歩"));
            QCOMPARE(a3->childAt(1)->ply(), 4);
        }
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
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);
        // Must not crash; invalid piece may be skipped or produce an error
        QVERIFY(moves.isEmpty());
        QVERIFY(!error.isEmpty());
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
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);
        // Must not crash; invalid coordinates should be handled gracefully
        QVERIFY(moves.isEmpty());
        QVERIFY(!error.isEmpty());
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
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);
        // Must not crash (buffer overflow protection)
        QVERIFY(moves.isEmpty());
        QVERIFY(!error.isEmpty());
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
        QVERIFY(KifuFile::parse<KifToSfenConverter>(tmp.fileName(), result, &error));
        QCOMPARE(result.mainline.moveCount(), 2);
        QVERIFY(result.variations.isEmpty());
        QVERIFY(!error.isEmpty());
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
        QStringList moves = KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);
        // Must not crash even with truncated input
        QCOMPARE(moves, QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
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
        const auto moves = KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);
        const QStringList expected = fileContent.contains("9999999")
            ? QStringList{QStringLiteral("7g7f")} : QStringList{};
        QCOMPARE(moves, expected);
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
        QVERIFY(KifuFile::parse<KifToSfenConverter>(tmp.fileName(), result, &error));
        // Must not crash
        QCOMPARE(result.mainline.moveCount(), 2);
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations.first().line.moveCount(), 0);
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
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, fileContent, QStringLiteral("kif")));

        QString error;
        (void)KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);

        KifParseResult result;
        const bool success = KifuFile::parse<KifToSfenConverter>(tmp.fileName(), result, &error);

        (void)KifuFile::initialSfen<KifToSfenConverter>(tmp.fileName());
        (void)KifuFile::gameInfo<KifToSfenConverter>(tmp.fileName());
        (void)KifuFile::mainline<KifToSfenConverter>(tmp.fileName(), &error);

        KifuTestHelper::verifyParsedRecord(success, result, error);
    }

    // ========================================
    // 境界値テスト: 極端な手数 (1500手)
    // ========================================

    void longMoveSequence_doesNotCrash()
    {
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(
            tmp, KifuTestHelper::generateLongKifContent(1500), QStringLiteral("kif")));

        QString error;
        (void)KifuFile::usiMoves<KifToSfenConverter>(tmp.fileName(), &error);

        KifParseResult result;
        QVERIFY2(KifuFile::parse<KifToSfenConverter>(tmp.fileName(), result, &error), qPrintable(error));

        QCOMPARE(result.mainline.moveCount(), 1500);
        KifuTestHelper::verifyParsedRecord(true, result, error);
    }

    // ========================================
    // 変化の1手目が「同」：分岐元に至る直前の手（本譜・親の変化）を基準にする
    // ========================================

    void parseWithVariations_variationStartingWithSame()
    {
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, QStringLiteral(
            "手合割：平手\n"
            "手数----指手---------消費時間--\n"
            "   1 ７六歩(77)   ( 0:00/00:00:00)\n"
            "   2 ３四歩(33)   ( 0:00/00:00:00)\n"
            "   3 ２六歩(27)   ( 0:00/00:00:00)+\n"
            "   4 中断         ( 0:00/00:00:00)\n"
            "\n"
            "変化：3手\n"
            "   3 ２二角成(88) ( 0:00/00:00:00)\n"
            "   4 同　銀(31)   ( 0:00/00:00:00)+\n"
            "   5 中断         ( 0:00/00:00:00)\n"
            "\n"
            "変化：4手\n"
            "   4 同　飛(82)   ( 0:00/00:00:00)\n"
            "   5 同　角打     ( 0:00/00:00:00)\n").toUtf8(), QStringLiteral("kifu")));

        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifToSfenConverter>(tmp.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 2);
        QCOMPARE(result.variations[0].line.usiMoves(),
                 QStringList({QStringLiteral("8h2b+"), QStringLiteral("3a2b")}));
        // 入れ子の変化の1手目「同　飛」は親の変化の3手目（２二角成）の地点
        QCOMPARE(result.variations[1].line.usiMoves(),
                 QStringList({QStringLiteral("8b2b"), QStringLiteral("B*2b")}));
        QCOMPARE(result.variations[1].line.sfens().size(), 3);
    }

    void parseWithVariations_mainlineParentSame()
    {
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, QStringLiteral(
            "手合割：平手\n"
            "手数----指手---------消費時間--\n"
            "   1 ７六歩(77)\n"
            "   2 ３四歩(33)\n"
            "   3 ２二角成(88)\n"
            "   4 同　銀(31)+\n"
            "\n"
            "変化：4手\n"
            "   4 同　飛(82)\n").toUtf8(), QStringLiteral("kifu")));

        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifToSfenConverter>(tmp.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.variations.size(), 1);
        QCOMPARE(result.variations[0].line.usiMoves(), QStringList({QStringLiteral("8b2b")}));
    }

    // 消費時間欄がない行の分岐マーク「+」は指し手の表記に含めない
    void extractMoves_branchMarkWithoutTime()
    {
        QTemporaryFile tmp;
        QVERIFY(KifuTestHelper::writeToTempFile(tmp, QStringLiteral(
            "手合割：平手\n"
            "手数----指手--\n"
            "   1 ７六歩(77)   +\n"
            "   2 ３四歩(33)+\n"
            "   3 ２六歩(27)\n"
            "\n"
            "変化：2手\n"
            "   2 ８四歩(83)   +\n"
            "\n"
            "変化：2手\n"
            "   2 ４四歩(43)\n").toUtf8(), QStringLiteral("kif")));

        KifParseResult result;
        QString error;
        QVERIFY2(KifuFile::parse<KifToSfenConverter>(tmp.fileName(), result, &error), qPrintable(error));
        QCOMPARE(result.mainline.entries.size(), 3);
        QCOMPARE(result.mainline.entries[0].displayText, QStringLiteral("▲７六歩(77)"));
        QCOMPARE(result.mainline.entries[1].displayText, QStringLiteral("△３四歩(33)"));
        QCOMPARE(result.mainline.usiMoves().size(), 3);
        QCOMPARE(result.variations.size(), 2);
        QCOMPARE(result.variations[0].line.entries.first().displayText, QStringLiteral("△８四歩(83)"));
    }
    // ========================================
    // テキスト版の入口（貼り付けた棋譜を一時ファイルなしで読む）
    // ========================================

    void parseText_matchesFile()
    {
        const QString path = fixturePath(QStringLiteral("test_branch.kif"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        const QString text = QString::fromUtf8(file.readAll());

        KifParseResult fromFile;
        QString fileError;
        QVERIFY(KifuFile::parse<KifToSfenConverter>(path, fromFile, &fileError));
        const KifuParseOutcome outcome = KifToSfenConverter::parse(text);
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const KifParseResult& fromText = outcome.record;
        QCOMPARE(fromText.mainline.usiMoves(), fromFile.mainline.usiMoves());
        QCOMPARE(fromText.mainline.sfens(), fromFile.mainline.sfens());
        QCOMPARE(fromText.variations.size(), fromFile.variations.size());
        for (qsizetype i = 0; i < fromText.variations.size(); ++i) {
            QCOMPARE(fromText.variations[i].startPly, fromFile.variations[i].startPly);
            QCOMPARE(fromText.variations[i].line.usiMoves(), fromFile.variations[i].line.usiMoves());
        }
        // 開始局面と対局情報も同じ解析で得る
        QCOMPARE(outcome.initialSfen, KifuFile::initialSfen<KifToSfenConverter>(path));
        QCOMPARE(outcome.initialSfen, fromText.mainline.startSfen);
        QCOMPARE(outcome.gameInfo.size(), KifuFile::gameInfo<KifToSfenConverter>(path).size());
    }

    void parseText_crlfAndBom()
    {
        const QString text = QChar(0xFEFF) + QStringLiteral(
            "手合割：平手\r\n手数----指手---------消費時間--\r\n   1 ７六歩(77)\r\n   2 ３四歩(33)\r\n   3 投了\r\n");
        const KifuParseOutcome outcome = KifToSfenConverter::parse(text);
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const RecordLine& line = outcome.record.mainline;
        QCOMPARE(line.usiMoves(), QStringList({QStringLiteral("7g7f"), QStringLiteral("3c3d")}));
        QVERIFY(line.endsWithTerminal());
        QCOMPARE(line.terminal()->terminal, TerminalType::Resign);
        QCOMPARE(line.entries.last().displayText, QStringLiteral("▲投了"));
    }

    // ========================================
    // 1回の走査: 表示用の指し手と USI の指し手の並びがそろう
    // ========================================

    // 終局語を含む見出し行（以前は USI の抽出だけがここで止まり、表示とずれた）
    void singlePass_headerWithTerminalWordKeepsLockstep()
    {
        const KifuParseOutcome outcome = KifToSfenConverter::parse(QStringLiteral(
            "手合割：平手\n作者：詰将棋太郎\n手数----指手---------消費時間--\n"
            "   1 ７六歩(77)\n   2 ３四歩(33)\n"));
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const RecordLine& line = outcome.record.mainline;
        QCOMPARE(line.usiMoves().size(), 2);
        QCOMPARE(line.entries.size(), 2);  // 2手（終局の行はない）
        QCOMPARE(line.sfens().size(), 3);  // 開始局面 + 2手
    }

    // 読めない指し手の前で手順を打ち切り、警告を残す（表示と USI がずれない）
    void singlePass_unparsableMoveStopsLine()
    {
        const KifuParseOutcome outcome = KifToSfenConverter::parse(QStringLiteral(
            "手数----指手---------消費時間--\n   1 ７六歩(77)\n   2 謎の手\n   3 ２六歩(27)\n"));
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const RecordLine& line = outcome.record.mainline;
        QCOMPARE(line.usiMoves(), QStringList({QStringLiteral("7g7f")}));
        QCOMPARE(line.entries.size(), 1);
        QVERIFY2(outcome.warning.contains(QStringLiteral("謎の手")), qPrintable(outcome.warning));
        QVERIFY2(outcome.warning.contains(QStringLiteral("read up to move 1")), qPrintable(outcome.warning));
    }

    // 読めない手で棋譜全体を読めなかったことにする指定（FailRecord）では、読み込み自体が失敗する
    void failRecordOption_unparsableMoveFailsTheRecord()
    {
        const ParseOptions options{ParseOptions::UnreadableMove::FailRecord};
        const KifuParseOutcome outcome = KifToSfenConverter::parse(QStringLiteral(
            "手数----指手---------消費時間--\n   1 ７六歩(77)\n   2 謎の手\n   3 ２六歩(27)\n"), options);
        QVERIFY(!outcome.success);
        QVERIFY2(outcome.warning.contains(QStringLiteral("謎の手")), qPrintable(outcome.warning));
        // 打ち切りではないので「何手目まで読んだか」は書かない
        QVERIFY2(!outcome.warning.contains(QStringLiteral("read up to move")), qPrintable(outcome.warning));
    }

    // 指し手の後ろに書いたコメントはその手に付く
    void singlePass_inlineCommentBelongsToItsMove()
    {
        const KifuParseOutcome outcome = KifToSfenConverter::parse(QStringLiteral(
            "手数----指手---------消費時間--\n   1 ７六歩(77) *角道を開ける\n   2 ３四歩(33) *同じく\n"));
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const RecordLine& line = outcome.record.mainline;
        QCOMPARE(line.entries.size(), 2);
        QVERIFY(line.openingComment.isEmpty());
        QCOMPARE(line.entries[0].comment, QStringLiteral("角道を開ける"));
        QCOMPARE(line.entries[1].comment, QStringLiteral("同じく"));
    }

    // 変化の開始局面: 直前の変化が分岐点まで届かなければ本譜から取る
    void variationBase_fallsBackToMainline()
    {
        const KifuParseOutcome outcome = KifToSfenConverter::parse(QStringLiteral(
            "手数----指手---------消費時間--\n"
            "   1 ７六歩(77)\n   2 ３四歩(33)\n   3 ２六歩(27)\n   4 ８四歩(83)\n   5 ２五歩(26)\n"
            "\n変化：3手\n   3 ６六歩(67)\n"
            "\n変化：5手\n   5 ６八銀(79)\n"));
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const KifParseResult& result = outcome.record;
        QCOMPARE(result.variations.size(), 2);
        QCOMPARE(result.variations[1].line.startSfen, result.mainline.sfenAt(4));
        QCOMPARE(result.variations[1].parentVariation, -1);
        QCOMPARE(result.variations[1].line.usiMoves(), QStringList({QStringLiteral("7i6h")}));
    }

    // 変化のコメント・しおりは最初の指し手に付き、開始局面のコメント・しおりは残らない
    void variation_commentsAndBookmarkOnFirstMove()
    {
        const KifuParseOutcome outcome = KifToSfenConverter::parse(QStringLiteral(
            "手数----指手---------消費時間--\n   1 ７六歩(77)\n   2 ３四歩(33)\n"
            "\n変化：2手\n   2 ８四歩(83)\n*分岐コメント\n&分岐しおり\n   3 ２六歩(27)\n"));
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        QCOMPARE(outcome.record.variations.size(), 1);
        const RecordLine& line = outcome.record.variations[0].line;
        QCOMPARE(line.entries.size(), 2);
        QCOMPARE(line.startPly, 2);
        QVERIFY(line.openingComment.isEmpty());
        QVERIFY(line.openingBookmark.isEmpty());
        QCOMPARE(line.entries[0].comment, QStringLiteral("分岐コメント"));
        QCOMPARE(line.entries[0].bookmark, QStringLiteral("分岐しおり"));
        QCOMPARE(line.usiMoves(), QStringList({QStringLiteral("8c8d"), QStringLiteral("2g2f")}));
    }

    // 不正な変化の開始手数は読み飛ばして警告する
    void variation_invalidStartIsSkippedWithWarning()
    {
        const KifuParseOutcome outcome = KifToSfenConverter::parse(QStringLiteral(
            "手数----指手---------消費時間--\n   1 ７六歩(77)\n\n変化：9手\n   9 ５五角打\n"));
        QVERIFY(outcome.success);
        QCOMPARE(outcome.record.variations.size(), 0);
        QVERIFY2(outcome.warning.contains(QStringLiteral("variation start")), qPrintable(outcome.warning));
    }

    // 指し手の表記は手から作り直す（「同歩」は「同　歩」、移動元を付ける）
    void displayText_isGeneratedFromTheMove()
    {
        const KifuParseOutcome outcome = KifToSfenConverter::parse(QStringLiteral(
            "手数----指手---------消費時間--\n"
            "   1 ２六歩(27)\n   2 ８四歩(83)\n   3 ２五歩(26)\n   4 ８五歩(84)\n"
            "   5 ２四歩(25)\n   6 同歩(23)\n"));
        QVERIFY2(outcome.success, qPrintable(outcome.warning));
        const RecordLine& line = outcome.record.mainline;
        QCOMPARE(line.moveCount(), 6);
        QCOMPARE(line.displayText(6), QStringLiteral("△同　歩(23)"));
        QCOMPARE(line.usiMove(6).toString(), QStringLiteral("2c2d"));
        QCOMPARE(line.entries[5].mover, Turn::White);
    }
};

QTEST_MAIN(TestKifConverter)
#include "tst_kifconverter.moc"
