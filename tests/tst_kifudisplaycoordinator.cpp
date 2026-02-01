#include <QtTest>

#include "kifudisplaycoordinator.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "shogimove.h"

class TestKifuDisplayCoordinator : public QObject
{
    Q_OBJECT

private slots:
    void testStartFromMidPositionTrimsRecordModel();
};

void TestKifuDisplayCoordinator::testStartFromMidPositionTrimsRecordModel()
{
    KifuBranchTree tree;
    tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));

    // 本譜を6手目まで作成
    KifuBranchNode* node = tree.root();
    node = tree.addMove(node, ShogiMove(QPoint(7, 7), QPoint(7, 6), QLatin1Char('P'), QLatin1Char(' '), false),
                        QStringLiteral("▲７六歩(77)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(3, 3), QPoint(3, 4), QLatin1Char('p'), QLatin1Char(' '), false),
                        QStringLiteral("△３四歩(33)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL b - 3"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(2, 7), QPoint(2, 6), QLatin1Char('P'), QLatin1Char(' '), false),
                        QStringLiteral("▲２六歩(27)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/pppppp1pp/6p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL w - 4"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(8, 3), QPoint(8, 4), QLatin1Char('p'), QLatin1Char(' '), false),
                        QStringLiteral("△８四歩(83)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/9/2P4P1/PP1PPPP1P/1B5R1/LNSGKGSNL b - 5"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(2, 6), QPoint(2, 5), QLatin1Char('P'), QLatin1Char(' '), false),
                        QStringLiteral("▲２五歩(26)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/1p4p2/7P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL w - 6"),
                        QString());
    node = tree.addMove(node, ShogiMove(QPoint(8, 4), QPoint(8, 5), QLatin1Char('p'), QLatin1Char(' '), false),
                        QStringLiteral("△８五歩(84)"),
                        QStringLiteral("lnsgkgsnl/1r5b1/p1pppp1pp/9/1p5P1/2P6/PP1PPPP1P/1B5R1/LNSGKGSNL b - 7"),
                        QString());

    KifuNavigationState state;
    state.setTree(&tree);

    KifuNavigationController nav;
    nav.setTreeAndState(&tree, &state);

    KifuDisplayCoordinator coordinator(&tree, &state, &nav);
    KifuRecordListModel recordModel;
    coordinator.setRecordModel(&recordModel);

    // 旧表示を想定して全手数を表示（手動でモデルに詰める）
    recordModel.appendItem(new KifuDisplay(QStringLiteral("=== 開始局面 ==="),
                                           QStringLiteral("（１手 / 合計）")));
    QVector<KifuBranchNode*> pathToEnd = tree.pathToNode(node);
    for (KifuBranchNode* n : std::as_const(pathToEnd)) {
        if (n == nullptr || n->ply() == 0) continue;
        const QString num = QString::number(n->ply());
        const QString spaces = QString(qMax(0, 4 - num.length()), QLatin1Char(' '));
        const QString line = spaces + num + QLatin1Char(' ') + n->displayText();
        recordModel.appendItem(new KifuDisplay(line, n->timeText(), n->comment()));
    }
    QCOMPARE(recordModel.rowCount(), 7); // 開始局面 + 6手

    // 4手目からセッション開始
    KifuBranchNode* node4 = tree.findByPlyOnMainLine(4);
    QVERIFY(node4 != nullptr);
    coordinator.onLiveGameSessionStarted(node4);

    // セッション開始直後は 4手目までにトリムされる
    QCOMPARE(recordModel.rowCount(), 5);
    const QModelIndex idx = recordModel.index(4, 0);
    const QString moveText = recordModel.data(idx, Qt::DisplayRole).toString();
    QVERIFY(moveText.contains(node4->displayText()));
}

QTEST_GUILESS_MAIN(TestKifuDisplayCoordinator)
#include "tst_kifudisplaycoordinator.moc"
