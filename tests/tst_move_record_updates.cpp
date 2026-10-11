#include <QtTest>
#include <QSignalSpy>
#include <QLabel>
#include <QTemporaryDir>
#include <QVBoxLayout>

#include "commenteditorpanel.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphcontroller.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "kifudisplaypresenter.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "sfenutils.h"
#include "shogimove.h"

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
            ShogiMove(QPoint(1, 7), QPoint(2, 7), Piece::BlackRook, Piece::None, false),
            ShogiMove(QPoint(7, 1), QPoint(6, 1), Piece::WhiteRook, Piece::None, false),
            ShogiMove(QPoint(2, 7), QPoint(1, 7), Piece::BlackRook, Piece::None, false),
            ShogiMove(QPoint(6, 1), QPoint(7, 1), Piece::WhiteRook, Piece::None, false),
        };
        const QString texts[] = {QStringLiteral("▲３八飛(28)"), QStringLiteral("△７二飛(82)"),
                                 QStringLiteral("▲２八飛(38)"), QStringLiteral("△８二飛(72)")};
        for (int ply = 1; ply <= plies; ++ply) {
            node = tree.addMoveQuiet(node, moves[(ply - 1) % 4], texts[(ply - 1) % 4],
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
        KifuNavigationState state;
        state.setTree(&tree);
        state.setCurrentNode(last);
        KifuRecordListModel model;
        KifuDisplayPresenter presenter;
        KifuDisplayPresenter::Refs refs;
        refs.tree = &tree;
        refs.state = &state;
        refs.recordModel = &model;
        presenter.updateRefs(refs);

        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        presenter.populateRecordModel();
        QCOMPARE(model.rowCount(), 121);
        QCOMPARE(inserted.count(), 1);
        QVERIFY(model.item(120)->currentMove().contains(QStringLiteral("８二飛")));

        // 内容が変わっていなければ何もしない
        inserted.clear();
        QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
        presenter.populateRecordModelFromPath(tree.pathToNode(last), 120);
        QCOMPARE(model.rowCount(), 121);
        QCOMPARE(inserted.count(), 0);
        QCOMPARE(reset.count(), 0);

        // 末尾に1手増えたときは、その1行だけを足す（全行を作り直さない）
        KifuBranchNode* next = tree.addMoveQuiet(
            last, ShogiMove(QPoint(1, 7), QPoint(2, 7), Piece::BlackRook, Piece::None, false),
            QStringLiteral("▲３八飛(28)"), QStringLiteral("sfen-121"));
        state.setCurrentNode(next);
        presenter.populateRecordModel();
        QCOMPARE(model.rowCount(), 122);
        QCOMPARE(reset.count(), 0);
        QCOMPARE(inserted.count(), 1);
        QCOMPARE(inserted.at(0).at(1).toInt(), 121);
        QCOMPARE(inserted.at(0).at(2).toInt(), 121);
        QVERIFY(model.item(121)->currentMove().contains(QStringLiteral("３八飛")));

        // 途中までの手順に替わったときは作り直す
        presenter.populateRecordModelFromPath(tree.pathToNode(tree.findByPlyOnMainLine(60)), 60);
        QCOMPARE(model.rowCount(), 61);
        QCOMPARE(reset.count(), 1);
    }

    // 分岐の印が変わらなければ、全行の変更通知（表全体の描き直し）を出さない
    void unchangedBranchMarksDoNotNotify()
    {
        KifuRecordListModel model;
        for (int i = 0; i < 10; ++i)
            model.appendItem(new KifuDisplay(QStringLiteral("%1 ▲７六歩(77)").arg(i), QString(), QString(), &model));
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        model.setBranchPlyMarks({3});
        QCOMPARE(changed.count(), 1);
        model.setBranchPlyMarks({3});
        QCOMPARE(changed.count(), 1);
        model.setBranchPlyMarks({});
        QCOMPARE(changed.count(), 2);
    }

    // コメント欄の手数の表示は手が進むたびに変わる。大きさを固定して、表示が変わっても
    // レイアウトの再計算（メインウィンドウ全体の配置計算につながる）を起こさない
    void commentPositionLabelDoesNotRelayout()
    {
        QWidget host;
        auto* layout = new QVBoxLayout(&host);
        CommentEditorPanel panel;
        layout->addWidget(panel.buildCommentUi(&host));
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

    // 対局外で指した手は SFEN 履歴に入らない。履歴の長さで横軸をいったん縮めると、
    // 1手ごとに目盛りを2回作り直すため、横軸は手数が10手の区切りを越えたときだけ変える。
    void graphAxisOnlyGrowsWhileMovesAreRecorded()
    {
        EvaluationChartWidget chart;
        EvaluationGraphController controller;
        controller.setEvalChart(&chart);
        QStringList sfenHistory{SfenUtils::hirateSfen()};
        controller.setSfenRecord(&sfenHistory);

        QSignalSpy axis(&chart, &EvaluationChartWidget::xAxisSettingsChanged);
        for (int ply = 1; ply <= 120; ++ply) controller.setCurrentPly(ply);

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
