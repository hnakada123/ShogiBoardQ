#include <QtTest>
#include <QHeaderView>
#include <QPaintEvent>
#include <QScreen>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QWindow>
#include <cmath>
#include <functional>

#include "incrementaltableview.h"
#include "kifudisplay.h"
#include "kifurecordlistmodel.h"
#include "numericrightaligncommadelegate.h"
#include "pvboardbuttondelegate.h"
#include "pvtextdelegate.h"
#include "recordhighlightdelegate.h"
#include "recordpaneappearancemanager.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "tablestyles.h"

/// 行の追加で変わった所だけを描き直す表（棋譜欄・思考表）が、全体を描き直したときと
/// 同じ画面になること。整数倍率では描き直す範囲が追加した行の付近に収まること。
/// 小数倍率（QT_SCALE_FACTOR=1.25 で同じテストを実行）では全体を描き直す経路を使う。
class TestIncrementalTableView : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    /// 表示領域が受け取った描き直しの範囲を記録する
    class PaintSpy : public QObject
    {
    public:
        QRegion region;

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Paint) region += static_cast<QPaintEvent*>(event)->region();
            return QObject::eventFilter(watched, event);
        }
    };

    static bool integralScale(const QWidget& widget)
    {
        const qreal ratio = widget.devicePixelRatioF();
        return std::abs(ratio - std::round(ratio)) < 1e-9;
    }

    static void settle()
    {
        for (int i = 0; i < 3; ++i) QCoreApplication::processEvents();
    }

    static QImage screenImage(QWidget& widget)
    {
        QWidget* window = widget.window();
        return window->windowHandle()->screen()->grabWindow(window->winId()).toImage()
            .convertToFormat(QImage::Format_ARGB32);
    }

    /// 画面に出ている表と、表全体を描き直した表とで違う画素の数。
    /// 部分描画での半透明の合成は、全体を描いたときと丸めが1だけ違うことがあるため、それは数えない。
    static int stalePixels(QTableView& view)
    {
        const QImage partial = screenImage(view);
        view.viewport()->repaint();
        view.repaint();
        QCoreApplication::processEvents();
        const QImage full = screenImage(view);
        if (partial.size() != full.size()) return -1;
        int stale = 0;
        for (int y = 0; y < full.height(); ++y) {
            const auto* a = reinterpret_cast<const QRgb*>(partial.constScanLine(y));
            const auto* b = reinterpret_cast<const QRgb*>(full.constScanLine(y));
            for (int x = 0; x < full.width(); ++x) {
                if (qAbs(qRed(a[x]) - qRed(b[x])) > 1 || qAbs(qGreen(a[x]) - qGreen(b[x])) > 1
                    || qAbs(qBlue(a[x]) - qBlue(b[x])) > 1)
                    ++stale;
            }
        }
        return stale;
    }

    static qint64 area(const QRegion& region)
    {
        qint64 total = 0;
        for (const QRect& rect : region) total += qint64(rect.width()) * rect.height();
        return total;
    }

    /// 操作の後で、描き残しが無いこと。整数倍率では描き直した範囲が表示領域の maxFraction 未満であること
    static void verifyStep(QTableView& view, const std::function<void()>& step, double maxFraction,
                           const QString& label)
    {
        PaintSpy spy;
        view.viewport()->installEventFilter(&spy);
        step();
        settle();
        view.viewport()->removeEventFilter(&spy);
        if (integralScale(view)) {
            const qint64 whole = qint64(view.viewport()->width()) * view.viewport()->height();
            QVERIFY2(double(area(spy.region)) < double(whole) * maxFraction,
                     qPrintable(QStringLiteral("%1: repainted %2 of %3 px").arg(label).arg(area(spy.region)).arg(whole)));
        }
        QVERIFY2(stalePixels(view) == 0, qPrintable(label));
    }

    static ShogiInfoRecord* thinkingRecord(int i)
    {
        QString pv;
        for (int m = 0; m < 6 + i % 9; ++m)
            pv += (m % 2 ? QStringLiteral("△３四歩(33)") : QStringLiteral("▲７六歩(77)"));
        return new ShogiInfoRecord(QString::number(i * 13), QStringLiteral("%1/%2").arg(i % 20).arg(i % 30),
                                   QString::number(i * 12345), QString::number((i * 37) % 900 - 300), pv);
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    // 棋譜欄: 1手ごとに末尾へ行を足し、現在行の強調を移して一番下までスクロールする
    void recordAppendRepaintsOnlyNewRows()
    {
        KifuRecordListModel model;
        IncrementalTableView view;
        view.setItemDelegate(new RecordHighlightDelegate(&view));
        view.setSelectionMode(QAbstractItemView::SingleSelection);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        view.verticalHeader()->setVisible(false);
        view.verticalHeader()->setDefaultSectionSize(view.fontMetrics().height() + 4);
        view.verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        view.setWordWrap(false);
        view.setTextElideMode(Qt::ElideRight);
        view.setStyleSheet(RecordPaneAppearanceManager::kifuTableStyleSheet(10));
        view.setModel(&model);
        view.horizontalHeader()->setStretchLastSection(true);
        view.resize(420, 260);
        // 縦のスクロールバーが出るだけの行を先に入れておく（出たときは表示領域の幅が変わり、全体を描き直す）
        model.appendItem(new KifuDisplay(QStringLiteral("=== 開始局面 ==="), QString(), &model));
        for (int ply = 1; ply <= 12; ++ply)
            model.appendItem(new KifuDisplay(QStringLiteral("%1 ▲２六歩(27)").arg(ply), QString(), &model));
        model.setCurrentHighlightRow(12);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.scrollToBottom();
        settle();
        // 整数倍率では表示領域を不透明にして、画面のずらしを使えるようにする
        QCOMPARE(view.viewport()->autoFillBackground(), integralScale(view));

        for (int ply = 13; ply <= 40; ++ply) {
            verifyStep(view, [&] {
                model.appendItem(new KifuDisplay(QStringLiteral("%1 ▲７六歩(77)").arg(ply),
                                                 QStringLiteral("00:01/00:00:%1").arg(ply, 2, 10, QLatin1Char('0')),
                                                 ply % 4 == 0 ? QStringLiteral("長いコメント").repeated(12) : QString(),
                                                 &model));
                model.setCurrentHighlightRow(ply);
                view.scrollToBottom();
            }, 0.4, QStringLiteral("append ply %1").arg(ply));
            if (QTest::currentTestFailed()) return;
        }
        // 選択した行の移動と、途中の行への手数の移動（スクロール）
        verifyStep(view, [&] { view.selectRow(35); }, 1.01, QStringLiteral("select"));
        verifyStep(view, [&] { model.setCurrentHighlightRow(3); view.scrollTo(model.index(3, 0)); }, 1.01,
                   QStringLiteral("goto"));
        // 末尾に足したが、表示は途中のまま
        verifyStep(view, [&] {
            model.appendItem(new KifuDisplay(QStringLiteral("41 ▲７六歩(77)"), QString(), &model));
        }, 0.4, QStringLiteral("append below view"));
        // 作り直し（全体を描き直す）
        verifyStep(view, [&] {
            model.clearAllItems();
            for (int i = 0; i < 5; ++i) model.appendItem(new KifuDisplay(QStringLiteral("%1").arg(i), QString(), &model));
        }, 1.01, QStringLiteral("rebuild"));
    }

    // 思考表: 思考情報が届くたびに先頭へ行を足す（1回に複数行のこともある）
    void thinkingPrependShiftsExistingRows()
    {
        ShogiEngineThinkingModel model;
        IncrementalTableView view;
        view.setModel(&model);
        view.setStyleSheet(TableStyles::thinking());
        view.setShowGrid(false);
        view.setMouseTracking(true);
        view.setItemDelegateForColumn(4, new PvBoardButtonDelegate(&view));
        view.setItemDelegateForColumn(5, new PvTextDelegate(&view));
        auto* numeric = new NumericRightAlignCommaDelegate(&view);
        for (int column = 0; column < 4; ++column) view.setItemDelegateForColumn(column, numeric);
        view.horizontalHeader()->setStretchLastSection(true);
        view.verticalHeader()->setVisible(false);
        view.verticalHeader()->setDefaultSectionSize(view.fontMetrics().height() + 4);
        view.verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        view.setWordWrap(false);
        view.resize(900, 260);
        // 読み筋の列の日本語フォントは初めて使うときに読み込まれ、表の文字設定が変わる
        // （そのときは Qt が全体を描き直す）。先に数行入れておく。
        // 縦のスクロールバーも先に出しておく（出たときは表示領域の幅が変わり、全体を描き直す）
        int next = 0;
        for (; next < 14; ++next) model.prependItem(thinkingRecord(next));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        settle();
        QCOMPARE(view.viewport()->autoFillBackground(), integralScale(view));

        for (int step = 0; step < 30; ++step) {
            const int count = step % 5 == 4 ? 2 : 1;
            verifyStep(view, [&] {
                for (int i = 0; i < count; ++i) model.prependItem(thinkingRecord(next++));
                model.trimToMaxRows(500);
            }, 0.4, QStringLiteral("prepend step %1").arg(step));
            if (QTest::currentTestFailed()) return;
        }
        // 行を選んだまま足す（選んだ行は一緒に下がる）
        verifyStep(view, [&] { view.selectRow(2); }, 1.01, QStringLiteral("select"));
        verifyStep(view, [&] { model.prependItem(thinkingRecord(next++)); }, 0.4, QStringLiteral("prepend with selection"));
        // 下へスクロールしているときは全体を描き直す
        verifyStep(view, [&] { view.verticalScrollBar()->setValue(5); }, 1.01, QStringLiteral("scroll"));
        verifyStep(view, [&] { model.prependItem(thinkingRecord(next++)); }, 1.01, QStringLiteral("prepend while scrolled"));
        verifyStep(view, [&] { view.verticalScrollBar()->setValue(0); }, 1.01, QStringLiteral("scroll back"));
        // 上限を超えた行の削除・消去は全体を描き直す
        verifyStep(view, [&] {
            model.prependItem(thinkingRecord(next++));
            model.trimToMaxRows(20);
        }, 1.01, QStringLiteral("trim"));
        verifyStep(view, [&] { model.clearAllItems(); }, 1.01, QStringLiteral("clear"));
        verifyStep(view, [&] { model.prependItem(thinkingRecord(next++)); }, 1.01, QStringLiteral("first row"));
    }

    // 同じモデルを渡し直しても、モデルを差し替えても、行の追加を正しく描く
    void modelSwitchKeepsUpdates()
    {
        KifuRecordListModel first;
        KifuRecordListModel second;
        for (KifuRecordListModel* model : {&first, &second}) {
            for (int row = 0; row < 16; ++row)
                model->appendItem(new KifuDisplay(QStringLiteral("%1 ▲２六歩(27)").arg(row), QString(), model));
        }
        IncrementalTableView view;
        view.setItemDelegate(new RecordHighlightDelegate(&view));
        view.verticalHeader()->setVisible(false);
        view.setStyleSheet(RecordPaneAppearanceManager::kifuTableStyleSheet(10));
        view.setModel(&first);
        view.resize(420, 260);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.scrollToBottom();
        settle();

        view.setModel(&first);  // 同じモデル（Qt の接続を外さないこと）
        verifyStep(view, [&] {
            first.appendItem(new KifuDisplay(QStringLiteral("16 ▲７六歩(77)"), QString(), &first));
            view.scrollToBottom();
        }, 0.4, QStringLiteral("same model"));
        QCOMPARE(view.model()->rowCount(), 17);

        view.setModel(&second);
        view.scrollToBottom();
        settle();
        verifyStep(view, [&] {
            second.appendItem(new KifuDisplay(QStringLiteral("16 ▲７六歩(77)"), QString(), &second));
            view.scrollToBottom();
        }, 0.4, QStringLiteral("switched model"));
        // 外したモデルの変更は表に影響しない
        verifyStep(view, [&] {
            first.appendItem(new KifuDisplay(QStringLiteral("17"), QString(), &first));
        }, 0.01, QStringLiteral("detached model"));
    }

    // 追加と描画クラスの変更が重なったときは、全体を描き直す（追加した行だけでは足りない）
    void settingsChangeFallsBackToFullRepaint()
    {
        ShogiEngineThinkingModel model;
        IncrementalTableView view;
        view.setModel(&model);
        view.setStyleSheet(TableStyles::thinking());
        view.verticalHeader()->setVisible(false);
        view.resize(900, 260);
        for (int i = 0; i < 8; ++i) model.prependItem(thinkingRecord(i));
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        settle();
        auto* numeric = new NumericRightAlignCommaDelegate(&view);
        PaintSpy spy;
        view.viewport()->installEventFilter(&spy);
        view.setItemDelegateForColumn(2, numeric);
        model.prependItem(thinkingRecord(100));
        settle();
        view.viewport()->removeEventFilter(&spy);
        const qint64 whole = qint64(view.viewport()->width()) * view.viewport()->height();
        QCOMPARE(area(spy.region), whole);
        QCOMPARE(stalePixels(view), 0);
    }
};

QTEST_MAIN(TestIncrementalTableView)
#include "tst_incremental_table_view.moc"
