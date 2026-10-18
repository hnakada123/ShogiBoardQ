#include <QtTest>
#include <limits>
#include <QSignalSpy>
#include <QDockWidget>
#include <QFontMetrics>
#include <QGraphicsView>
#include <QHeaderView>
#include <QLabel>
#include <QMainWindow>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include "engineinfowidget.h"
#include "kifubranchlistmodel.h"
#include "record_rows_helper.h"
#include "kifumovedelegate.h"
#include "kifupresentation.h"
#include "kifurecordlistmodel.h"
#include "numericrightaligncommadelegate.h"
#include "opaquebackground.h"
#include "tablebuttondelegate.h"
#include "pvtextdelegate.h"
#include "recordhighlightdelegate.h"
#include "recordpane.h"
#include "shogienginethinkingmodel.h"
#include "shogiinforecord.h"
#include "tablestyles.h"
#include "usicommlogmodel.h"

/// 思考情報が届くたびの表示の更新が、余計な描き直しやレイアウトの再計算を起こさないことを検証する。

/// 思考の表の1行（時間・深さ・ノード数・評価値・読み筋）
/// 思考の表に足すときに、古い行を消さない
inline constexpr int kUnlimitedRows = std::numeric_limits<int>::max();

inline ThinkingInfoRow thinkingRow(qint64 timeMs, int depth, qint64 nodes, const UsiScore& score, const QString& pv,
                                    const QString& usiPv = {})
{
    ThinkingInfoRow row;
    row.timeMs = timeMs;
    row.depth = depth;
    row.nodes = nodes;
    row.score = score;
    row.pvKanji = pv;
    row.usiPv = usiPv;
    return row;
}

/// 情報欄の全部の項目がある探索状況
static EngineSearchStatus fullStatus()
{
    EngineSearchStatus status;
    status.searchedMove = QStringLiteral("▲２三歩成(24)");
    status.depth = 12;
    status.seldepth = 18;
    status.nodes = 123456789;
    status.nps = 1234567;
    status.hashfull = 125;
    return status;
}

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

    // 比べる基準: 文字まで Qt に描かせる（QStyledItemDelegate::paint）
    class QtNumericDelegate : public NumericRightAlignCommaDelegate
    {
    public:
        using NumericRightAlignCommaDelegate::NumericRightAlignCommaDelegate;
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyledItemDelegate::paint(painter, option, index);
        }
    };
    class QtKifuMoveDelegate : public KifuMoveDelegate
    {
    public:
        using KifuMoveDelegate::KifuMoveDelegate;
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyledItemDelegate::paint(painter, option, index);
        }
    };
    // 棋譜欄の現在行の目印は RecordHighlightDelegate と同じに描く
    class QtRecordHighlightDelegate : public RecordHighlightDelegate
    {
    public:
        using RecordHighlightDelegate::RecordHighlightDelegate;
        void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override
        {
            QStyledItemDelegate::paint(painter, option, index);
            if (index.column() != 0) return;
            const auto* recordModel = qobject_cast<const KifuRecordListModel*>(index.model());
            const bool current = recordModel && recordModel->currentHighlightRow() == index.row();
            if (current || (option.state & QStyle::State_Selected)) {
                painter->fillRect(QRect(option.rect.left(), option.rect.top(), 3, option.rect.height()),
                                  TableStyles::selectionAccent());
            }
        }
    };

    enum class CellState { Plain, Selected, Hover, Focus, Disabled, Active };

    /// 表を状態 state にして撮る（ウィンドウが前面かどうかで色が変わるので、1つずつ前面にして撮る）
    static QImage grabInState(QWidget* window, QTableView* view, CellState state, const QModelIndex& target)
    {
        if (state != CellState::Plain && state != CellState::Hover) view->selectRow(0);
        if (state == CellState::Disabled) view->setEnabled(false);
        view->setMouseTracking(true);
        window->show();
        if (!QTest::qWaitForWindowExposed(window)) return QImage();
        if (state == CellState::Focus || state == CellState::Active) {
            window->activateWindow();
            if (!QTest::qWaitForWindowActive(window)) return QImage();
        }
        if (state == CellState::Focus) {
            view->setFocus();
            view->setCurrentIndex(target);
        }
        if (state == CellState::Hover) QTest::mouseMove(view->viewport(), view->visualRect(target).center());
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        return view->grab().toImage();
    }

    /// 思考タブの表と同じ設定の表（読み筋の列だけ delegate を変えられる）
    static void setUpThinkingView(QTableView& view, ShogiEngineThinkingModel* model, bool pvDelegate)
    {
        view.setModel(model);
        view.setStyleSheet(TableStyles::thinking());
        view.setShowGrid(false);
        view.setItemDelegateForColumn(ShogiEngineThinkingModel::BoardColumn, new TableButtonDelegate(&view));
        auto* numeric = new NumericRightAlignCommaDelegate(&view);
        for (int column = 0; column < 4; ++column) view.setItemDelegateForColumn(column, numeric);
        if (pvDelegate) view.setItemDelegateForColumn(ShogiEngineThinkingModel::PvColumn, new PvTextDelegate(&view));
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
            model.prependRow(thinkingRow(row * 13, row, row * 12345, UsiScore::cp(row * 37 - 200),
                                          longPv(2 + row * 3)), kUnlimitedRows);
        }
        for (const int pvWidth : {60, 180, 420, 900}) {
            QTableView plain;
            QTableView cut;
            setUpThinkingView(plain, &model, false);
            setUpThinkingView(cut, &model, true);
            for (QTableView* view : {&plain, &cut}) {
                view->resize(pvWidth + 620, 320);
                view->horizontalHeader()->setStretchLastSection(false);
                view->horizontalHeader()->resizeSection(ShogiEngineThinkingModel::PvColumn, pvWidth);
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

    // 読み筋の文字は自分で描く（枠・背景は Qt）。選択・マウスを乗せた行・フォーカス・無効・
    // ウィンドウが前面かどうか・英語表記（欧文と記号の混在）でも、Qt が全文を描いた場合と同じ画素になる
    void pvTextRendersSameInEveryState()
    {
        ShogiEngineThinkingModel model;
        const QStringList western = {QStringLiteral("▲P-7f △P-3d ▲Bx2b+ △Sx2b ▲B*4e △S-3c "),
                                     QStringLiteral("△N-3c ▲R-2f △P*2e ▲R-2h △K-6b ")};
        for (int row = 0; row < 14; ++row) {
            const QString pv = row % 3 == 2 ? western.at(row % 2).repeated(1 + row % 4) : longPv(1 + row * 3);
            model.prependRow(thinkingRow(row * 13, row, row * 12345, UsiScore::cp(row * 37 - 200), pv), kUnlimitedRows);
        }
        enum State { Plain, Hover, Focus, Disabled, Active };
        for (const State state : {Plain, Hover, Focus, Disabled, Active}) {
            for (const int pvWidth : {50, 230, 700}) {
                // ウィンドウが前面かどうかで色が変わるので、1つずつ前面にして撮る
                const auto render = [&](bool pvDelegate) {
                    QTableView view;
                    setUpThinkingView(view, &model, pvDelegate);
                    view.setMouseTracking(true);
                    view.resize(pvWidth + 620, 340);
                    view.horizontalHeader()->setStretchLastSection(false);
                    view.horizontalHeader()->resizeSection(ShogiEngineThinkingModel::PvColumn, pvWidth);
                    view.setSelectionBehavior(QAbstractItemView::SelectRows);
                    view.selectRow(2);
                    if (state == Disabled) view.setEnabled(false);
                    view.show();
                    if (!QTest::qWaitForWindowExposed(&view)) return QImage();
                    if (state == Focus || state == Active) {
                        view.activateWindow();
                        if (!QTest::qWaitForWindowActive(&view)) return QImage();
                    }
                    if (state == Focus) {
                        view.setFocus();
                        view.setCurrentIndex(model.index(4, ShogiEngineThinkingModel::PvColumn));
                    }
                    if (state == Hover) QTest::mouseMove(view.viewport(), view.visualRect(model.index(5, ShogiEngineThinkingModel::PvColumn)).center());
                    QCoreApplication::processEvents();
                    return view.grab().toImage();
                };
                const QImage expected = render(false);
                const QImage actual = render(true);
                QVERIFY(!expected.isNull());
                QCOMPARE(actual.size(), expected.size());
                QVERIFY2(actual == expected,
                         qPrintable(QStringLiteral("state %1, PV column width %2").arg(int(state)).arg(pvWidth)));
            }
        }
    }

    // 数値の列・エンジン情報の表・棋譜欄も、文字は配置を覚えて自分で描く（ItemTextDelegate）。
    // 選択・マウスを乗せた行・フォーカス・無効・ウィンドウが前面かどうか・表記で、Qt が描いた場合と同じ画素になる
    void cellTextRendersSameInEveryState()
    {
        const QString startSfen = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");
        const QList<CellState> states = {CellState::Plain, CellState::Selected, CellState::Hover,
                                         CellState::Focus, CellState::Disabled, CellState::Active};
        for (const QString& notation : {QStringLiteral("japanese"), QStringLiteral("western")}) {
            KifuPresentation::configure(QStringLiteral("ja_JP"), notation, false);
            for (const CellState state : states) {
                const QString where = QStringLiteral("%1, state %2").arg(notation).arg(int(state));

                // 思考表の数値の列（右寄せ・3桁区切り。負の数・文字列の評価値を含む）
                ShogiEngineThinkingModel thinking;
                for (int row = 0; row < 12; ++row) {
                    const UsiScore score = row == 5 ? UsiScore::mate(15) : UsiScore::cp(row * 397 - 1800);
                    thinking.prependRow(thinkingRow(row * 1370, row * 3, qint64(row) * 98765432, score, longPv(3)), kUnlimitedRows);
                }
                const auto renderThinking = [&](bool qtNumeric) {
                    QTableView view;
                    setUpThinkingView(view, &thinking, true);
                    if (qtNumeric) {
                        auto* numeric = new QtNumericDelegate(&view);
                        for (int column = 0; column < 4; ++column) view.setItemDelegateForColumn(column, numeric);
                    }
                    view.resize(620, 300);
                    return grabInState(&view, &view, state, thinking.index(3, 2));
                };
                const QImage thinkingExpected = renderThinking(true);
                QVERIFY(!thinkingExpected.isNull());
                QVERIFY2(renderThinking(false) == thinkingExpected, qPrintable(QStringLiteral("thinking: ") + where));

                // エンジン情報の表（指し手の列は KifuMoveDelegate）
                UsiCommLogModel log;
                log.setEngineName(QStringLiteral("YaneuraOu NNUE 9.10"));
                log.setPredictedMove(QStringLiteral("△同　歩(23)"));
                log.setStatus(fullStatus());
                // 列幅を保存した値で狭くすると、折り返す設定の表で2行になる・はみ出す値ができる（Qt に描かせる）
                const auto renderInfo = [&](bool qtText, bool narrow) {
                    EngineInfoWidget info(AnalysisSettings::ThinkingTable::Engine1);
                    info.setModel(&log);
                    if (narrow) info.setColumnWidths({70, 60, 48, 40, 52, 70, 40});
                    info.resize(1000, 80);
                    auto* table = info.findChild<QTableWidget*>(QStringLiteral("engineInfoTable"));
                    if (!table) return QImage();
                    if (qtText) {
                        table->setItemDelegate(new QStyledItemDelegate(table));
                        auto* move = new QtKifuMoveDelegate(table);
                        table->setItemDelegateForColumn(1, move);
                        table->setItemDelegateForColumn(2, move);
                    }
                    return grabInState(&info, table, state, table->model()->index(0, 2));
                };
                for (const bool narrow : {false, true}) {
                    const QImage infoExpected = renderInfo(true, narrow);
                    QVERIFY(!infoExpected.isNull());
                    QVERIFY2(renderInfo(false, narrow) == infoExpected,
                             qPrintable(QStringLiteral("engine info: %1, narrow %2").arg(where).arg(narrow)));
                }

                // 棋譜欄（指し手の書体・分岐の背景色・現在行・中央寄せの消費時間・しおり・長いコメント）
                RecordRows rows(startSfen);
                KifuRecordListModel& record = rows.model;
                KifuBranchListModel branch(&rows.tree);
                for (int ply = 1; ply <= 14; ++ply) {
                    rows.append(QStringLiteral("▲７六歩(77)"), QStringLiteral("0:%1/0:00:%1").arg(ply, 2, 10, QLatin1Char('0')),
                                ply % 4 == 1 ? longPv(8) : QString(),
                                ply % 5 == 2 ? QStringLiteral("しおり%1").arg(ply) : QString(),
                                ShogiMove(UsiMove::fromString(u"7g7f"), Piece::BlackPawn));
                }
                rows.markBranch(3);
                rows.markBranch(7);
                record.setCurrentHighlightRow(5);
                const auto renderRecord = [&](bool qtText) {
                    RecordPane pane;
                    pane.setModels(&record, &branch);
                    pane.resize(560, 420);
                    QTableView* view = pane.kifuView();
                    if (qtText) view->setItemDelegate(new QtRecordHighlightDelegate(view));
                    return grabInState(&pane, view, state, record.index(4, 0));
                };
                const QImage recordExpected = renderRecord(true);
                QVERIFY(!recordExpected.isNull());
                QVERIFY2(renderRecord(false) == recordExpected, qPrintable(QStringLiteral("record: ") + where));
            }
        }
        KifuPresentation::configure(QStringLiteral("ja_JP"), QStringLiteral("auto"), false);
    }

    // 思考情報は1行で複数の値が変わる。値が変わらなければ表を更新せず、
    // 列幅の計算（レイアウトの再計算につながる）はまとめて1回にする
    void engineInfoUpdatesAreCoalesced()
    {
        UsiCommLogModel log;
        EngineInfoWidget info(AnalysisSettings::ThinkingTable::Engine1);
        info.setModel(&log);
        info.resize(900, 60);
        info.show();
        QVERIFY(QTest::qWaitForWindowExposed(&info));
        auto* table = info.findChild<QTableWidget*>(QStringLiteral("engineInfoTable"));
        QVERIFY(table != nullptr);

        EngineSearchStatus nodesOnly;
        nodesOnly.nodes = 12345;
        log.updateStatus(nodesOnly);
        QCoreApplication::processEvents();
        QSignalSpy changed(table->model(), &QAbstractItemModel::dataChanged);
        log.updateStatus(nodesOnly);
        QCOMPARE(changed.count(), 0);

        // 値ごとに計算すると、表の幅を値の数だけ変える（そのたびに親のレイアウトを計算し直す）
        EventCounter resizes(QEvent::Resize);
        table->installEventFilter(&resizes);
        const int widthBefore = table->width();
        // info 1行の探索状況は1回で届く
        log.updateStatus(fullStatus());
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(table->width() > widthBefore);
        QVERIFY2(resizes.count == 1, qPrintable(QStringLiteral("table resized %1 times").arg(resizes.count)));
        // 列幅は内容に合わせて広がっている
        const QFontMetrics metrics(table->font());
        QVERIFY(table->columnWidth(4) >= metrics.horizontalAdvance(QStringLiteral("123,456,789")));
    }

    // 測った文字の幅と見出しの高さを使い回しても、文字の大きさを変えれば測り直す
    void engineInfoGeometryFollowsFontSize()
    {
        UsiCommLogModel log;
        EngineInfoWidget info(AnalysisSettings::ThinkingTable::Engine1);
        info.setModel(&log);
        info.resize(1400, 80);
        info.show();
        QVERIFY(QTest::qWaitForWindowExposed(&info));
        auto* table = info.findChild<QTableWidget*>(QStringLiteral("engineInfoTable"));
        QVERIFY(table != nullptr);
        const auto expectedHeight = [table] {
            return table->horizontalHeader()->sizeHint().height() + table->verticalHeader()->defaultSectionSize()
                + 2 * table->frameWidth();
        };
        EngineSearchStatus status;
        status.searchedMove = QStringLiteral("▲２三歩成(24)");
        status.nodes = 123456789;
        log.setStatus(status);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QCOMPARE(table->height(), expectedHeight());
        const int nodesWidth = table->columnWidth(4);
        QVERIFY(nodesWidth >= QFontMetrics(table->font()).horizontalAdvance(QStringLiteral("123,456,789")));

        info.setFontSize(16);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(table->columnWidth(4) > nodesWidth);
        QVERIFY(table->columnWidth(4) >= QFontMetrics(table->font()).horizontalAdvance(QStringLiteral("123,456,789")));
        QCOMPARE(table->height(), expectedHeight());

        // 値が同じ幅に戻れば、列幅も戻る
        status.nodes = 1;
        log.setStatus(status);
        QCoreApplication::processEvents();
        QCoreApplication::processEvents();
        QVERIFY(table->columnWidth(4) < QFontMetrics(table->font()).horizontalAdvance(QStringLiteral("123,456,789")) + 20);
    }
};

QTEST_MAIN(TestThinkingDisplayUpdates)
#include "tst_thinking_display_updates.moc"
