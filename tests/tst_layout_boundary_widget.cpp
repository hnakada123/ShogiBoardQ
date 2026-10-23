#include <QtTest>
#include <QDockWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QLayoutItem>
#include <QMainWindow>
#include <QMenu>
#include <QVBoxLayout>
#include <functional>
#include <tuple>

#include "dockregistry.h"
#include "branchtreeview.h"
#include "commenteditorpanel.h"
#include "considerationpanel.h"
#include "csalogpanel.h"
#include "thinkingpanel.h"
#include "usilogpanel.h"
#include "engineinfowidget.h"
#include "layoutboundarywidget.h"

/// 比べた結果が違えば、そのテストをそこで終える
#define COMPARE_LAYOUTS(reference, bounded, step)                                                  \
    do {                                                                                           \
        compareLayouts(reference, bounded, step);                                                  \
        if (QTest::currentTestFailed()) return;                                                    \
    } while (false)

/// 中身の大きさの条件が変わったときだけ親に配置のやり直しを伝える入れ物が、入れ物の無い場合と
/// 同じ配置になり、変わらないときはメインウィンドウの配置をやり直さないこと
class TestLayoutBoundaryWidget : public QObject
{
    Q_OBJECT

    /// メインウィンドウが受け取った配置のやり直しの通知を数える
    class LayoutRequestCounter : public QObject
    {
    public:
        int count = 0;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::LayoutRequest) ++count;
            return QObject::eventFilter(watched, event);
        }
    };

    /// 思考ドックに似た構成のドックを持つウィンドウ。エンジン情報の部品のように、横に並べる
    /// レイアウトの中に幅を固定した部品を置いた行が2つと、折り返す説明がある
    struct Setup {
        QMainWindow window;
        QDockWidget* dock = nullptr;
        QDockWidget* other = nullptr;
        QWidget* page = nullptr;
        QLabel* label = nullptr;
        QWidget* firstRow = nullptr;
        QWidget* secondRow = nullptr;
        QWidget* first = nullptr;   ///< firstRow の中の、幅を固定した部品
        QWidget* second = nullptr;  ///< secondRow の中の、幅を固定した部品
        LayoutRequestCounter counter;

        explicit Setup(bool boundary, bool tabified = true)
        {
            window.setCentralWidget(new QWidget(&window));
            dock = new QDockWidget(QStringLiteral("思考"), &window);
            page = new QWidget(dock);
            auto* layout = new QVBoxLayout(page);
            label = new QLabel(QStringLiteral("折り返して表示する説明の文。幅が狭いと複数の行になる。"), page);
            label->setWordWrap(true);
            const auto addRow = [this](QWidget** row, QWidget** part, int width) {
                *row = new QWidget(page);
                auto* rowLayout = new QHBoxLayout(*row);
                rowLayout->setContentsMargins(0, 0, 0, 0);
                *part = new QWidget(*row);
                (*part)->setFixedSize(width, 40);
                rowLayout->addWidget(*part);
                rowLayout->addStretch();
            };
            addRow(&firstRow, &first, 300);
            addRow(&secondRow, &second, 200);
            layout->addWidget(label);
            layout->addWidget(firstRow);
            layout->addWidget(secondRow);
            layout->addStretch();
            dock->setWidget(boundary ? new LayoutBoundaryWidget(page, dock) : page);
            window.addDockWidget(Qt::BottomDockWidgetArea, dock);
            other = new QDockWidget(QStringLiteral("棋譜"), &window);
            other->setWidget(new QLabel(QStringLiteral("棋譜"), other));
            window.addDockWidget(Qt::BottomDockWidgetArea, other);
            if (tabified) {
                window.tabifyDockWidget(dock, other);
                dock->raise();
            } else {
                window.splitDockWidget(dock, other, Qt::Horizontal);
            }
            window.resize(900, 600);
            window.installEventFilter(&counter);
        }
    };

    /// 親のレイアウトが読む、ページの大きさの条件
    using Constraints = std::tuple<QSize, QSize, QSize, QSize, QSizePolicy, bool>;
    static Constraints constraintsOf(const QWidget* page)
    {
        return {page->sizeHint(), page->minimumSizeHint(), page->minimumSize(), page->maximumSize(),
                page->sizePolicy(), page->hasHeightForWidth()};
    }

    /// 届いている配置のやり直しをすべて済ませる（通知の処理中に次の通知が届く）
    static void settle()
    {
        for (int i = 0; i < 8; ++i) QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QCoreApplication::processEvents();
    }

    /// 入れ物の有無で、ドックと中身の大きさの条件と位置が同じこと
    static void compareLayouts(const Setup& reference, const Setup& bounded, const char* step)
    {
        const auto message = [step](const char* what) { return QByteArray(step) + ": " + what; };
        QVERIFY2(reference.dock->minimumSizeHint() == bounded.dock->minimumSizeHint(), message("minimumSizeHint"));
        QVERIFY2(reference.dock->sizeHint() == bounded.dock->sizeHint(), message("sizeHint"));
        QVERIFY2(reference.dock->minimumSize() == bounded.dock->minimumSize(), message("minimumSize"));
        QVERIFY2(reference.dock->maximumSize() == bounded.dock->maximumSize(), message("maximumSize"));
        QVERIFY2(reference.window.minimumSizeHint() == bounded.window.minimumSizeHint(), message("window minimumSizeHint"));
        QVERIFY2(reference.dock->geometry() == bounded.dock->geometry(), message("dock geometry"));
        QVERIFY2(reference.page->size() == bounded.page->size(), message("page size"));
        QVERIFY2(reference.page->mapTo(reference.dock, QPoint()) == bounded.page->mapTo(bounded.dock, QPoint()),
                 message("page position"));
        const QList<QPair<QWidget*, QWidget*>> children = {
            {reference.label, bounded.label}, {reference.firstRow, bounded.firstRow},
            {reference.secondRow, bounded.secondRow}, {reference.first, bounded.first},
            {reference.second, bounded.second}};
        for (const auto& [a, b] : children) QVERIFY2(a->geometry() == b->geometry(), message("child geometry"));
        // レイアウトに置いたときの見え方が、中身そのものと同じ
        const QWidgetItem content(reference.page);
        const QWidgetItem container(bounded.dock->widget());
        QVERIFY2(content.sizeHint() == container.sizeHint(), message("item sizeHint"));
        QVERIFY2(content.minimumSize() == container.minimumSize(), message("item minimumSize"));
        QVERIFY2(content.maximumSize() == container.maximumSize(), message("item maximumSize"));
        QVERIFY2(content.expandingDirections() == container.expandingDirections(), message("item expandingDirections"));
        QVERIFY2(content.hasHeightForWidth() == container.hasHeightForWidth(), message("item hasHeightForWidth"));
        if (content.hasHeightForWidth()) {
            for (int width : {200, 400, 800})
                QVERIFY2(content.heightForWidth(width) == container.heightForWidth(width), message("item heightForWidth"));
        }
    }

private slots:
    void layoutMatchesUnboundedDock()
    {
        Setup reference(false);
        Setup bounded(true);
        for (Setup* setup : {&reference, &bounded}) {
            setup->window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&setup->window));
        }
        settle();
        COMPARE_LAYOUTS(reference, bounded, "initial");

        // 1つずつ変えて、ドックの大きさの条件が変わるかと、メインウィンドウへの通知を確かめる
        struct Step {
            const char* name;
            std::function<void(Setup&)> apply;
        };
        const QList<Step> steps = {
            {"narrower part shrinks", [](Setup& s) { s.second->setFixedWidth(150); }},
            {"narrower part grows below wider", [](Setup& s) { s.second->setFixedWidth(280); }},
            {"narrower part becomes wider", [](Setup& s) { s.second->setFixedWidth(420); }},
            {"former wider part changes below", [](Setup& s) { s.first->setFixedWidth(330); }},
            {"wider part shrinks", [](Setup& s) { s.second->setFixedWidth(310); }},
            {"part height", [](Setup& s) { s.first->setFixedHeight(90); }},
            {"description text", [](Setup& s) { s.label->setText(QStringLiteral("短い説明")); }},
            {"description font", [](Setup& s) {
                QFont font = s.label->font();
                font.setPointSize(font.pointSize() + 6);
                s.label->setFont(font);
            }},
            {"page minimum size", [](Setup& s) { s.page->setMinimumSize(500, 260); }},
            {"page maximum height", [](Setup& s) { s.page->setMaximumHeight(400); }},
            {"page size policy", [](Setup& s) {
                s.page->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Preferred);
            }},
            {"page size policy back", [](Setup& s) {
                s.page->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
            }},
        };
        int changedSteps = 0;
        int unchangedSteps = 0;
        for (const Step& step : steps) {
            reference.counter.count = 0;
            bounded.counter.count = 0;
            const Constraints before = constraintsOf(reference.page);
            for (Setup* setup : {&reference, &bounded}) step.apply(*setup);
            settle();
            COMPARE_LAYOUTS(reference, bounded, step.name);
            QVERIFY2(reference.counter.count > 0, step.name);  // 入れ物が無ければ、変わらなくても通知が届く
            // 中身の大きさの条件が変わったときだけ、入れ物の無い場合と同じ回数だけメインウィンドウまで伝わる
            if (constraintsOf(reference.page) != before) {
                ++changedSteps;
                QVERIFY2(bounded.counter.count == reference.counter.count, step.name);
            } else {
                ++unchangedSteps;
                QVERIFY2(bounded.counter.count == 0, step.name);
            }
        }
        QVERIFY(changedSteps >= 5);
        QVERIFY(unchangedSteps >= 3);

        // ドックを閉じている間に中身が変わっても、表示したときに伝わる
        for (Setup* setup : {&reference, &bounded}) {
            setup->dock->hide();
            settle();
            setup->first->setFixedWidth(700);
            settle();
            setup->dock->show();
            setup->dock->raise();
        }
        settle();
        COMPARE_LAYOUTS(reference, bounded, "changed while hidden");
        // 中身のレイアウトを組み直さない変更は、表示したときにも中身から通知が来ない
        for (Setup* setup : {&reference, &bounded}) {
            setup->dock->hide();
            settle();
            setup->page->setMinimumSize(760, 300);
            settle();
            setup->dock->show();
            setup->dock->raise();
        }
        settle();
        COMPARE_LAYOUTS(reference, bounded, "minimum size changed while hidden");

        // 切り離しても、戻しても同じ
        for (Setup* setup : {&reference, &bounded}) setup->dock->setFloating(true);
        settle();
        QCOMPARE(bounded.dock->size(), reference.dock->size());
        QCOMPARE(bounded.page->size(), reference.page->size());
        for (Setup* setup : {&reference, &bounded}) setup->first->setFixedWidth(820);
        settle();
        QCOMPARE(bounded.dock->minimumSizeHint(), reference.dock->minimumSizeHint());
        QCOMPARE(bounded.dock->size(), reference.dock->size());
        QCOMPARE(bounded.page->size(), reference.page->size());
        for (Setup* setup : {&reference, &bounded}) setup->dock->setFloating(false);
        settle();
        COMPARE_LAYOUTS(reference, bounded, "docked again");

        // ウィンドウの大きさを変えても、中身は入れ物いっぱいに置かれる
        for (Setup* setup : {&reference, &bounded}) setup->window.resize(1100, 700);
        settle();
        COMPARE_LAYOUTS(reference, bounded, "window resized");
    }

    /// 処理待ちの配置のやり直しが残っている途中でも、ドックが見る大きさの条件が入れ物の無い場合と
    /// 同じこと（中身の幅が今の幅で決まる部品があると、伝わる時点の違いで落ち着く大きさが変わる）
    void transientStatesMatchUnboundedDock()
    {
        Setup reference(false);
        Setup bounded(true);
        for (Setup* setup : {&reference, &bounded}) {
            setup->window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&setup->window));
        }
        settle();
        const auto sameNow = [&]() {
            return reference.dock->minimumSizeHint() == bounded.dock->minimumSizeHint()
                && reference.dock->sizeHint() == bounded.dock->sizeHint()
                && reference.dock->minimumSize() == bounded.dock->minimumSize()
                && reference.dock->size() == bounded.dock->size();
        };
        // 処理待ちのまま、切り離して縮め、戻す。各時点でイベントを処理せずに比べる
        for (Setup* setup : {&reference, &bounded}) setup->second->setFixedWidth(520);
        QVERIFY2(sameNow(), "width changed");
        for (Setup* setup : {&reference, &bounded}) setup->dock->setFloating(true);
        QVERIFY2(sameNow(), "floated");
        for (Setup* setup : {&reference, &bounded}) setup->dock->resize(400, 300);
        QVERIFY2(sameNow(), "resized");
        QCoreApplication::sendPostedEvents(nullptr, QEvent::LayoutRequest);
        QVERIFY2(sameNow(), "one round");
        settle();
        QVERIFY2(sameNow(), "settled");
        for (Setup* setup : {&reference, &bounded}) {
            setup->first->setFixedWidth(560);
            setup->dock->setFloating(false);
        }
        QVERIFY2(sameNow(), "docked again");
        settle();
        COMPARE_LAYOUTS(reference, bounded, "docked again");
    }

    /// 横に並べたドックの間で幅を分けるときも、入れ物の無い場合と同じこと
    void splitDocksMatchUnboundedDock()
    {
        Setup reference(false, false);
        Setup bounded(true, false);
        for (Setup* setup : {&reference, &bounded}) {
            setup->window.show();
            QVERIFY(QTest::qWaitForWindowExposed(&setup->window));
        }
        settle();
        COMPARE_LAYOUTS(reference, bounded, "initial");
        for (const QSize& size : {QSize(1300, 700), QSize(1000, 900), QSize(1500, 600)}) {
            for (Setup* setup : {&reference, &bounded}) setup->window.resize(size);
            settle();
            COMPARE_LAYOUTS(reference, bounded, "window resized");
            QCOMPARE(bounded.other->geometry(), reference.other->geometry());
        }
    }

    void thinkingAndConsiderationDocksUseBoundary()
    {
        QMainWindow window;
        window.setCentralWidget(new QWidget(&window));
        QMenu menu;
        ThinkingPanel thinking(&window);
        ConsiderationPanel consideration;
        UsiLogPanel usiLog;
        CsaLogPanel csaLog;
        CommentEditorPanel comment;
        BranchTreeView branchTree;
        using Id = DockRegistry::DockId;
        DockRegistry::Contents contents{};
        const auto put = [&contents](Id id, QWidget* content) { contents.at(static_cast<std::size_t>(id)) = content; };
        put(Id::Thinking, &thinking);
        put(Id::Consideration, &consideration);
        put(Id::UsiLog, &usiLog);
        put(Id::CsaLog, &csaLog);
        put(Id::Comment, &comment);
        put(Id::BranchTree, &branchTree);
        DockRegistry registry(&window, &menu);
        registry.createAll(contents);
        for (QDockWidget* dock : {registry.dock(Id::Thinking), registry.dock(Id::Consideration)}) {
            QVERIFY(dock);
            auto* boundary = qobject_cast<LayoutBoundaryWidget*>(dock->widget());
            QVERIFY2(boundary, qPrintable(dock->objectName()));
            QVERIFY(boundary->content());
            QVERIFY(!boundary->content()->findChildren<EngineInfoWidget*>().isEmpty());
        }
        // ほかのドックの中身は、そのままドックに置く
        QVERIFY(!qobject_cast<LayoutBoundaryWidget*>(registry.dock(Id::UsiLog)->widget()));
    }
};

QTEST_MAIN(TestLayoutBoundaryWidget)
#include "tst_layout_boundary_widget.moc"
