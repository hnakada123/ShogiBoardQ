#include <QtTest>
#include <QStandardItemModel>
#include <QTableWidget>
#include <functional>

#include "tableheaderview.h"
#include "engineinfowidget.h"
#include "incrementaltableview.h"
#include "tablestyles.h"

/// 見出しの文字の大きさを覚える見出しが、条件を変えても Qt がその場で測った値と同じ大きさを返すこと
class TestTableHeaderView : public QObject
{
    Q_OBJECT

    /// 覚えた値と、Qt がその場で測った値を取り出す
    class ProbeHeader : public TableHeaderView
    {
    public:
        using TableHeaderView::TableHeaderView;
        QSize cached(int section) const { return sectionSizeFromContents(section); }
        QSize measured(int section) const { return QHeaderView::sectionSizeFromContents(section); }
    };

    /// 描き直しの回数を数える
    class PaintCounter : public QObject
    {
    public:
        explicit PaintCounter(int* count) : m_count(count) {}

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Paint) ++*m_count;
            return QObject::eventFilter(watched, event);
        }

    private:
        int* m_count;
    };

    static void verifySameSizes(const ProbeHeader& header, const QString& label)
    {
        for (int section = 0; section < header.count(); ++section) {
            const QSize measured = header.measured(section);
            QVERIFY2(header.cached(section) == measured,
                     qPrintable(QStringLiteral("%1: section %2").arg(label).arg(section)));
            // 2回目（覚えた値）も同じ
            QVERIFY2(header.cached(section) == measured,
                     qPrintable(QStringLiteral("%1: section %2 (2nd)").arg(label).arg(section)));
        }
    }

private slots:
    void sizesFollowEveryCondition()
    {
        QStandardItemModel model(3, 4);
        model.setHorizontalHeaderLabels({QStringLiteral("時間(ms)"), QStringLiteral("深さ"),
                                         QStringLiteral("ノード数"), QStringLiteral("読み筋")});
        QTableView view;
        auto* header = new ProbeHeader(Qt::Horizontal, &view);
        view.setHorizontalHeader(header);
        view.setModel(&model);
        view.verticalHeader()->hide();  // 見出しの幅を表示領域の幅だけで決める
        view.setStyleSheet(TableStyles::thinking());
        view.resize(600, 200);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        verifySameSizes(*header, QStringLiteral("initial"));

        // スクロールバーの出入りのように、見出しの幅が2つの値を行き来する
        for (const int width : {500, 600, 500, 300}) {
            view.resize(width, 200);
            QCoreApplication::processEvents();
            verifySameSizes(*header, QStringLiteral("width %1").arg(width));
        }

        // 書体と見出しの高さは変えず、左の余白だけを変える（スタイルの変更で測り直すこと）
        const QSize headerSize = header->size();
        view.setStyleSheet(TableStyles::thinking()
                           + QStringLiteral("QHeaderView::section { padding-left: 30px; }"));
        QCoreApplication::processEvents();
        QCOMPARE(header->size(), headerSize);
        verifySameSizes(*header, QStringLiteral("style sheet"));

        model.setHeaderData(1, Qt::Horizontal, QStringLiteral("とても長い見出しの深さ"));
        verifySameSizes(*header, QStringLiteral("text"));

        QFont big = view.font();
        big.setPointSize(big.pointSize() + 6);
        model.setHeaderData(2, Qt::Horizontal, big, Qt::FontRole);
        verifySameSizes(*header, QStringLiteral("font role"));

        QPixmap icon(24, 24);
        icon.fill(Qt::red);
        model.setHeaderData(0, Qt::Horizontal, QIcon(icon), Qt::DecorationRole);
        verifySameSizes(*header, QStringLiteral("icon"));

        QFont headerFont = header->font();
        headerFont.setPointSize(headerFont.pointSize() + 3);
        header->setFont(headerFont);
        verifySameSizes(*header, QStringLiteral("header font"));


        header->setSortIndicatorShown(true);
        verifySameSizes(*header, QStringLiteral("sort indicator"));

        header->setEnabled(false);
        verifySameSizes(*header, QStringLiteral("disabled"));
        header->setEnabled(true);

        view.setLayoutDirection(Qt::RightToLeft);
        QCoreApplication::processEvents();
        verifySameSizes(*header, QStringLiteral("right to left"));

        // 大きさを決める値を見出しの SizeHintRole で渡したときは、それをそのまま返す
        model.setHeaderData(3, Qt::Horizontal, QSize(77, 33), Qt::SizeHintRole);
        QCOMPARE(header->cached(3), QSize(77, 33));
    }

    // 表の選択が変わっても、見出しの描き分けが変わらなければ描き直さない。変わるときは描き直し、
    // Qt の見出しと同じ絵になる
    void selectionRepaintsOnlyWhenHighlightChanges()
    {
        QStandardItemModel model(1, 3);
        QStandardItemModel referenceModel(1, 3);
        for (QStandardItemModel* m : {&model, &referenceModel})
            m->setHorizontalHeaderLabels({QStringLiteral("指し手"), QStringLiteral("消費時間"), QStringLiteral("コメント")});
        QWidget window;
        QTableView view(&window);
        QTableView reference(&window);
        view.setHorizontalHeader(new TableHeaderView(Qt::Horizontal, &view));
        view.horizontalHeader()->setSectionsClickable(true);
        view.horizontalHeader()->setHighlightSections(true);
        for (QTableView* table : {&view, &reference}) {
            table->setModel(table == &view ? &model : &referenceModel);
            table->setSelectionBehavior(QAbstractItemView::SelectRows);
            table->setSelectionMode(QAbstractItemView::SingleSelection);
            table->verticalHeader()->hide();
        }
        view.setGeometry(0, 0, 360, 200);
        reference.setGeometry(0, 210, 360, 200);
        window.resize(360, 420);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));

        int paints = 0;
        PaintCounter counter(&paints);
        view.horizontalHeader()->viewport()->installEventFilter(&counter);
        auto step = [&](const std::function<void(QTableView&)>& change) {
            change(view);
            change(reference);
            QCoreApplication::processEvents();
            QCOMPARE(view.horizontalHeader()->grab().toImage(), reference.horizontalHeader()->grab().toImage());
        };
        auto rowsTo = [](int rows) {
            return [rows](QTableView& table) {
                auto* m = static_cast<QStandardItemModel*>(table.model());
                while (m->rowCount() < rows) m->appendRow({new QStandardItem(QStringLiteral("▲７六歩")), new QStandardItem(QStringLiteral("00:01")), new QStandardItem});
            };
        };
        // 1行だけの表で選ぶと、列全体が選ばれた描き分けになる
        step([](QTableView& table) { table.selectRow(0); });
        if (QTest::currentTestFailed()) return;
        step(rowsTo(30));
        if (QTest::currentTestFailed()) return;
        step([](QTableView& table) { table.selectRow(3); });
        if (QTest::currentTestFailed()) return;
        // 選ぶ行が替わるだけなら、見出しは描き直さない
        for (int row = 4; row < 10; ++row) {
            paints = 0;
            view.selectRow(row);
            reference.selectRow(row);
            QCoreApplication::processEvents();
            QCOMPARE(paints, 0);
            QCOMPARE(view.horizontalHeader()->grab().toImage(), reference.horizontalHeader()->grab().toImage());
        }
        // 選択を外す・列を選ぶ（描き分けが変わる）
        paints = 0;
        view.clearSelection();
        reference.clearSelection();
        QCoreApplication::processEvents();
        QVERIFY(paints > 0);
        QCOMPARE(view.horizontalHeader()->grab().toImage(), reference.horizontalHeader()->grab().toImage());
        for (QTableView* table : {&view, &reference}) {
            table->setSelectionBehavior(QAbstractItemView::SelectColumns);
            table->selectColumn(1);
        }
        QCoreApplication::processEvents();
        QCOMPARE(view.horizontalHeader()->grab().toImage(), reference.horizontalHeader()->grab().toImage());
    }

    // 思考表・棋譜欄とエンジン情報の表で使う
    void tablesUseCachingHeader()
    {
        IncrementalTableView view;
        QVERIFY(qobject_cast<TableHeaderView*>(view.horizontalHeader()));
        // QTableView の既定の見出しと同じ設定
        QVERIFY(view.horizontalHeader()->sectionsClickable());
        QVERIFY(view.horizontalHeader()->highlightSections());
        EngineInfoWidget info;
        auto* table = info.findChild<QTableWidget*>(QStringLiteral("engineInfoTable"));
        QVERIFY(table);
        QVERIFY(qobject_cast<TableHeaderView*>(table->horizontalHeader()));
        QCOMPARE(table->horizontalHeaderItem(0)->text(), EngineInfoWidget::tr("エンジン"));
    }
};

QTEST_MAIN(TestTableHeaderView)
#include "tst_table_header_view.moc"
