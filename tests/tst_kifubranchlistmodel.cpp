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

    void clearBranchCandidates_clearsRowsAndBackToMainRow()
    {
        KifuBranchListModel model;

        QList<KifDisplayItem> items;
        items.append(KifDisplayItem(QStringLiteral("▲７六歩(77)")));
        model.updateBranchCandidates(items);
        model.setHasBackToMainRow(true);
        QCOMPARE(model.rowCount(), 2);

        model.clearBranchCandidates();

        QCOMPARE(model.rowCount(), 0);
        QCOMPARE(model.hasBackToMainRow(), false);
    }

    void updateBranchCandidates_replacesRows()
    {
        KifuBranchListModel model;

        QList<KifDisplayItem> items;
        items.append(KifDisplayItem(QStringLiteral("▲２六歩(27)")));
        model.updateBranchCandidates(items);
        QCOMPARE(model.rowCount(), 1);

        QList<KifDisplayItem> items2;
        items2.append(KifDisplayItem(QStringLiteral("▲７六歩(77)")));
        items2.append(KifDisplayItem(QStringLiteral("▲６六歩(67)")));
        model.updateBranchCandidates(items2);

        QCOMPARE(model.rowCount(), 2);
        QCOMPARE(model.labelAt(0), QStringLiteral("▲７六歩(77)"));
        QCOMPARE(model.labelAt(1), QStringLiteral("▲６六歩(67)"));
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
