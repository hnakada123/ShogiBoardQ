#include <QtTest>

#include "abstractlistmodel.h"

namespace {
class DummyItem
{
public:
    DummyItem()
    {
        ++s_liveCount;
    }

    ~DummyItem()
    {
        --s_liveCount;
    }

    static int s_liveCount;
};

int DummyItem::s_liveCount = 0;

class DummyListModel : public AbstractListModel<DummyItem>
{
public:
    explicit DummyListModel(QObject* parent = nullptr)
        : AbstractListModel(parent)
    {
    }

    int columnCount(const QModelIndex& parent = QModelIndex()) const override
    {
        Q_UNUSED(parent)
        return 1;
    }

    QVariant data(const QModelIndex& index, int role) const override
    {
        if (!index.isValid() || role != Qt::DisplayRole) {
            return {};
        }
        return QStringLiteral("dummy");
    }
};
} // namespace

class TestAbstractListModel : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        QCOMPARE(DummyItem::s_liveCount, 0);
    }

    void cleanup()
    {
        QCOMPARE(DummyItem::s_liveCount, 0);
    }

    void removeItem_deletesRemovedObject()
    {
        DummyListModel model;
        DummyItem* item = new DummyItem();

        model.appendItem(item);
        QCOMPARE(DummyItem::s_liveCount, 1);
        QCOMPARE(model.rowCount(), 1);

        model.removeItem(item);
        QCOMPARE(DummyItem::s_liveCount, 0);
        QCOMPARE(model.rowCount(), 0);
    }

    void removeLastItem_deletesLastObject()
    {
        DummyListModel model;
        model.appendItem(new DummyItem());
        model.appendItem(new DummyItem());
        QCOMPARE(DummyItem::s_liveCount, 2);
        QCOMPARE(model.rowCount(), 2);

        model.removeLastItem();
        QCOMPARE(DummyItem::s_liveCount, 1);
        QCOMPARE(model.rowCount(), 1);

        model.clearAllItems();
        QCOMPARE(DummyItem::s_liveCount, 0);
        QCOMPARE(model.rowCount(), 0);
    }
};

QTEST_MAIN(TestAbstractListModel)
#include "tst_abstractlistmodel.moc"
