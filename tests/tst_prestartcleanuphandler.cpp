#include <QtTest>

#include "prestartcleanuphandler.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "shogimove.h"

class TestPreStartCleanupHandler : public QObject
{
    Q_OBJECT

private slots:
    void testStartFromCurrentPositionWhenStartSfenSet();
    void testStartFromCurrentPositionWhenStartSfenEmpty();
    void testStartFromCurrentPositionStartpos();
    void testFindBySfenReturnsCorrectNodeOnMainLine();
    void testFindBySfenReturnsCorrectNodeOnBranchLine();
};

void TestPreStartCleanupHandler::testStartFromCurrentPositionWhenStartSfenSet()
{
    const QString startSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString currentSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/p1ppppppp/1p7/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 5");

    QVERIFY(PreStartCleanupHandler::shouldStartFromCurrentPosition(startSfen, currentSfen, 4));
}

void TestPreStartCleanupHandler::testStartFromCurrentPositionWhenStartSfenEmpty()
{
    const QString startSfen;
    const QString currentSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/p1ppppppp/1p7/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 5");

    QVERIFY(PreStartCleanupHandler::shouldStartFromCurrentPosition(startSfen, currentSfen, 0));
}

void TestPreStartCleanupHandler::testStartFromCurrentPositionStartpos()
{
    const QString startSfen = QStringLiteral(
        "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
    const QString currentSfen = QStringLiteral("startpos");

    QVERIFY(!PreStartCleanupHandler::shouldStartFromCurrentPosition(startSfen, currentSfen, 0));
}

/**
 * @brief findBySfenで本譜のSFENを検索した場合、本譜のノードが返されることを確認
 *
 * シナリオ:
 * - 本譜と分岐で同じply（6手目）に異なるSFENがある
 * - 本譜のSFENで検索すると、本譜のノードが返される
 */
void TestPreStartCleanupHandler::testFindBySfenReturnsCorrectNodeOnMainLine()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜を作成
    KifuBranchNode* node = tree.root();
    node = tree.addMove(node,
        ShogiMove(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲７六歩"), QStringLiteral("sfen_main_1"), QString());
    node = tree.addMove(node,
        ShogiMove(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△３四歩"), QStringLiteral("sfen_main_2"), QString());
    KifuBranchNode* node2 = node;  // 分岐点を保存
    node = tree.addMove(node,
        ShogiMove(QPoint(2, 7), QPoint(2, 6), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲２六歩"), QStringLiteral("sfen_main_3"), QString());
    node = tree.addMove(node,
        ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８四歩"), QStringLiteral("sfen_main_4"), QString());
    node = tree.addMove(node,
        ShogiMove(QPoint(2, 6), QPoint(2, 5), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲２五歩"), QStringLiteral("sfen_main_5"), QString());
    KifuBranchNode* mainNode6 = tree.addMove(node,
        ShogiMove(QPoint(8, 4), QPoint(8, 5), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８五歩"), QStringLiteral("sfen_main_6"), QString());

    // 分岐を作成（3手目から異なる）
    KifuBranchNode* branchNode = tree.addMove(node2,
        ShogiMove(QPoint(1, 7), QPoint(1, 6), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲１六歩"), QStringLiteral("sfen_branch_3"), QString());
    branchNode = tree.addMove(branchNode,
        ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８四歩"), QStringLiteral("sfen_branch_4"), QString());
    branchNode = tree.addMove(branchNode,
        ShogiMove(QPoint(4, 9), QPoint(4, 8), QLatin1Char('S'), QLatin1Char(' '), false),
        QStringLiteral("▲４八銀"), QStringLiteral("sfen_branch_5"), QString());
    KifuBranchNode* branchNode6 = tree.addMove(branchNode,
        ShogiMove(QPoint(8, 4), QPoint(8, 5), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８五歩"), QStringLiteral("sfen_branch_6"), QString());

    // 本譜の6手目のSFENで検索
    KifuBranchNode* found = tree.findBySfen(QStringLiteral("sfen_main_6"));
    QVERIFY(found != nullptr);
    QCOMPARE(found, mainNode6);
    QCOMPARE(found->ply(), 6);
    QCOMPARE(found->displayText(), QStringLiteral("△８五歩"));

    // 分岐の6手目のSFENで検索
    found = tree.findBySfen(QStringLiteral("sfen_branch_6"));
    QVERIFY(found != nullptr);
    QCOMPARE(found, branchNode6);
    QCOMPARE(found->ply(), 6);
}

/**
 * @brief findBySfenで分岐のSFENを検索した場合、分岐のノードが返されることを確認
 */
void TestPreStartCleanupHandler::testFindBySfenReturnsCorrectNodeOnBranchLine()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜を作成
    KifuBranchNode* node = tree.root();
    node = tree.addMove(node,
        ShogiMove(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false),
        QStringLiteral("▲７六歩"), QStringLiteral("sfen_1"), QString());
    KifuBranchNode* node1 = node;  // 分岐点を保存
    node = tree.addMove(node,
        ShogiMove(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△３四歩"), QStringLiteral("sfen_main_2"), QString());

    // 分岐を作成（2手目から異なる）
    KifuBranchNode* branchNode2 = tree.addMove(node1,
        ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
        QStringLiteral("△８四歩"), QStringLiteral("sfen_branch_2"), QString());

    // 本譜の2手目と分岐の2手目は同じplyだが異なるSFEN
    QCOMPARE(node->ply(), 2);
    QCOMPARE(branchNode2->ply(), 2);
    QVERIFY(node->sfen() != branchNode2->sfen());

    // 分岐のSFENで検索すると分岐のノードが返される
    KifuBranchNode* found = tree.findBySfen(QStringLiteral("sfen_branch_2"));
    QVERIFY(found != nullptr);
    QCOMPARE(found, branchNode2);
    QCOMPARE(found->displayText(), QStringLiteral("△８四歩"));

    // 本譜のSFENで検索すると本譜のノードが返される
    found = tree.findBySfen(QStringLiteral("sfen_main_2"));
    QVERIFY(found != nullptr);
    QCOMPARE(found, node);
    QCOMPARE(found->displayText(), QStringLiteral("△３四歩"));
}

QTEST_GUILESS_MAIN(TestPreStartCleanupHandler)
#include "tst_prestartcleanuphandler.moc"
