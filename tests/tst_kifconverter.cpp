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
        QVector<BranchLine> lines = tree.allLines();
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
};

QTEST_MAIN(TestKifConverter)
#include "tst_kifconverter.moc"
