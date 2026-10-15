/// 局面編集後の開始要求を、本体の盤・棋譜・開始処理で検証する。
#include <QtTest>
#include <QTemporaryDir>
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "gamestartcoordinator.h"
#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "matchcoordinator.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "sfenutils.h"

class TestPositionEditGameStart : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }
    void selectedStartIsRespected_data()
    {
        QTest::addColumn<int>("preset");
        QTest::newRow("edited-current-position") << 0;
        QTest::newRow("explicit-hirate") << 1;
    }
    void selectedStartIsRespected()
    {
        QFETCH(int, preset);
        MainWindow window;
        auto& p = window.composer().parts();
        auto& state = window.composer().state();
        const QString edited = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4 w - 37");
        p.branchTree->setRootSfen(edited);
        p.navState->goToRoot();
        state.startSfenStr = state.currentSfenStr = edited;
        GameStartCoordinator::Ctx ctx;
        ctx.dialogData.startingPositionNumber = preset;
        ctx.dialogData.isHuman1 = ctx.dialogData.isHuman2 = true;
        ctx.currentSfen = state.currentSfenStr;
        ctx.startSfen = state.startSfenStr;
        ctx.recordLine = p.navState->currentRecordLine();
        ctx.startNodeId = p.branchTree->root()->nodeId();
        QString error;
        QVERIFY2(p.gameStart->initializeGame(ctx, error), qPrintable(error));
        const QString expected = preset == 0 ? edited : SfenUtils::hirateSfen();
        QCOMPARE(p.branchTree->root()->sfen(), expected);
        QCOMPARE(p.match->record().startSfen, expected);
        QCOMPARE(p.gameController->currentPlayer(), preset == 0 ? Turn::White : Turn::Black);
        // 終局の知らせ（ダイアログ）を出さずに対局を終える
        p.match->handleBreakOff();
        QVERIFY(p.match->isGameOver());
        p.kifuNavController->goToFirst();
        QCOMPARE(p.navState->currentNode(), p.branchTree->root());
        QCOMPARE(state.currentSfenStr, expected);
        p.gameRecord->clearDirty();
    }
};
QTEST_MAIN(TestPositionEditGameStart)
#include "tst_positionedit_gamestart.moc"
