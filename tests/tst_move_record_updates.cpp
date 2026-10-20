#include <QtTest>
#include <QSignalSpy>
#include <QScopeGuard>
#include <QHeaderView>
#include <QLabel>
#include <QStyledItemDelegate>
#include <QScrollBar>
#include <QTableView>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QVBoxLayout>

#include "commenteditorpanel.h"
#include "evaluationchartwidget.h"
#include "enginelifecyclemanager.h"
#include "evaluationgraphpresenter.h"
#include "shogiclock.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifudisplaypresenter.h"
#include "kifunavigationstate.h"
#include "kifubranchlistmodel.h"
#include "livegamesession.h"
#include "kifurecordlistmodel.h"
#include "recordhighlightdelegate.h"
#include "recordpaneappearancemanager.h"
#include "sfenutils.h"
#include "shogimove.h"
#include "kifmovetext.h"
#include "kifupresentation.h"
#include "test_replay.h"

/// 1手指すたびに行う棋譜欄・評価値グラフの更新が、手数に比例して重くならないことを検証する。
class TestMoveRecordUpdates : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    /// 受け取ったレイアウトの再計算の要求を数える
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

    /// 飛車を左右に往復させる棋譜（対局外で指した手と同じく、ツリーにだけ記録する）
    static KifuBranchNode* buildLine(KifuBranchTree& tree, int plies)
    {
        tree.setRootSfen(SfenUtils::hirateSfen());
        KifuBranchNode* node = tree.root();
        const ShogiMove moves[] = {
            ShogiMove(UsiMove::fromString(u"2h3h"), Piece::BlackRook),
            ShogiMove(UsiMove::fromString(u"8b7b"), Piece::WhiteRook),
            ShogiMove(UsiMove::fromString(u"3h2h"), Piece::BlackRook),
            ShogiMove(UsiMove::fromString(u"7b8b"), Piece::WhiteRook),
        };
        const QString texts[] = {QStringLiteral("▲３八飛(28)"), QStringLiteral("△７二飛(82)"),
                                 QStringLiteral("▲２八飛(38)"), QStringLiteral("△８二飛(72)")};
        for (int ply = 1; ply <= plies; ++ply) {
            node = TestTree::addMove(tree, node, moves[(ply - 1) % 4], texts[(ply - 1) % 4],
                                     QStringLiteral("sfen-%1").arg(ply));
        }
        return node;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    // 棋譜欄の作り直しは行をまとめて追加する。1行ずつ追加すると、棋譜欄が行ごとに
    // 全行の文字幅を測り直し、手数の2乗に比例して遅くなる。
    // 末尾に手が増えただけのときは、増えた行だけを足す。
    void recordModelIsRebuiltInOneInsertion()
    {
        KifuBranchTree tree;
        KifuBranchNode* last = buildLine(tree, 120);
        KifuRecordListModel model(&tree);

        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        model.setPath(tree.pathToNode(last));
        QCOMPARE(model.rowCount(), 121);
        QCOMPARE(inserted.count(), 1);
        QVERIFY(model.data(model.index(120, KifuRecordListModel::MoveColumn)).toString()
                    .contains(QStringLiteral("８二飛")));

        // 内容が変わっていなければ何もしない
        inserted.clear();
        model.setPath(tree.pathToNode(last));
        QCOMPARE(model.rowCount(), 121);
        QCOMPARE(inserted.count(), 0);
        QCOMPARE(removed.count(), 0);

        // 末尾に1手増えたときは、その1行だけを足す（全行を作り直さない）
        KifuBranchNode* next = TestTree::addMove(tree, 
            last, ShogiMove(UsiMove::fromString(u"2h3h"), Piece::BlackRook),
            QStringLiteral("▲３八飛(28)"), QStringLiteral("sfen-121"));
        model.setPath(tree.pathToNode(next));
        QCOMPARE(model.rowCount(), 122);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(inserted.at(0).at(1).toInt(), 121);
        QCOMPARE(inserted.at(0).at(2).toInt(), 121);
        QVERIFY(model.data(model.index(121, KifuRecordListModel::MoveColumn)).toString()
                    .contains(QStringLiteral("３八飛")));

        // 途中までの手順に替わったときは、後ろの行だけを削る
        model.setPath(tree.pathToNode(TestTree::findByPlyOnMainLine(tree, 60)));
        QCOMPARE(model.rowCount(), 61);
        QCOMPARE(removed.count(), 1);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(reset.count(), 0);
    }

    // 分岐の印が変わらなければ、全行の変更通知（表全体の描き直し）を出さない
    void unchangedBranchMarksDoNotNotify()
    {
        KifuBranchTree tree;
        KifuBranchNode* last = buildLine(tree, 10);
        KifuNavigationState nav(&tree);
        LiveGameSession live(&tree, &nav);
        KifuRecordListModel model(&tree);
        KifuBranchListModel branches(&tree);
        KifuDisplayPresenter::Deps deps;
        deps.navState = &nav;
        deps.liveSession = &live;
        deps.recordModel = &model;
        deps.branchModel = &branches;
        KifuDisplayPresenter presenter(deps);
        nav.setCurrentNode(last);

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        const auto wholeTableUpdates = [&changed, &model]() {
            int count = 0;
            for (const QList<QVariant>& args : std::as_const(changed)) {
                if (args.at(0).toModelIndex().row() == 0
                    && args.at(1).toModelIndex().row() == model.rowCount() - 1) ++count;
            }
            return count;
        };
        // 手を移るだけなら、強調する行だけを描き直す
        for (int ply : {3, 7, 0, 10}) nav.setCurrentNode(TestTree::findByPlyOnMainLine(tree, ply));
        QCOMPARE(wholeTableUpdates(), 0);
        QVERIFY(!changed.isEmpty());

        // 分岐ができて「+」の印が変わったときだけ、全行を描き直す
        TestTree::addMove(tree, TestTree::findByPlyOnMainLine(tree, 4), ShogiMove(), QStringLiteral("▲９六歩(97)"),
                     QStringLiteral("branch"));
        QCOMPARE(wholeTableUpdates(), 1);
        nav.setCurrentNode(TestTree::findByPlyOnMainLine(tree, 2));
        QCOMPARE(wholeTableUpdates(), 1);
    }

    // コメント欄の手数の表示は手が進むたびに変わる。大きさを固定して、表示が変わっても
    // レイアウトの再計算（メインウィンドウ全体の配置計算につながる）を起こさない
    void commentPositionLabelDoesNotRelayout()
    {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        // パネルは host の子にする（host が消す）
        CommentEditorPanel& panel = *new CommentEditorPanel(&host);
        layout->addWidget(&panel);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        auto* label = host.findChild<QLabel*>(QStringLiteral("kifuCommentPosition"));
        QVERIFY(label != nullptr);
        QCOMPARE(label->minimumSize(), label->maximumSize());
        // 表示した直後のレイアウトの要求を先に済ませる
        for (int i = 0; i < 3; ++i) QCoreApplication::processEvents();

        LayoutRequestCounter counter;
        host.installEventFilter(&counter);
        for (QWidget* child : host.findChildren<QWidget*>()) child->installEventFilter(&counter);
        for (int ply : {1, 12, 123, 1234, 0, 1234}) {
            panel.setCurrentMoveIndex(ply);
            QCoreApplication::processEvents();
            QVERIFY(label->sizeHint().width() <= label->width());
        }
        QCOMPARE(label->text(), QStringLiteral("1234手目"));
        QCOMPARE(counter.count, 0);
    }

    // 対局中は手ごとに同じ（多くは空の）コメントを設定し直す。前に表示したのと同じ文書を
    // 利用者が触っていなければ作り直さない。触っていれば、これまでどおり作り直して先頭を表示する
    void sameCommentIsNotRebuilt()
    {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        // パネルは host の子にする（host が消す）
        CommentEditorPanel& panel = *new CommentEditorPanel(&host);
        layout->addWidget(&panel);
        host.resize(360, 200);
        host.show();
        QVERIFY(QTest::qWaitForWindowExposed(&host));
        auto* edit = host.findChild<QTextEdit*>(QStringLiteral("kifuCommentEdit"));
        QVERIFY(edit != nullptr);
        const QString comment = QStringLiteral("角交換から持久戦 https://example.com/kifu\n").repeated(12);

        // 同じ内容の文書を作ったときの姿（比べる基準）
        panel.setCommentText(comment);
        const QString expectedHtml = edit->toHtml();
        const auto verifyShown = [&](const char* where) {
            QVERIFY2(edit->toHtml() == expectedHtml, where);
            QVERIFY2(edit->textCursor().position() == 0 && !edit->textCursor().hasSelection(), where);
            QVERIFY2(edit->verticalScrollBar()->value() == 0, where);
            QVERIFY2(!edit->document()->isUndoAvailable() && !edit->document()->isRedoAvailable(), where);
            QVERIFY2(!panel.hasUnsavedComment(), where);
        };

        QSignalSpy rebuilt(edit, &QTextEdit::textChanged);
        panel.setCommentText(comment);
        QCOMPARE(rebuilt.count(), 0);
        verifyShown("unchanged");

        // 別のコメントなら作り直す
        panel.setCommentText(QString());
        panel.setCommentText(QString());
        QCOMPARE(rebuilt.count(), 1);
        panel.setCommentText(comment);
        QCOMPARE(rebuilt.count(), 2);
        verifyShown("other comment");

        // カーソルを動かした・スクロールした・編集して取り消した・外から書き換えたときは作り直す
        rebuilt.clear();
        edit->moveCursor(QTextCursor::End);
        panel.setCommentText(comment);
        QCOMPARE(rebuilt.count(), 1);
        verifyShown("cursor moved");

        edit->verticalScrollBar()->setValue(edit->verticalScrollBar()->maximum());
        QVERIFY(edit->verticalScrollBar()->value() > 0);
        panel.setCommentText(comment);
        QCOMPARE(rebuilt.count(), 2);
        verifyShown("scrolled");

        edit->insertPlainText(QStringLiteral("追記"));
        edit->undo();
        edit->moveCursor(QTextCursor::Start);
        QVERIFY(edit->document()->isRedoAvailable());
        rebuilt.clear();
        panel.setCommentText(comment);
        QCOMPARE(rebuilt.count(), 1);
        verifyShown("edited and undone");

        edit->setPlainText(QStringLiteral("外から書き換えた"));
        edit->moveCursor(QTextCursor::Start);
        rebuilt.clear();
        panel.setCommentText(comment);
        QCOMPARE(rebuilt.count(), 1);
        verifyShown("replaced from outside");
    }

    void moveAndTimeColumnsStayStable_data()
    {
        QTest::addColumn<bool>("western");
        QTest::addColumn<bool>("promoteBishop");
        QTest::newRow("japanese-promotion") << false << true;
        QTest::newRow("japanese-no-promotion") << false << false;
        QTest::newRow("western-promotion") << true << true;
        QTest::newRow("western-no-promotion") << true << false;
    }

    // 開始局面の時点で、成り・不成・４桁の手数・時間の桁変化を収める幅があること。
    // 通常のデリゲートで測った実際の文字幅とも比較し、安定化のために文字を切っていないか確認する。
    void moveAndTimeColumnsStayStable()
    {
        QFETCH(bool, western);
        QFETCH(bool, promoteBishop);
        const auto saved = KifuPresentation::options();
        const auto restore = qScopeGuard([saved] {
            KifuPresentation::configure(QStringLiteral("ja_JP"),
                saved.notation == KifuPresentation::Notation::Japanese ? QStringLiteral("japanese")
                                                                      : QStringLiteral("western"),
                saved.alwaysOrigin);
        });
        KifuPresentation::configure(QStringLiteral("ja_JP"),
            western ? QStringLiteral("western") : QStringLiteral("japanese"), true);
        QStringList moves = QStringLiteral(
            "7g7f 7a6b 2g2f 8c8d 2f2e 3c3d 2e2d 2c2d 2h2d 4a3b 2d2h 6c6d 7i7h").split(' ');
        moves.append(promoteBishop ? QStringLiteral("2b8h+") : QStringLiteral("2b8h"));
        const QStringList cycle{QStringLiteral("2h3h"), QStringLiteral("8h9g"),
                                QStringLiteral("3h2h"), QStringLiteral("9g8h")};
        while (moves.size() < 1000) moves.append(cycle.at((moves.size() - 14) % 4));
        const auto steps = TestReplay::play(SfenUtils::hirateSfen(), moves);
        QVERIFY(steps.has_value());
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        QList<KifuBranchNode*> path{tree.root()};
        for (const auto& [move, sfen] : *steps) {
            KifMoveText::Notes notes;
            notes.declinedPromotion = KifMoveText::couldPromote(move.movingPiece, move.move);
            const QString label = KifMoveText::format(path.size() % 2 ? Turn::Black : Turn::White,
                                                     move.movingPiece, move.move, {}, notes);
            auto* node = TestTree::addMove(tree, path.last(), move, label, sfen);
            const qint64 seconds = path.size() % 60;
            node->record().time = RecordTime::fromMilliseconds(seconds * 61000,
                (path.size() % 100) * 3600000 + seconds * 61000);
            path.append(node);
        }
        // 分岐の印も含む。
        TestTree::addMove(tree, tree.root(), ShogiMove(UsiMove::fromString(u"2g2f"), Piece::BlackPawn),
                          QStringLiteral("▲２六歩(27)"), SfenUtils::hirateSfen());
        KifuRecordListModel model(&tree);
        model.setPath({tree.root()});
        QTableView view;
        view.setFont(QFont(QStringLiteral("Noto Serif CJK JP"), 11));
        view.setWordWrap(false);
        view.setModel(&model);
        view.setItemDelegate(new RecordHighlightDelegate(&view));
        view.setColumnHidden(2, true);
        view.setColumnHidden(3, true);
        view.horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
        view.resize(800, 300);
        view.show();
        QVERIFY(QTest::qWaitForWindowExposed(&view));
        view.resizeColumnsToContents();
        const int moveWidth = view.columnWidth(0);
        const int timeWidth = view.columnWidth(1);
        QStyledItemDelegate plain;
        QStyleOptionViewItem option;
        option.initFrom(&view);
        option.font = view.font();
        option.fontMetrics = view.fontMetrics();
        option.widget = &view;
        for (int ply : {9, 10, 13, 14, 15, 99, 100, 999, 1000, 0}) {
            model.setPath(path.mid(0, ply + 1));
            view.resizeColumnsToContents();
            QCOMPARE(view.columnWidth(0), moveWidth);
            QCOMPARE(view.columnWidth(1), timeWidth);
            for (int column : {0, 1}) {
                const QModelIndex index = model.index(ply, column);
                QVERIFY2(view.columnWidth(column) >= plain.sizeHint(option, index).width(),
                         qPrintable(index.data().toString()));
            }
        }
        QFont larger = view.font();
        larger.setPointSize(18);
        view.setFont(larger);
        view.resizeColumnsToContents();
        QVERIFY(view.columnWidth(0) > moveWidth);
        QVERIFY(view.columnWidth(1) > timeWidth);
    }

    // 棋譜欄の長いコメントは、表示幅より十分先で切ってから描く。切っても、全文を渡して
    // 省略表示（…）にした場合とコメント列の絵が同じになる。
    void longCommentsRenderSameWhenCut()
    {
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        const QString sentence = QStringLiteral("ここで▲７六歩と突くのが本筋で、△３四歩なら角交換から持久戦になる。");
        QList<KifuBranchNode*> path{tree.root()};
        for (int row = 1; row < 14; ++row) {
            KifuBranchNode* node = TestTree::addMove(tree, path.last(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
                                                QStringLiteral("sfen-%1").arg(row),
                                                QStringLiteral("00:01/00:00:%1").arg(row, 2, 10, QLatin1Char('0')));
            node->record().comment = (sentence.repeated(1 + row % 5) + QStringLiteral(" (%1)").arg(row));
            path.append(node);
        }
        // 5手目を分岐にして、分岐の印の付いた行も比べる
        TestTree::addMove(tree, path.at(4), ShogiMove(), QStringLiteral("△３四歩(33)"), QStringLiteral("branch"));
        KifuRecordListModel model(&tree);
        model.setPath(path);
        model.setCurrentHighlightRow(3);
        for (const int commentWidth : {80, 240, 600}) {
            QTableView cut;
            QTableView plain;
            for (QTableView* view : {&cut, &plain}) {
                view->setModel(&model);
                view->setStyleSheet(RecordPaneAppearanceManager::kifuTableStyleSheet(10));
                view->setWordWrap(false);
                view->setTextElideMode(Qt::ElideRight);
                view->verticalHeader()->setVisible(false);
                view->horizontalHeader()->setStretchLastSection(false);
                view->horizontalHeader()->resizeSection(3, commentWidth);
                view->resize(320 + commentWidth, 420);
                view->selectRow(6);
                view->show();
                QVERIFY(QTest::qWaitForWindowExposed(view));
            }
            cut.setItemDelegate(new RecordHighlightDelegate(&cut));
            plain.setItemDelegate(new QStyledItemDelegate(&plain));
            QCoreApplication::processEvents();
            // 現在行の目印（指し手の列だけに描く）を除き、コメント列を比べる
            const QRect column(cut.columnViewportPosition(3), 0, cut.columnWidth(3), cut.viewport()->height());
            const QImage expected = plain.viewport()->grab(column).toImage();
            const QImage actual = cut.viewport()->grab(column).toImage();
            QVERIFY(!actual.isNull());
            QVERIFY2(actual == expected, qPrintable(QStringLiteral("comment column width %1").arg(commentWidth)));
        }
    }

    // 対局外で指した手は SFEN 履歴に入らない。履歴の長さで横軸をいったん縮めると、
    // 1手ごとに目盛りを2回作り直すため、横軸は手数が10手の区切りを越えたときだけ変える。
    void graphAxisOnlyGrowsWhileMovesAreRecorded()
    {
        KifuBranchTree tree;
        buildLine(tree, 120);
        KifuNavigationState nav(&tree);
        LiveGameSession live(&tree, &nav);
        EvaluationChartWidget chart;
        EvaluationGraphPresenter presenter({&chart, &nav, &live});

        QSignalSpy axis(&chart, &EvaluationChartWidget::xAxisSettingsChanged);
        for (int ply = 1; ply <= 120; ++ply) nav.setCurrentNode(TestTree::findByPlyOnMainLine(tree, ply));

        QVERIFY(!axis.isEmpty());
        int previous = 0;
        for (const auto& arguments : std::as_const(axis)) {
            const int limit = arguments.at(0).toInt();
            QVERIFY2(limit >= previous, qPrintable(QStringLiteral("x axis shrank from %1 to %2")
                                                       .arg(previous).arg(limit)));
            previous = limit;
        }
        QCOMPARE(previous, 120);
        QVERIFY2(axis.count() <= 20, qPrintable(QStringLiteral("x axis changed %1 times").arg(axis.count())));
    }
};

QTEST_MAIN(TestMoveRecordUpdates)
#include "tst_move_record_updates.moc"
