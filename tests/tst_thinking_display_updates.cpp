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
#include <QTemporaryDir>
#include <QVBoxLayout>

#include "applicationfonts.h"
#include "elidelabel.h"
#include "engineinfowidget.h"
#include "kifubranchlistmodel.h"
#include "record_rows_helper.h"
#include "kifumovedelegate.h"
#include "kifupresentation.h"
#include "kifurecordlistmodel.h"
#include "movefont.h"
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

/// 読み筋の欄には pv をそのまま出す（開始局面の無い読み筋は、エンジンが送った語のまま表示する）
inline ThinkingInfoRow thinkingRow(qint64 timeMs, int depth, qint64 nodes, const UsiScore& score, const QString& pv)
{
    ThinkingInfoRow row;
    row.timeMs = timeMs;
    row.depth = depth;
    row.nodes = nodes;
    row.score = score;
    row.pv = PvText::play(QString(), QStringList{pv});
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

    /// 情報の帯の値（ElideLabel）
    static ElideLabel* bandValue(const EngineInfoWidget& info, const char* name)
    {
        return info.findChild<ElideLabel*>(QString::fromLatin1(name));
    }

    /// 情報の帯の表示中の文字の部品の位置（作った順）
    static QList<QRect> labelGeometries(const EngineInfoWidget& info)
    {
        QList<QRect> rects;
        const auto labels = info.findChildren<QLabel*>();
        for (const QLabel* label : labels) {
            if (!label->isHidden()) rects.append(label->geometry());
        }
        return rects;
    }

    /// 情報の帯の文字の部品が、帯の中に重ならずに並んでいるか
    static bool labelsFitBand(const EngineInfoWidget& info)
    {
        const QList<QRect> rects = labelGeometries(info);
        for (qsizetype i = 0; i < rects.size(); ++i) {
            if (!info.rect().contains(rects.at(i))) return false;
            for (qsizetype j = i + 1; j < rects.size(); ++j) {
                if (rects.at(i).intersects(rects.at(j))) return false;
            }
        }
        return true;
    }

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
        // 本体と同じく、表の行高を計算する前に共通の書体を確定する。
        ApplicationFonts::initialize();
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
            // 選択色とフォーカスの描画条件を揃え、両方とも前面にしてから比較する。
            plain.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&plain));
            plain.setFocus();
            QTRY_VERIFY(plain.hasFocus());
            const QImage expected = plain.grab().toImage();
            cut.activateWindow();
            QVERIFY(QTest::qWaitForWindowActive(&cut));
            cut.setFocus();
            QTRY_VERIFY(cut.hasFocus());
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
            NotationStyle::configure(QStringLiteral("ja_JP"), notation, false);
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
        NotationStyle::configure(QStringLiteral("ja_JP"), QStringLiteral("auto"), false);
    }

    // 思考情報は1秒に何度も届く。値が変わっても帯の配置を変えず、レイアウトの再計算も起こさない。
    // 同じ値なら描き直さない
    void engineBandUpdatesDoNotLayOutAgain()
    {
        QWidget window;
        auto* layout = new QVBoxLayout(&window);
        auto* info = new EngineInfoWidget(&window);
        layout->addWidget(info);
        layout->addStretch();
        UsiCommLogModel log;
        info->setModel(&log);
        window.resize(1600, 120);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        log.setEngineName(QStringLiteral("Engine"));
        log.setStatus(fullStatus());
        QCoreApplication::processEvents();

        auto* nodes = bandValue(*info, "engineInfoNodes");
        const QList<QRect> geometryBefore = labelGeometries(*info);
        const int heightBefore = info->height();

        EventCounter layouts(QEvent::LayoutRequest);
        EventCounter resizes(QEvent::Resize);
        EventCounter paints(QEvent::Paint);
        window.installEventFilter(&layouts);
        info->installEventFilter(&layouts);
        nodes->installEventFilter(&resizes);
        nodes->installEventFilter(&paints);

        log.updateStatus(fullStatus());
        QCoreApplication::processEvents();
        QCOMPARE(paints.count, 0);

        // 想定の桁数を超える値でも配置は変えない（省略し、全文はツールチップで読める）
        EngineSearchStatus large = fullStatus();
        large.depth = 12345;
        large.seldepth = 123456;
        large.nodes = 1234567890123456789LL;
        large.nps = 1234567890123LL;
        log.setStatus(large);
        log.setPredictedMove(QStringLiteral("△同　歩(23)"));
        QCoreApplication::processEvents();
        QCOMPARE(nodes->fullText(), QStringLiteral("1,234,567,890,123,456,789"));
        QVERIFY(nodes->toolTip().contains(nodes->fullText()));
        QVERIFY(QFontMetrics(nodes->font()).horizontalAdvance(nodes->fullText()) > nodes->width());
        QCOMPARE(labelGeometries(*info), geometryBefore);
        QCOMPARE(info->height(), heightBefore);
        QCOMPARE(layouts.count, 0);
        QCOMPARE(resizes.count, 0);

        // 値が消えても配置は変えない
        log.clear();
        info->setModel(nullptr);
        QCoreApplication::processEvents();
        QVERIFY(nodes->fullText().isEmpty());
        QCOMPARE(labelGeometries(*info), geometryBefore);
    }

    // 2つのエンジンの帯は、エンジン名の長さや値によらず、どの幅でもすべての項目が同じ位置に並ぶ
    void engineBandsAlignWhateverTheyShow()
    {
        UsiCommLogModel log1;
        UsiCommLogModel log2;
        log1.setEngineName(QStringLiteral("YaneuraOu NNUE 9.10git 64ZEN2 with a very long display name"));
        log2.setEngineName(QStringLiteral("H"));
        log1.setPredictedMove(QStringLiteral("△９四歩(93)"));
        EngineSearchStatus status = fullStatus();
        status.nodes = std::numeric_limits<qint64>::max();
        log1.setStatus(status);
        EngineSearchStatus few;
        few.nodes = 1;
        log2.setStatus(few);

        QWidget window;
        auto* layout = new QVBoxLayout(&window);
        auto* first = new EngineInfoWidget(&window);
        auto* second = new EngineInfoWidget(&window);
        layout->addWidget(first);
        layout->addWidget(second);
        layout->addStretch();
        first->setModel(&log1);
        second->setModel(&log2);
        first->setSide(Turn::Black);
        second->setSide(Turn::White);
        window.resize(1600, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        for (const int width : {1600, 1300, 1100, 900, 700, 500, 320}) {
            window.resize(width, 300);
            QCoreApplication::processEvents();
            QCOMPARE(first->width(), second->width());
            QCOMPARE(first->height(), second->height());
            QCOMPARE(labelGeometries(*first), labelGeometries(*second));
        }
    }

    // 1行に収まらなければ、エンジン名と指し手の行と数値の行に分ける。どの幅でも項目は帯の中に重ならずに並ぶ
    void engineBandWrapsWhenNarrow()
    {
        EngineInfoWidget info;
        info.setSide(Turn::Black);
        info.resize(2000, info.height());
        info.show();
        QVERIFY(QTest::qWaitForWindowExposed(&info));
        QCoreApplication::processEvents();
        const int oneRow = info.height();
        const auto* name = bandValue(info, "engineInfoName");
        const auto* searched = bandValue(info, "engineInfoSearched");
        const auto* depth = bandValue(info, "engineInfoDepth");
        QCOMPARE(depth->y(), name->y());

        bool twoRows = false;
        int previousHeight = oneRow;
        for (int width = 2000; width >= 300; width -= 13) {
            info.resize(width, info.height());
            // 行を増やすのは画面に描いた後なので、配置が整うのを待つ
            QTRY_VERIFY2(labelsFitBand(info), qPrintable(QStringLiteral("width %1").arg(width)));
            QVERIFY2(info.height() >= previousHeight, qPrintable(QString::number(width)));
            previousHeight = info.height();
            // エンジン名の行には、狭くなるまで指し手を置き、数値は別の行に送る
            if (info.height() > oneRow && !twoRows) {
                twoRows = true;
                QCOMPARE(searched->y(), name->y());
                QVERIFY(depth->y() > name->y());
            }
        }
        QVERIFY(twoRows);
        info.resize(2000, info.height());
        QCoreApplication::processEvents();
        QCOMPARE(info.height(), oneRow);
    }

    // 隠れている帯（ドックのタブの裏）は幅が決まっていないので、折り返した高さを親に求めない。
    // 高さを固定するので、求めるとドックの最小の高さが膨らみ、盤を拡大するとウィンドウまで伸びていた
    void hiddenEngineBandStaysOneRowHigh()
    {
        QWidget window;
        auto* layout = new QVBoxLayout(&window);
        auto* info = new EngineInfoWidget(&window);
        layout->addWidget(info);
        window.resize(2000, 200);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();
        const int oneRow = info->height();

        // 見えていて狭ければ折り返す
        window.resize(320, 400);
        QCoreApplication::processEvents();
        QTRY_VERIFY(info->height() > oneRow);

        // 隠れたら狭いままでも1行の高さに戻り、また出せば折り返す
        window.hide();
        QCoreApplication::processEvents();
        QCOMPARE(info->height(), oneRow);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTRY_VERIFY(info->height() > oneRow);

        // 一度も表示していない帯は、既定の小さな幅でも1行の高さ
        QWidget tabPage;
        EngineInfoWidget neverShown(&tabPage);
        QCoreApplication::processEvents();
        QCOMPARE(neverShown.height(), oneRow);
    }

    // ドックのタブの裏の帯は isVisible() でも見えている扱いのまま、起動時の仮の狭い幅で残っている。そこで行を増やすと
    // 高さが親の最小の高さになり、メインウィンドウを押し広げていた（簡体字・繁体字で高さ 980 が 1011 になった）。
    // 画面に描かれない帯は行を増やさず、前に出て描かれたら増やす
    void coveredEngineBandDoesNotGrow()
    {
        QWidget window;
        auto* info = new EngineInfoWidget(&window);
        info->setGeometry(0, 0, 230, info->height());
        const int oneRow = info->height();
        auto* cover = new QWidget(&window);   // タブの手前のドック
        cover->setAutoFillBackground(true);
        cover->setGeometry(0, 0, 600, 300);
        cover->raise();
        window.resize(600, 300);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QVERIFY(info->isVisible());
        QTest::qWait(50);
        QCOMPARE(info->height(), oneRow);

        // 手前に出たら折り返す（幅が同じで大きさの変更の知らせが来なくても）
        info->raise();
        QTRY_VERIFY(info->height() > oneRow);
    }

    // 表示された瞬間の幅が仮の値で、すぐ本来の幅になるなら、行は増やさない
    void engineBandDoesNotGrowForTransientWidth()
    {
        QWidget window;
        auto* info = new EngineInfoWidget(&window);
        info->setGeometry(0, 0, 230, info->height());
        const int oneRow = info->height();

        EventCounter resizes(QEvent::Resize);
        class HeightWatcher : public QObject {
        public:
            int tallest = 0;
        protected:
            bool eventFilter(QObject* watched, QEvent* event) override
            {
                if (event->type() == QEvent::Resize) tallest = qMax(tallest, static_cast<QWidget*>(watched)->height());
                return QObject::eventFilter(watched, event);
            }
        } watcher;
        info->installEventFilter(&watcher);

        // 表示された瞬間の幅は狭い。同じ処理の中で本来の幅になる
        window.resize(1600, 200);
        window.show();
        QCOMPARE(info->height(), oneRow);
        info->resize(1500, info->height());
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QTest::qWait(20);
        QCOMPARE(info->height(), oneRow);
        QCOMPARE(watcher.tallest, oneRow);

        // 幅が決まっていて狭ければ、次のイベントループで行を増やす
        info->resize(420, info->height());
        QCOMPARE(info->height(), oneRow);
        QTRY_VERIFY(info->height() > oneRow);
        const int wrapped = info->height();

        // 同じ処理の中で何度狭めても、増やすのは1回だけ
        info->resize(1500, info->height());
        QCoreApplication::processEvents();
        QCOMPARE(info->height(), oneRow);
        for (const int width : {500, 450, 420}) info->resize(width, info->height());
        QTRY_COMPARE(info->height(), wrapped);

        // 待っている間に広げれば増やさない
        info->resize(1500, info->height());
        QCoreApplication::processEvents();
        info->resize(420, info->height());
        info->resize(1500, info->height());
        QTest::qWait(20);
        QCOMPARE(info->height(), oneRow);
    }

    // 手番の印はエンジン名の前に出す。印の有無で項目の位置は変わらない
    void engineBandShowsSideMark()
    {
        EngineInfoWidget info;
        info.resize(1600, info.height());
        info.show();
        QVERIFY(QTest::qWaitForWindowExposed(&info));
        QCoreApplication::processEvents();
        auto* mark = info.findChild<QLabel*>(QStringLiteral("engineInfoSide"));
        const auto* name = bandValue(info, "engineInfoName");
        const auto* nodes = bandValue(info, "engineInfoNodes");
        QVERIFY(mark);
        QVERIFY(!mark->isVisible());
        const int nameLeft = name->x();
        const QRect nodesGeometry = nodes->geometry();

        info.setSide(Turn::Black);
        QVERIFY(mark->isVisible());
        QCOMPARE(mark->text(), QStringLiteral("▲"));
        QCOMPARE(mark->accessibleName(), EngineInfoWidget::tr("先手"));
        QVERIFY(mark->geometry().right() < name->x());
        QVERIFY(name->x() > nameLeft);
        QCOMPARE(nodes->geometry(), nodesGeometry);

        info.setSide(Turn::White);
        QCOMPARE(mark->text(), QStringLiteral("△"));
        QCOMPARE(mark->accessibleName(), EngineInfoWidget::tr("後手"));

        info.setSide(std::nullopt);
        QVERIFY(!mark->isVisible());
        QCOMPARE(name->x(), nameLeft);
        QCOMPARE(nodes->geometry(), nodesGeometry);
    }

    // 定跡手の探索手は長くなる。余裕のある幅なら省略しない（狭ければ省略し、エンジン名を優先する）
    void bookMoveFitsWhenThereIsRoom()
    {
        const QString move = QStringLiteral("△８四歩(83)");
        QCOMPARE(KifuPresentation::bookMove(move, 0.0), QCoreApplication::translate("KifuPresentation", "%1（定跡）").arg(move));
        QCOMPARE(KifuPresentation::bookMove(move, 39.94), QCoreApplication::translate("KifuPresentation", "%1（定跡 %2%）").arg(move, QStringLiteral("39.9")));

        UsiCommLogModel log;
        const QString engineName = QStringLiteral("YaneuraOu NNUE 9.10git 64ZEN2");
        log.setEngineName(engineName);
        EngineSearchStatus status;
        status.searchedMove = KifuPresentation::bookMove(QStringLiteral("▲９九銀不成(99)"), 99.9);
        log.setStatus(status);
        EngineInfoWidget info;
        info.setModel(&log);
        info.setSide(Turn::Black);
        info.setFontSize(10);
        info.resize(2400, info.height());
        info.show();
        QVERIFY(QTest::qWaitForWindowExposed(&info));
        QCoreApplication::processEvents();
        const auto* searched = bandValue(info, "engineInfoSearched");
        const auto* name = bandValue(info, "engineInfoName");
        const int fullWidth = QFontMetrics(searched->font()).horizontalAdvance(status.searchedMove);
        QVERIFY(searched->width() >= fullWidth);
        QVERIFY(name->width() >= QFontMetrics(name->font()).horizontalAdvance(engineName));

        // 幅を狭めていくと先に探索手の余分が無くなり、エンジン名は残る
        int width = 2400;
        while (searched->width() >= fullWidth && width > 300) {
            width -= 10;
            info.resize(width, info.height());
            QCoreApplication::processEvents();
        }
        QVERIFY(width > 300);
        QVERIFY(name->width() >= QFontMetrics(name->font()).horizontalAdvance(engineName));
    }

    // ラベルが書体の一部をアプリの設定から受けても（指定した書体と描く書体が違っても）、実際に描く書体で測る
    void engineBandMeasuresTheFontLabelsDraw()
    {
        const QFont before = QApplication::font("QLabel");
        QFont wider = before;
        wider.setLetterSpacing(QFont::AbsoluteSpacing, 3);
        QApplication::setFont(wider, "QLabel");
        {
            UsiCommLogModel log;
            const QString engineName = QStringLiteral("YaneuraOu NNUE 9.10git 64ZEN2");
            log.setEngineName(engineName);
            EngineInfoWidget info;
            info.setModel(&log);
            info.setSide(Turn::Black);
            info.setFontSize(10);
            info.resize(3000, info.height());
            info.show();
            QVERIFY(QTest::qWaitForWindowExposed(&info));
            QCoreApplication::processEvents();
            const auto* name = bandValue(info, "engineInfoName");
            const auto* nodes = bandValue(info, "engineInfoNodes");
            QCOMPARE(name->font().letterSpacing(), 3.0);
            QVERIFY(name->width() >= name->fontMetrics().horizontalAdvance(engineName));
            QVERIFY(nodes->width() >= nodes->fontMetrics().horizontalAdvance(QStringLiteral("999,999,999,999")));
        }
        QApplication::setFont(before, "QLabel");
    }

    // 文字の大きさを変えると、項目の幅と行の高さを測り直す
    void engineBandFollowsFontSize()
    {
        EngineInfoWidget info;
        info.setFontSize(10);
        info.resize(2400, info.height());
        info.show();
        QVERIFY(QTest::qWaitForWindowExposed(&info));
        QCoreApplication::processEvents();
        const auto* nodes = bandValue(info, "engineInfoNodes");
        const auto* searched = bandValue(info, "engineInfoSearched");
        const int smallWidth = nodes->width();
        const int smallHeight = info.height();

        info.setFontSize(16);
        QCoreApplication::processEvents();
        QCOMPARE(info.fontSize(), 16);
        QCOMPARE(nodes->font().pointSize(), 16);
        QCOMPARE(searched->font().pointSize(), 16);
        QVERIFY(nodes->width() > smallWidth);
        QVERIFY(info.height() > smallHeight);
        QVERIFY(nodes->width() >= QFontMetrics(nodes->font()).horizontalAdvance(QStringLiteral("999,999,999,999")));

        info.setFontSize(10);
        QCoreApplication::processEvents();
        QCOMPARE(nodes->width(), smallWidth);
        QCOMPARE(info.height(), smallHeight);
    }

    // 指し手の値は指し手の書体で書く。検討タブの帯は予想手を出さない
    void engineBandMoveFieldsUseMoveFont()
    {
        EngineInfoWidget info;
        QCOMPARE(bandValue(info, "engineInfoSearched")->font(), MoveFont::font(info.font()));
        QCOMPARE(bandValue(info, "engineInfoNodes")->font(), info.font());

        EngineInfoWidget withoutPrediction(nullptr, false);
        withoutPrediction.resize(1600, withoutPrediction.height());
        withoutPrediction.show();
        QVERIFY(QTest::qWaitForWindowExposed(&withoutPrediction));
        QVERIFY(!bandValue(withoutPrediction, "engineInfoPredicted")->isVisible());
        QVERIFY(bandValue(withoutPrediction, "engineInfoSearched")->isVisible());
    }
};

QTEST_MAIN(TestThinkingDisplayUpdates)
#include "tst_thinking_display_updates.moc"
