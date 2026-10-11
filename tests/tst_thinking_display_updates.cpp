#include <QtTest>
#include <QSignalSpy>
#include <QDockWidget>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include "engineinfowidget.h"
#include "kifumovedelegate.h"
#include "numericrightaligncommadelegate.h"
#include "opaquebackground.h"
#include "pvboardbuttondelegate.h"
#include "pvtextdelegate.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "tablestyles.h"
#include "usicommlogmodel.h"

/// 思考情報が届くたびの表示の更新が、余計な描き直しやレイアウトの再計算を起こさないことを検証する。
class TestThinkingDisplayUpdates : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    /// 受け取った描画・レイアウトの再計算の要求を数える
    class EventCounter : public QObject
    {
    public:
        explicit EventCounter(QEvent::Type type) : m_type(type) {}
        int count = 0;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == m_type) ++count;
            return QObject::eventFilter(watched, event);
        }

    private:
        QEvent::Type m_type;
    };

    static QString longPv(int moves)
    {
        static const QStringList kMoves = {
            QStringLiteral("▲７六歩(77)"), QStringLiteral("△３四歩(33)"), QStringLiteral("▲２六歩(27)"),
            QStringLiteral("△８四歩(83)"), QStringLiteral("▲２五歩(26)"), QStringLiteral("△同　歩(24)"),
            QStringLiteral("▲６八銀(79)"), QStringLiteral("△３三角打"), QStringLiteral("▲２三歩成(24)")};
        QString pv;
        for (int i = 0; i < moves; ++i) pv += kMoves.at(i % kMoves.size());
        return pv;
    }

    /// 思考タブの表と同じ設定の表（読み筋の列だけ delegate を変えられる）
    static void setUpThinkingView(QTableView& view, ShogiEngineThinkingModel* model, bool pvDelegate)
    {
        view.setModel(model);
        view.setStyleSheet(TableStyles::thinking());
        view.setShowGrid(false);
        view.setItemDelegateForColumn(4, new PvBoardButtonDelegate(&view));
        auto* numeric = new NumericRightAlignCommaDelegate(&view);
        for (int column = 0; column < 4; ++column) view.setItemDelegateForColumn(column, numeric);
        if (pvDelegate) view.setItemDelegateForColumn(5, new PvTextDelegate(&view));
        view.horizontalHeader()->setStretchLastSection(true);
        view.verticalHeader()->setVisible(false);
        view.verticalHeader()->setDefaultSectionSize(KifuMoveDelegate::textHeight(view.font()) + 4);
        view.setWordWrap(false);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    // ドックの中身を不透明にしておけば、中の部品の更新でドックやメインウィンドウは描き直さない
    void updatesInsideOpaqueContentDoNotRepaintParent()
    {
        QMainWindow window;
        window.setCentralWidget(new QWidget(&window));
        auto* dock = new QDockWidget(QStringLiteral("思考"), &window);
        auto* content = new QWidget(dock);
        auto* contentLayout = new QVBoxLayout(content);
        auto* label = new QLabel(QStringLiteral("12,345"), content);
        label->setFixedSize(120, 30);
        contentLayout->addWidget(label);
        dock->setWidget(content);
        window.addDockWidget(Qt::BottomDockWidgetArea, dock);
        OpaqueBackground::keep(content);
        window.resize(400, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(content->autoFillBackground());
        for (int i = 0; i < 3; ++i) QCoreApplication::processEvents();

        EventCounter windowPaints(QEvent::Paint);
        EventCounter dockPaints(QEvent::Paint);
        EventCounter contentPaints(QEvent::Paint);
        window.installEventFilter(&windowPaints);
        dock->installEventFilter(&dockPaints);
        content->installEventFilter(&contentPaints);
        label->setText(QStringLiteral("123,456"));
        QTRY_VERIFY(contentPaints.count > 0);
        QCOMPARE(dockPaints.count, 0);
        QCOMPARE(windowPaints.count, 0);

        // 切り離したドックは枠が別の色で背景を描くので、その間は塗らない
        dock->setFloating(true);
        QTRY_VERIFY(!content->autoFillBackground());
        dock->setFloating(false);
        QTRY_VERIFY(content->autoFillBackground());
    }

    // 親と違う背景色の中身（枠付きの表示領域など）は塗らない。塗ると枠の角の色が変わる
    void contentWithDifferentBackgroundIsNotFilled()
    {
        QMainWindow window;
        auto* dock = new QDockWidget(QStringLiteral("分岐ツリー"), &window);
        auto* view = new QGraphicsView(dock);
        dock->setWidget(view);
        window.addDockWidget(Qt::BottomDockWidgetArea, dock);
        OpaqueBackground::keep(view);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(view->palette().brush(view->backgroundRole()) != dock->palette().brush(dock->backgroundRole()));
        QVERIFY(!view->autoFillBackground());
    }

    // 読み筋は表示幅より十分先で切って描く。短い文字列や改行を含む文字列はそのまま
    void pvTextIsCutBeyondVisibleWidth()
    {
        PvTextDelegate delegate;
        QFont font;
        const QString text = longPv(30);
        const int width = 200;
        const QString cut = delegate.textForWidth(text, font, width);
        QVERIFY(cut.size() < text.size());
        QVERIFY(text.startsWith(cut));
        QVERIFY(QFontMetrics(font).horizontalAdvance(cut) > width);
        // 2回目は覚えた位置を使う
        QCOMPARE(delegate.textForWidth(text, font, width), cut);
        // 幅が変われば切る位置も変わる
        QVERIFY(delegate.textForWidth(text, font, width * 2).size() > cut.size());

        const QString shortText = longPv(2);
        QCOMPARE(delegate.textForWidth(shortText, font, width), shortText);
        const QString multiLine = text + QLatin1Char('\n') + text;
        QCOMPARE(delegate.textForWidth(multiLine, font, width), multiLine);
        // 全文が表示幅に収まるなら切らない
        QCOMPARE(delegate.textForWidth(text, font, 100000), text);
    }

    // 切った文字列で描いても、省略表示（…）を含めて見た目は全文で描いた場合と同じ
    void pvTextRendersSameAsFullText()
    {
        ShogiEngineThinkingModel model;
        for (int row = 0; row < 12; ++row) {
            model.prependItem(new ShogiInfoRecord(QString::number(row * 13), QStringLiteral("%1/%2").arg(row).arg(row + 3),
                                                  QString::number(row * 12345), QString::number(row * 37 - 200),
                                                  longPv(2 + row * 3)));
        }
        for (const int pvWidth : {60, 180, 420, 900}) {
            QTableView plain;
            QTableView cut;
            setUpThinkingView(plain, &model, false);
            setUpThinkingView(cut, &model, true);
            for (QTableView* view : {&plain, &cut}) {
                view->resize(pvWidth + 520, 320);
                view->horizontalHeader()->setStretchLastSection(false);
                view->horizontalHeader()->resizeSection(5, pvWidth);
                view->selectRow(2);
                view->show();
                QVERIFY(QTest::qWaitForWindowExposed(view));
            }
            const QImage expected = plain.grab().toImage();
            const QImage actual = cut.grab().toImage();
            QCOMPARE(actual.size(), expected.size());
            QVERIFY2(actual == expected, qPrintable(QStringLiteral("PV column width %1").arg(pvWidth)));
        }
    }

    // 思考情報は1行で複数の値が変わる。値が変わらなければ表を更新せず、
    // 列幅の計算（レイアウトの再計算につながる）はまとめて1回にする
    void engineInfoUpdatesAreCoalesced()
    {
        UsiCommLogModel log;
        EngineInfoWidget info;
        info.setModel(&log);
        info.resize(900, 60);
        info.show();
        QVERIFY(QTest::qWaitForWindowExposed(&info));
        auto* table = info.findChild<QTableWidget*>(QStringLiteral("engineInfoTable"));
        QVERIFY(table != nullptr);

        log.setNodeCount(QStringLiteral("12,345"));
        QCoreApplication::processEvents();
        QSignalSpy changed(table->model(), &QAbstractItemModel::dataChanged);
        log.setNodeCount(QStringLiteral("12,345"));
        QCOMPARE(changed.count(), 0);

        // 値ごとに計算すると、表の幅を値の数だけ変える（そのたびに親のレイアウトを計算し直す）
        EventCounter resizes(QEvent::Resize);
        table->installEventFilter(&resizes);
        const int widthBefore = table->width();
        log.setSearchedMove(QStringLiteral("▲２三歩成(24)"));
        log.setSearchDepth(QStringLiteral("12/18"));
        log.setNodeCount(QStringLiteral("123,456,789"));
        log.setNodesPerSecond(QStringLiteral("1,234,567"));
        log.setHashUsage(QStringLiteral("12.5%"));
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(table->width() > widthBefore);
        QVERIFY2(resizes.count == 1, qPrintable(QStringLiteral("table resized %1 times").arg(resizes.count)));
        // 列幅は内容に合わせて広がっている
        const QFontMetrics metrics(table->font());
        QVERIFY(table->columnWidth(4) >= metrics.horizontalAdvance(QStringLiteral("123,456,789")));
    }
};

QTEST_MAIN(TestThinkingDisplayUpdates)
#include "tst_thinking_display_updates.moc"
