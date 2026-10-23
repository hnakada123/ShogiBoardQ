/// @file tst_app_branch_navigation.cpp
/// @brief 棋譜を移る操作（ボタン・分岐ツリー・分岐候補欄）を、本体の合成ルートで組み立てた部品で確かめるテスト
///
/// 操作は KifuNavigationController が表示中の手（KifuNavigationState）を動かすだけで、
/// 盤・棋譜欄・分岐候補欄は表示中の手の変化を受けて自分で合わせる。

#include <QtTest>
#include <QChartView>
#include <QComboBox>
#include <QDockWidget>
#include <QPushButton>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "branchtreeview.h"
#include "appactivitystate.h"
#include "evaluationchartwidget.h"
#include "evaluationgraphpresenter.h"
#include "kifudocument.h"
#include "kifubranchlistmodel.h"
#include "kifubranchnode.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifufilecontroller.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "kifurecordlistmodel.h"
#include "livegamesession.h"
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "recordpane.h"
#include "sessionresetcoordinator.h"
#include "sfenutils.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "test_replay.h"

namespace {

const QString kNoLanceSfen =
    QStringLiteral("lnsgkgsn1/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1");

/// 盤の局面（手数を除く）
QString boardPosition(const MainWindowComponents& p)
{
    return p.gameController->board()->toSfen().section(QLatin1Char(' '), 0, 2);
}

QString position(const QString& sfen)
{
    return sfen.section(QLatin1Char(' '), 0, 2);
}

} // namespace

class TestAppBranchNavigation : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    /// 本譜5手と、2手目から分かれる変化（△８四歩 ▲２六歩）を読み込む
    static KifuBranchNode* loadRecord(MainWindowComponents& p)
    {
        if (!p.kifuFileController->loadKifuText(QStringLiteral("position startpos moves 7g7f 3c3d 2g2f 8c8d 2f2e")))
            return nullptr;
        KifuBranchNode* ply1 = TestTree::findByPlyOnMainLine(*p.branchTree, 1);
        const QStringList usi{QStringLiteral("8c8d"), QStringLiteral("2g2f")};
        const QStringList sfens = TestReplay::buildSfenRecord(ply1->sfen(), usi, false);
        const QList<ShogiMove> moves = TestReplay::buildGameMoves(ply1->sfen(), usi);
        KifuBranchNode* branch2 = TestTree::addMove(*p.branchTree, ply1, moves.at(0), QStringLiteral("△８四歩(83)"), sfens.at(1));
        TestTree::addMove(*p.branchTree, branch2, moves.at(1), QStringLiteral("▲２六歩(27)"), sfens.at(2));
        p.document->markSaved(QString());
        p.kifuNavController->goToFirst();
        return branch2;
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void navigationButtonsMoveTheCursorAndTheBoard()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(loadRecord(p));
        QSignalSpy changed(p.navState, &KifuNavigationState::changed);

        p.recordPane->nextButton()->click();
        QCOMPARE(p.navState->currentPly(), 1);
        QCOMPARE(changed.count(), 1);
        QCOMPARE(boardPosition(p), position(p.navState->currentSfen()));
        QCOMPARE(p.kifuRecord->currentHighlightRow(), 1);

        p.recordPane->lastButton()->click();
        QCOMPARE(p.navState->currentPly(), 5);
        p.recordPane->prevButton()->click();
        QCOMPARE(p.navState->currentPly(), 4);
        QCOMPARE(boardPosition(p), position(p.navState->currentSfen()));
        p.recordPane->firstButton()->click();
        QCOMPARE(p.navState->currentNode(), p.branchTree->root());
        QCOMPARE(boardPosition(p), position(SfenUtils::hirateSfen()));
        QCOMPARE(p.kifuRecord->currentHighlightRow(), 0);
    }

    void evaluationGraphNavigationFollowsGameState_data()
    {
        QTest::addColumn<AppActivityState::Activity>("playing");
        QTest::newRow("local-game") << AppActivityState::Activity::DuringGame;
        QTest::newRow("csa-game") << AppActivityState::Activity::DuringCsaGame;
    }

    /// 対局・再開でグラフからの局面移動を止め、終局・中断で再び操作できる。
    void evaluationGraphNavigationFollowsGameState()
    {
        QFETCH(AppActivityState::Activity, playing);
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(loadRecord(p));
        p.kifuNavController->goToLast();
        p.evalGraph->plotEngineScore(Turn::Black, 1, UsiScore::cp(100));
        p.evalGraph->plotEngineScore(Turn::Black, 5, UsiScore::cp(200));
        auto* graph = p.evalChart;
        window.resize(1400, 900);
        window.show();
        for (auto* dock : window.findChildren<QDockWidget*>()) {
            if (dock->widget() == graph) { dock->show(); dock->raise(); }
        }
        auto* chartView = qobject_cast<QChartView*>(graph->chartViewWidget());
        QVERIFY(chartView);
        QTRY_VERIFY(chartView->isVisible());
        QTRY_VERIFY(chartView->chart()->plotArea().width() > 0);
        const auto clickPoint = [&]() {
            const QPoint point = chartView->mapFromScene(chartView->chart()->mapToScene(
                chartView->chart()->mapToPosition(QPointF(1, 100))));
            QTest::mouseClick(chartView->viewport(), Qt::LeftButton, Qt::NoModifier, point);
        };
        const QList<QPushButton*> buttons = {graph->firstButton(), graph->back10Button(), graph->prevButton(),
                                             graph->nextButton(), graph->fwd10Button(), graph->lastButton()};
        auto& activity = window.composer().activity();
        // 同じグラフで対局開始→終局→再開→中断に相当する状態遷移を確かめる。
        for (int cycle = 0; cycle < 2; ++cycle) {
            ActivityLease lease = activity.begin(playing, nullptr);
            const QString currentPosition = boardPosition(p);
            QSignalSpy changed(p.navState, &KifuNavigationState::changed);
            for (auto* button : buttons) {
                QVERIFY(!button->isEnabled());
                button->click();
            }
            clickPoint();
            QCOMPARE(changed.count(), 0);
            QCOMPARE(p.navState->currentPly(), 5);
            QCOMPARE(boardPosition(p), currentPosition);
            QCOMPARE(graph->currentPly(), 5);
            QVERIFY(graph->findChild<QPushButton*>(QStringLiteral("evalDisplaySettings"))->isEnabled());
            QVERIFY(graph->findChild<QComboBox*>(QStringLiteral("evalRangeMode"))->isEnabled());

            lease.reset();
            for (auto* button : buttons) QVERIFY(button->isEnabled());
            clickPoint();
            QCOMPARE(p.navState->currentPly(), 1);
            QCOMPARE(graph->currentPly(), 1);
            QCOMPARE(boardPosition(p), position(p.navState->currentSfen()));
            QVERIFY(boardPosition(p) != currentPosition);
            graph->firstButton()->click();
            QCOMPARE(p.navState->currentPly(), 0);
            graph->lastButton()->click();
            QCOMPARE(p.navState->currentPly(), 5);
        }
    }

    /// 分岐ツリーのノードをクリックすると、その手順のその手へ移り、盤・棋譜欄・分岐候補欄が合う
    void branchTreeClickMovesToTheChosenLine()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        KifuBranchNode* branch2 = loadRecord(p);
        QVERIFY(branch2);

        emit p.branchTreeView->nodeActivated(branch2->nodeId());

        QCOMPARE(p.navState->currentNode(), branch2);
        QCOMPARE(p.navState->currentLineIndex(), 1);
        QCOMPARE(boardPosition(p), position(branch2->sfen()));
        QCOMPARE(p.kifuRecord->rowCount(), 4);   // 開始局面〜変化の終わり（3手目）
        QCOMPARE(p.kifuRecord->currentHighlightRow(), 2);
        QCOMPARE(p.kifuBranch->candidateCount(), 2);
        QVERIFY(p.kifuBranch->hasBackToMainRow());
        QCOMPARE(p.kifuBranch->currentHighlightRow(), 1);
    }

    /// 分岐候補欄の行を選ぶと、その手へ移る。末尾の「本譜へ戻る」は本譜の同じ手数へ移る
    void branchCandidateRowsSwitchLines()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        KifuBranchNode* branch2 = loadRecord(p);
        QVERIFY(branch2);
        p.kifuNavController->goToPly(2);
        QCOMPARE(p.kifuBranch->candidateCount(), 2);
        QVERIFY(!p.kifuBranch->hasBackToMainRow());

        emit p.recordPane->branchRowActivated(1);
        QCOMPARE(p.navState->currentNode(), branch2);
        QVERIFY(p.kifuBranch->hasBackToMainRow());

        emit p.recordPane->branchRowActivated(2);
        QCOMPARE(p.navState->currentNode(), TestTree::findByPlyOnMainLine(*p.branchTree, 2));
        QVERIFY(p.navState->isOnMainLine());
        QCOMPARE(boardPosition(p), position(p.navState->currentSfen()));
    }

    /// 棋譜欄の行を選ぶと、表示中の手順のその手へ移る
    void recordRowSelectionMovesWithinTheShownLine()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        KifuBranchNode* branch2 = loadRecord(p);
        QVERIFY(branch2);
        p.kifuNavController->goToNode(branch2->childAt(0));

        emit p.recordPane->mainRowChanged(2);
        QCOMPARE(p.navState->currentNode(), branch2);
        QCOMPARE(p.navState->currentLineIndex(), 1);
        emit p.recordPane->mainRowChanged(0);
        QCOMPARE(p.navState->currentNode(), p.branchTree->root());
        // 開始局面へ戻っても、選んでいた変化の手順のまま
        QCOMPARE(p.navState->currentLineIndex(), 1);
    }

    /// 新しい対局の準備では、ライブ対局を捨てて新しい開始局面だけの棋譜にし、分岐の選択も忘れる
    void resetForNewGameDiscardsLiveSessionAndReturnsToRoot()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        QVERIFY(loadRecord(p));
        p.kifuNavController->goToPlyOnLine(p.branchTree->allLines().at(1).leaf->nodeId(), 3);
        QCOMPARE(p.navState->currentLineIndex(), 1);
        p.liveGameSession->startFromRoot();
        QVERIFY(p.liveGameSession->isActive());

        KifuBranchNode* root = p.sessionReset->prepareGameStart({GameStartPreparation::Mode::NewGame, kNoLanceSfen, -1});

        QCOMPARE(root, p.branchTree->root());
        QCOMPARE(p.branchTree->root()->sfen(), kNoLanceSfen);
        QCOMPARE(p.navState->currentNode(), p.branchTree->root());
        QCOMPARE(p.navState->currentLineIndex(), 0);
        QCOMPARE(boardPosition(p), position(kNoLanceSfen));
        p.liveGameSession->discard();
        p.document->markSaved(QString());
    }
};

QTEST_MAIN(TestAppBranchNavigation)

#include "tst_app_branch_navigation.moc"
