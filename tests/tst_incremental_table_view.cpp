#include <QtTest>
#include <QHeaderView>
#include <QPaintEvent>
#include <QScreen>
#include <QScrollBar>
#include <QTemporaryDir>
#include <QWindow>
#include <cmath>
#include <functional>
#include <limits>

#include "analysissettings.h"
#include "thinkingtableview.h"
#include "header_drag_helper.h"
#include "incrementaltableview.h"
#include "kifurecordlistmodel.h"
#include "record_rows_helper.h"
#include "numericrightaligncommadelegate.h"
#include "tablebuttondelegate.h"
#include "pvtextdelegate.h"
#include "recordhighlightdelegate.h"
#include "recordpane.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "tablestyles.h"

/// 行の追加で変わった所だけを描き直す表（棋譜欄・思考表）が、全体を描き直したときと
/// 同じ画面になること。整数倍率では描き直す範囲が追加した行の付近に収まること。
/// 小数倍率（QT_SCALE_FACTOR=1.25 で同じテストを実行）では全体を描き直す経路を使う。
/// 思考の表に足すときに、古い行を消さない
constexpr int kUnlimitedRows = std::numeric_limits<int>::max();

class TestIncrementalTableView : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    /// 表示領域が受け取った描き直しの範囲を記録する
    class PaintSpy : public QObject
    {
    public:
        QRegion region;
        int fullPaints = 0;  ///< 表示領域全体を描いた回数

    protected:
        bool eventFilter(QObject* watched, QEvent* event) override
        {
            if (event->type() == QEvent::Paint) {
                const QRegion& painted = static_cast<QPaintEvent*>(event)->region();
                region += painted;
                if ((QRegion(static_cast<QWidget*>(watched)->rect()) - painted).isEmpty()) ++fullPaints;
            }
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

    /// 思考表と同じ設定にする
    static void setUpThinkingView(QTableView& view, QAbstractItemModel* model)
    {
        view.setModel(model);
        view.setStyleSheet(TableStyles::thinking());
        view.setShowGrid(false);
        view.setMouseTracking(true);
        view.setItemDelegateForColumn(ShogiEngineThinkingModel::BoardColumn, new TableButtonDelegate(&view));
        view.setItemDelegateForColumn(ShogiEngineThinkingModel::PvColumn, new PvTextDelegate(&view));
        auto* numeric = new NumericRightAlignCommaDelegate(&view);
        for (int column = 0; column < 4; ++column) view.setItemDelegateForColumn(column, numeric);
        view.horizontalHeader()->setStretchLastSection(true);
        view.verticalHeader()->setVisible(false);
        view.verticalHeader()->setDefaultSectionSize(view.fontMetrics().height() + 4);
        view.verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        view.setWordWrap(false);
    }

    /// 列幅・表示領域・スクロールバーが Qt の表と同じで、画面に描き残しが無いこと
    static void verifySameLayout(QTableView& view, QTableView& reference, const QString& label)
    {
        QVERIFY2(view.viewport()->size() == reference.viewport()->size(), qPrintable(label));
        for (int column = 0; column < view.model()->columnCount(); ++column)
            QVERIFY2(view.columnWidth(column) == reference.columnWidth(column), qPrintable(label));
        for (QScrollBar* bar : {view.horizontalScrollBar(), view.verticalScrollBar()}) {
            QScrollBar* other = bar == view.horizontalScrollBar() ? reference.horizontalScrollBar()
                                                                   : reference.verticalScrollBar();
            QVERIFY2(bar->isVisible() == other->isVisible() && bar->maximum() == other->maximum()
                         && bar->pageStep() == other->pageStep(),
                     qPrintable(label));
        }
        QVERIFY2(stalePixels(view) == 0, qPrintable(label));
    }

    static ThinkingInfoRow thinkingRecord(int i)
    {
        QString pv;
        for (int m = 0; m < 6 + i % 9; ++m)
            pv += (m % 2 ? QStringLiteral("△３四歩(33)") : QStringLiteral("▲７六歩(77)"));
        ThinkingInfoRow row;
        row.timeMs = i * 13;
        row.depth = i % 20;
        row.nodes = qint64(i) * 12345;
        row.score = UsiScore::cp((i * 37) % 900 - 300);
        // 開始局面の無い読み筋は、エンジンが送った語のまま表示する
        row.pv = PvText::play(QString(), QStringList{pv});
        return row;
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
        RecordRows rows;
        KifuRecordListModel& model = rows.model;
        IncrementalTableView view;
        view.setItemDelegate(new RecordHighlightDelegate(&view));
        view.setSelectionMode(QAbstractItemView::SingleSelection);
        view.setSelectionBehavior(QAbstractItemView::SelectRows);
        view.verticalHeader()->setVisible(false);
        view.verticalHeader()->setDefaultSectionSize(view.fontMetrics().height() + 4);
        view.verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
        view.setWordWrap(false);
        view.setTextElideMode(Qt::ElideRight);
        view.setStyleSheet(RecordPane::tableStyleSheet(10));
        view.setModel(&model);
        view.horizontalHeader()->setStretchLastSection(true);
        view.resize(420, 260);
        // 縦のスクロールバーが出るだけの行を先に入れておく（出たときは表示領域の幅が変わり、全体を描き直す）
        for (int ply = 1; ply <= 12; ++ply) rows.append(QStringLiteral("▲２六歩(27)"));
        model.setCurrentHighlightRow(12);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.scrollToBottom();
        settle();
        // 整数倍率では表示領域を不透明にして、画面のずらしを使えるようにする
        QCOMPARE(view.viewport()->autoFillBackground(), integralScale(view));

        for (int ply = 13; ply <= 40; ++ply) {
            verifyStep(view, [&] {
                rows.append(QStringLiteral("▲７六歩(77)"),
                            QStringLiteral("00:01/00:00:%1").arg(ply, 2, 10, QLatin1Char('0')),
                            ply % 4 == 0 ? QStringLiteral("長いコメント").repeated(12) : QString());
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
            rows.append(QStringLiteral("▲７六歩(77)"));
        }, 0.4, QStringLiteral("append below view"));
        // 作り直し（全体を描き直す）
        verifyStep(view, [&] {
            rows.reset();
            for (int i = 0; i < 4; ++i) rows.append(QStringLiteral("▲%1").arg(i));
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
        view.setItemDelegateForColumn(ShogiEngineThinkingModel::BoardColumn, new TableButtonDelegate(&view));
        view.setItemDelegateForColumn(ShogiEngineThinkingModel::PvColumn, new PvTextDelegate(&view));
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
        for (; next < 14; ++next) model.prependRow(thinkingRecord(next), kUnlimitedRows);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        settle();
        QCOMPARE(view.viewport()->autoFillBackground(), integralScale(view));

        for (int step = 0; step < 30; ++step) {
            const int count = step % 5 == 4 ? 2 : 1;
            verifyStep(view, [&] {
                for (int i = 0; i < count; ++i) model.prependRow(thinkingRecord(next++), kUnlimitedRows);
                model.trimToMaxRows(500);
            }, 0.4, QStringLiteral("prepend step %1").arg(step));
            if (QTest::currentTestFailed()) return;
        }
        // 行を選んだまま足す（選んだ行は一緒に下がる）
        verifyStep(view, [&] { view.selectRow(2); }, 1.01, QStringLiteral("select"));
        verifyStep(view, [&] { model.prependRow(thinkingRecord(next++), kUnlimitedRows); }, 0.4, QStringLiteral("prepend with selection"));
        // 下へスクロールしているときは全体を描き直す
        verifyStep(view, [&] { view.verticalScrollBar()->setValue(5); }, 1.01, QStringLiteral("scroll"));
        verifyStep(view, [&] { model.prependRow(thinkingRecord(next++), kUnlimitedRows); }, 1.01, QStringLiteral("prepend while scrolled"));
        verifyStep(view, [&] { view.verticalScrollBar()->setValue(0); }, 1.01, QStringLiteral("scroll back"));
        // 上限を超えた行の削除: 消える行が表示領域より下なら、足した行の分だけを描き直す
        QVERIFY(view.rowViewportPosition(20) >= view.viewport()->height());
        verifyStep(view, [&] {
            model.prependRow(thinkingRecord(next++), kUnlimitedRows);
            model.trimToMaxRows(20);
        }, 0.4, QStringLiteral("trim below view"));
        for (int step = 0; step < 5; ++step) {
            verifyStep(view, [&] {
                model.prependRow(thinkingRecord(next++), kUnlimitedRows);
                model.prependRow(thinkingRecord(next++), kUnlimitedRows);
                model.trimToMaxRows(20);
            }, 0.4, QStringLiteral("prepend and trim step %1").arg(step));
            if (QTest::currentTestFailed()) return;
        }
        QCOMPARE(model.rowCount(), 20);
        // 削除だけ（表示領域より下）は何も描き直さない
        {
            PaintSpy spy;
            view.viewport()->installEventFilter(&spy);
            model.trimToMaxRows(18);
            settle();
            view.viewport()->removeEventFilter(&spy);
            if (integralScale(view)) QVERIFY2(spy.region.isEmpty(), "remove below view");
            QVERIFY(stalePixels(view) == 0);
        }
        // 消える行が見えているときと、下へスクロールしているときは全体を描き直す
        verifyStep(view, [&] {
            model.prependRow(thinkingRecord(next++), kUnlimitedRows);
            model.trimToMaxRows(4);
        }, 1.01, QStringLiteral("trim visible rows"));
        for (int i = 0; i < 20; ++i) model.prependRow(thinkingRecord(next++), kUnlimitedRows);
        settle();
        verifyStep(view, [&] { view.verticalScrollBar()->setValue(3); }, 1.01, QStringLiteral("scroll before trim"));
        verifyStep(view, [&] { model.trimToMaxRows(22); }, 1.01, QStringLiteral("trim while scrolled"));
        verifyStep(view, [&] { view.verticalScrollBar()->setValue(0); }, 1.01, QStringLiteral("scroll back before clear"));
        verifyStep(view, [&] { model.clear(); }, 1.01, QStringLiteral("clear"));
        verifyStep(view, [&] { model.prependRow(thinkingRecord(next++), kUnlimitedRows); }, 1.01, QStringLiteral("first row"));
    }

    // 思考表は手ごとに消して足し直すので、縦のスクロールバーが出入りし、表示領域の幅と
    // 最後の列の幅が変わる。そのとき表示領域全体を描くのは1回だけで、列幅・スクロールバー・
    // 画面は、Qt の表と同じになる。
    void scrollBarToggleRepaintsViewportOnce()
    {
        ShogiEngineThinkingModel model;
        ShogiEngineThinkingModel referenceModel;
        QWidget window;
        IncrementalTableView view(&window);
        QTableView reference(&window);
        setUpThinkingView(view, &model);
        setUpThinkingView(reference, &referenceModel);
        view.setGeometry(0, 0, 900, 130);
        reference.setGeometry(0, 140, 900, 130);
        window.resize(900, 270);
        int next = 0;
        model.prependRow(thinkingRecord(next), kUnlimitedRows);
        referenceModel.prependRow(thinkingRecord(next++), kUnlimitedRows);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();

        for (int cycle = 0; cycle < 3; ++cycle) {
            for (const bool fill : {true, false}) {
                PaintSpy spy;
                view.viewport()->installEventFilter(&spy);
                for (ShogiEngineThinkingModel* target : {&model, &referenceModel}) {
                    if (fill) {
                        for (int i = 0; i < 8; ++i) target->prependRow(thinkingRecord(next + i), kUnlimitedRows);
                    } else {
                        target->clear();
                    }
                }
                if (fill) next += 8;
                settle();
                QTest::qWait(20);  // Qt の列幅の処理（0ms のタイマー）を待つ
                settle();
                view.viewport()->removeEventFilter(&spy);
                const QString label = QStringLiteral("cycle %1 %2").arg(cycle).arg(fill ? "fill" : "clear");
                QCOMPARE(view.verticalScrollBar()->isVisible(), fill);
                // 消すときは、行を消した表を描いた後でスクロールバーが消えて幅が変わるので、
                // 全体を2回描く（中身が無いので軽い）。足すときは幅が変わってから描くので1回。
                QVERIFY2(spy.fullPaints == (fill ? 1 : 2),
                         qPrintable(label + QStringLiteral(": full repaints %1").arg(spy.fullPaints)));
                verifySameLayout(view, reference, label);
                if (QTest::currentTestFailed()) return;
            }
        }

        // 表の幅を変える（ドックのリサイズ）。狭くすると横のスクロールバーも出る
        for (int width : {700, 420, 300, 260, 640, 900}) {
            PaintSpy spy;
            view.viewport()->installEventFilter(&spy);
            view.resize(width, view.height());
            reference.resize(width, reference.height());
            settle();
            QTest::qWait(20);
            settle();
            view.viewport()->removeEventFilter(&spy);
            const QString label = QStringLiteral("width %1").arg(width);
            QVERIFY2(spy.fullPaints <= 2, qPrintable(label + QStringLiteral(": full repaints %1").arg(spy.fullPaints)));
            verifySameLayout(view, reference, label);
            if (QTest::currentTestFailed()) return;
            if (width == 260) QVERIFY(view.horizontalScrollBar()->isVisible());
        }
        QVERIFY(!view.horizontalScrollBar()->isVisible());
    }

    // アプリの思考表は縦のスクロールバーを常に出しておく。思考を始めるたびに表を消して
    // 1行ずつ足しても、表示領域と最後の列の幅は変わらず、表示領域全体と見出しを描き直さない
    // （スクロールバーが現れるたびに、幅が変わって両方を描き直していた）。
    void thinkingViewKeepsWidthWhileFilling()
    {
        ShogiEngineThinkingModel model1;
        ShogiEngineThinkingModel model2;
        QWidget window;
        ThinkingTableView view1(AnalysisSettings::ThinkingTable::Engine1, &window);
        ThinkingTableView view2(AnalysisSettings::ThinkingTable::Engine2, &window);
        view1.setThinkingModel(&model1);
        view2.setThinkingModel(&model2);
        view1.setGeometry(0, 0, 900, 130);
        view2.setGeometry(0, 140, 900, 130);
        window.resize(900, 270);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
        // 読み筋の列の日本語フォントは初めて使うときに読み込まれ、表の文字設定が変わる
        // （そのときは Qt が全体を描き直す）。先に1行入れて消しておく。
        model1.prependRow(thinkingRecord(0), kUnlimitedRows);
        settle();
        model1.clear();
        settle();
        QVERIFY(view1.verticalScrollBar()->isVisible());
        const int viewportWidth = view1.viewport()->width();
        const int lastWidth = view1.columnWidth(ShogiEngineThinkingModel::PvColumn);

        int next = 1;
        for (int cycle = 0; cycle < 2; ++cycle) {
            for (int row = 0; row < 8; ++row) {
                PaintSpy viewportSpy;
                PaintSpy headerSpy;
                view1.viewport()->installEventFilter(&viewportSpy);
                view1.horizontalHeader()->viewport()->installEventFilter(&headerSpy);
                model1.prependRow(thinkingRecord(next++), kUnlimitedRows);
                settle();
                view1.viewport()->removeEventFilter(&viewportSpy);
                view1.horizontalHeader()->viewport()->removeEventFilter(&headerSpy);
                const QString label = QStringLiteral("cycle %1 row %2").arg(cycle).arg(row);
                QVERIFY2(view1.verticalScrollBar()->isVisible(), qPrintable(label));
                QVERIFY2(view1.viewport()->width() == viewportWidth, qPrintable(label));
                QVERIFY2(view1.columnWidth(ShogiEngineThinkingModel::PvColumn) == lastWidth, qPrintable(label));
                // 小数倍率では、行の追加は元から全体の描き直しになる（画面のずらしを使えない）
                if (integralScale(view1)) {
                    QVERIFY2(viewportSpy.fullPaints == 0,
                             qPrintable(label + QStringLiteral(": full repaints %1").arg(viewportSpy.fullPaints)));
                }
                QVERIFY2(headerSpy.region.isEmpty(), qPrintable(label + QStringLiteral(": header repainted")));
                QVERIFY2(stalePixels(view1) == 0, qPrintable(label));
            }
            model1.clear();
            settle();
            QVERIFY(view1.verticalScrollBar()->isVisible());
            QCOMPARE(view1.columnWidth(ShogiEngineThinkingModel::PvColumn), lastWidth);
        }
    }

    // 思考表の列幅は、最後の列が表示領域に合わせて伸び縮みしただけでは保存しない
    // （縦のスクロールバーが出入りするたびや、表の幅が変わるたびに設定ファイルへ書いていた）
    void stretchedColumnWidthIsNotSaved()
    {
        ShogiEngineThinkingModel model1;
        ShogiEngineThinkingModel model2;
        QWidget window;
        ThinkingTableView view1(AnalysisSettings::ThinkingTable::Engine1, &window);
        ThinkingTableView view2(AnalysisSettings::ThinkingTable::Engine2, &window);
        view1.setThinkingModel(&model1);
        view2.setThinkingModel(&model2);
        view1.setGeometry(0, 0, 900, 130);
        view2.setGeometry(0, 140, 900, 130);
        window.resize(900, 270);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        settle();
        const QList<int> saved = AnalysisSettings::thinkingViewColumnWidths(AnalysisSettings::ThinkingTable::Engine1);

        const int lastWidth = view1.columnWidth(ShogiEngineThinkingModel::PvColumn);
        for (int i = 0; i < 8; ++i) model1.prependRow(thinkingRecord(i), kUnlimitedRows);
        settle();
        model1.clear();
        settle();
        view1.resize(700, 130);
        settle();
        QVERIFY(view1.columnWidth(ShogiEngineThinkingModel::PvColumn) != lastWidth);
        view1.resize(820, 130);
        settle();
        QCOMPARE(AnalysisSettings::thinkingViewColumnWidths(AnalysisSettings::ThinkingTable::Engine1), saved);

        // プログラムで列幅を変えても（文字の大きさの変更など）保存しない
        view1.setColumnWidth(2, 140);
        settle();
        QCOMPARE(AnalysisSettings::thinkingViewColumnWidths(AnalysisSettings::ThinkingTable::Engine1), saved);

        // 利用者が見出しの境目をドラッグして列幅を変えたときは、すべての列の幅を保存する
        HeaderDrag::resizeSection(view1.horizontalHeader(), 2, 123);
        settle();
        const QList<int> changed = AnalysisSettings::thinkingViewColumnWidths(AnalysisSettings::ThinkingTable::Engine1);
        QCOMPARE(changed.size(), model1.columnCount());
        QCOMPARE(changed.at(2), 123);

        // 次に作る表は、保存した幅で始まる
        ThinkingTableView restored(AnalysisSettings::ThinkingTable::Engine1);
        restored.setThinkingModel(&model1);
        QCOMPARE(restored.columnWidth(2), 123);
    }

    // 同じモデルを渡し直しても、モデルを差し替えても、行の追加を正しく描く
    void modelSwitchKeepsUpdates()
    {
        RecordRows firstRows;
        RecordRows secondRows;
        KifuRecordListModel& first = firstRows.model;
        KifuRecordListModel& second = secondRows.model;
        for (RecordRows* rows : {&firstRows, &secondRows}) {
            for (int row = 1; row < 16; ++row) rows->append(QStringLiteral("▲２六歩(27)"));
        }
        IncrementalTableView view;
        view.setItemDelegate(new RecordHighlightDelegate(&view));
        view.verticalHeader()->setVisible(false);
        view.setStyleSheet(RecordPane::tableStyleSheet(10));
        view.setModel(&first);
        view.resize(420, 260);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.scrollToBottom();
        settle();

        view.setModel(&first);  // 同じモデル（Qt の接続を外さないこと）
        verifyStep(view, [&] {
            firstRows.append(QStringLiteral("▲７六歩(77)"));
            view.scrollToBottom();
        }, 0.4, QStringLiteral("same model"));
        QCOMPARE(view.model()->rowCount(), 17);

        view.setModel(&second);
        view.scrollToBottom();
        settle();
        verifyStep(view, [&] {
            secondRows.append(QStringLiteral("▲７六歩(77)"));
            view.scrollToBottom();
        }, 0.4, QStringLiteral("switched model"));
        // 外したモデルの変更は表に影響しない
        verifyStep(view, [&] {
            firstRows.append(QStringLiteral("▲１七"));
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
        for (int i = 0; i < 8; ++i) model.prependRow(thinkingRecord(i), kUnlimitedRows);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        settle();
        auto* numeric = new NumericRightAlignCommaDelegate(&view);
        PaintSpy spy;
        view.viewport()->installEventFilter(&spy);
        view.setItemDelegateForColumn(2, numeric);
        model.prependRow(thinkingRecord(100), kUnlimitedRows);
        settle();
        view.viewport()->removeEventFilter(&spy);
        const qint64 whole = qint64(view.viewport()->width()) * view.viewport()->height();
        QCOMPARE(area(spy.region), whole);
        QCOMPARE(stalePixels(view), 0);
    }
};

QTEST_MAIN(TestIncrementalTableView)
#include "tst_incremental_table_view.moc"
