#include <QtTest>

#include "kifubranchlistmodel.h"

class TestKifuBranchListModel : public QObject
{
    Q_OBJECT

private slots:
    void labelAt_reflectsDisplayRoleText()
    {
        KifuBranchListModel model;

        QList<KifDisplayItem> items;
        items.append(KifDisplayItem(QStringLiteral("3 ▲２六歩(27)")));
        model.updateBranchCandidates(items);

        QCOMPARE(model.rowCount(), 1);
        QCOMPARE(model.labelAt(0), QStringLiteral("▲２六歩(27)"));
    }

    void clearBranchCandidates_unlocksAndClearsEvenWhenLocked()
    {
        KifuBranchListModel model;

        QList<KifDisplayItem> items;
        items.append(KifDisplayItem(QStringLiteral("▲７六歩(77)")));
        model.updateBranchCandidates(items);
        model.setLocked(true);

        model.clearBranchCandidates();

        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.hasBackToMainRow(), false);
        QCOMPARE(model.isLocked(), false);
    }

    void updateBranchCandidates_doesNotAutoLock()
    {
        KifuBranchListModel model;
        model.setLocked(false);

        QList<KifDisplayItem> items;
        items.append(KifDisplayItem(QStringLiteral("▲２六歩(27)")));
        model.updateBranchCandidates(items);

        QCOMPARE(model.isLocked(), false);
    }

    void firstBranchRowIndex_treatsBackToMainAsSuffixRow()
    {
        KifuBranchListModel model;

        QCOMPARE(model.firstBranchRowIndex(), -1);

        QList<KifDisplayItem> items;
        items.append(KifDisplayItem(QStringLiteral("▲２六歩(27)")));
        model.updateBranchCandidates(items);
        model.setHasBackToMainRow(true);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.branchCandidateCount(), 1);
        QCOMPARE(model.firstBranchRowIndex(), 0);
        QCOMPARE(model.labelAt(1), QStringLiteral("本譜へ戻る"));
    }
};

QTEST_MAIN(TestKifuBranchListModel)
#include "tst_kifubranchlistmodel.moc"
