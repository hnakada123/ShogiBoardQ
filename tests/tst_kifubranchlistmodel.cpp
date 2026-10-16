#include <QtTest>
#include <QSignalSpy>

#include "kifubranchlistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "shogimove.h"

static const QString kHirateSfen =
    QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

/// 分岐候補欄のモデル。行は分岐ツリーのノードで、表記は表示するたびにノードから作る
class TestKifuBranchListModel : public QObject
{
    Q_OBJECT

    KifuBranchTree m_tree;
    QList<KifuBranchNode*> m_firstMoves;   ///< 開始局面から指せる3つの手（分岐候補）

private slots:
    void init()
    {
        m_tree.setRootSfen(kHirateSfen);
        m_firstMoves.clear();
        for (const QString& text : {QStringLiteral("▲７六歩(77)"), QStringLiteral("▲２六歩(27)"),
                                    QStringLiteral("▲５六歩(57)")}) {
            m_firstMoves.append(m_tree.addMove(m_tree.root(), ShogiMove(), text, QStringLiteral("after")));
        }
    }

    void rowsAreCandidateNodesFollowedByBackToMain()
    {
        KifuBranchListModel model(&m_tree);
        QCOMPARE(model.rowCount(), 0);

        model.setCandidates(m_firstMoves, 1, true);
        QCOMPARE(model.rowCount(), 4);
        QCOMPARE(model.candidateCount(), 3);
        QVERIFY(model.hasBackToMainRow());
        QVERIFY(model.isBackToMainRow(3));
        QVERIFY(!model.isBackToMainRow(2));
        QCOMPARE(model.currentHighlightRow(), 1);
        QVERIFY(model.data(model.index(0, 0)).toString().contains(QStringLiteral("７六歩")));
        QVERIFY(model.data(model.index(1, 0)).toString().contains(QStringLiteral("２六歩")));
        QCOMPARE(model.data(model.index(3, 0)).toString(), QStringLiteral("本譜へ戻る"));
        QVERIFY(model.data(model.index(1, 0), Qt::BackgroundRole).isValid());
        QVERIFY(!model.data(model.index(0, 0), Qt::BackgroundRole).isValid());
    }

    /// 表記はノードから作るので、ノードの表記が変われば描き直しで新しい表記になる
    void labelsFollowTheNode()
    {
        KifuBranchListModel model(&m_tree);
        model.setCandidates(m_firstMoves, -1, false);
        m_firstMoves.at(2)->record().displayText = QStringLiteral("▲９六歩(97)");
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.refreshAll();
        QCOMPARE(changed.count(), 1);
        QVERIFY(model.data(model.index(2, 0)).toString().contains(QStringLiteral("９六歩")));
    }

    /// 分岐の無い局面では手を指すたびに呼ばれる。行が同じならリセット（表全体の描き直し）をせず、
    /// 強調する行が変わったときだけその2行を描き直す
    void resetsOnlyWhenRowsChange()
    {
        KifuBranchListModel model(&m_tree);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);

        model.setCandidates({}, -1, false);
        QCOMPARE(reset.count(), 0);

        model.setCandidates(m_firstMoves, 0, false);
        QCOMPARE(reset.count(), 1);
        model.setCandidates(m_firstMoves, 0, false);
        QCOMPARE(reset.count(), 1);
        QCOMPARE(changed.count(), 0);

        model.setCandidates(m_firstMoves, 2, false);
        QCOMPARE(reset.count(), 1);
        QCOMPARE(changed.count(), 2);
        QCOMPARE(model.currentHighlightRow(), 2);

        model.setCandidates(m_firstMoves, 2, true);
        QCOMPARE(reset.count(), 2);
        QCOMPARE(model.rowCount(), 4);

        model.setCandidates(m_firstMoves.mid(0, 2), 0, false);
        QCOMPARE(reset.count(), 3);
        QCOMPARE(model.rowCount(), 2);
    }

    /// 候補のノードがツリーから消えても、描き直しで落ちない
    void removedNodeShowsNothing()
    {
        KifuBranchListModel model(&m_tree);
        model.setCandidates(m_firstMoves, 0, false);
        QVERIFY(m_tree.removeSubtree(m_firstMoves.at(1)));
        QVERIFY(!model.data(model.index(1, 0)).isValid());
        QVERIFY(model.data(model.index(0, 0)).isValid());
    }
};

QTEST_MAIN(TestKifuBranchListModel)
#include "tst_kifubranchlistmodel.moc"
