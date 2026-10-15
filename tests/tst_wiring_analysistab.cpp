/// 本体の解析タブと受け手を組み立て、シグナルによる画面・モデルの変化を検証する。
#include <QtTest>
#include <QTemporaryDir>
#include <QPlainTextEdit>
#include <QTimer>
#include <QMessageBox>
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "analysistabwiring.h"
#include "engineanalysistab.h"
#include "gamerecordmodel.h"
#include "kifubranchtree.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "shogienginethinkingmodel.h"
#include "pvboarddialog.h"
#include "sfenpositiontracer.h"
#include "sessionlifecyclecoordinator.h"

class TestWiringAnalysisTab : public QObject {
    Q_OBJECT
    QTemporaryDir m_config;
    QString m_seenDialog;
    QString m_seenMessage;
public:
    void rejectModal()
    {
        auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
        if (dialog) {
            m_seenDialog = QString::fromLatin1(dialog->metaObject()->className());
            if (auto* message = qobject_cast<QMessageBox*>(dialog)) m_seenMessage = message->text();
            dialog->reject();
        }
    }
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }
    void creationIsIdempotent()
    {
        QWidget parent;
        AnalysisTabWiring::Deps deps;
        deps.centralParent = &parent;
        AnalysisTabWiring wiring(deps);
        auto* tab = wiring.buildUiAndWire();
        QCOMPARE(wiring.buildUiAndWire(), tab);
        QSignalSpy forwarded(&wiring, &AnalysisTabWiring::branchNodeActivated);
        emit tab->branchNodeActivated(3, 18);
        QCOMPARE(forwarded.count(), 1);
        QCOMPARE(forwarded.first().at(0).toInt(), 3);
        QCOMPARE(forwarded.first().at(1).toInt(), 18);
    }
    void branchAndCommentSignalsUpdateRecord()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        p.sessionLifecycle->resetToInitialState();
        const QStringList usi{"7g7f"};
        const auto moves = SfenPositionTracer::buildGameMoves(SfenUtils::hirateSfen(), usi);
        const auto sfens = SfenPositionTracer::buildSfenRecord(SfenUtils::hirateSfen(), usi, false);
        auto* node = p.branchTree->addMove(p.branchTree->root(), moves.first(), QStringLiteral("▲７六歩"), sfens.last());
        QVERIFY(node);
        emit p.analysisTab->branchNodeActivated(0, 1);
        QCOMPARE(p.navState->currentNode(), node);
        emit p.analysisTab->commentUpdated(1, QStringLiteral("実物の配線によるコメント"));
        QCOMPARE(node->comment(), QStringLiteral("実物の配線によるコメント"));
        QVERIFY(p.gameRecord->isDirty());
        p.gameRecord->clearDirty();
    }
    void usiCommandReportsUnavailableEngine()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        emit p.analysisTab->usiCommandRequested(2, QStringLiteral("isready"));
        QString logs;
        const auto editors = window.findChildren<QPlainTextEdit*>();
        for (auto* editor : editors) logs += editor->toPlainText();
        QVERIFY2(logs.contains(QStringLiteral("E1: エンジンが起動していません")), qPrintable(logs));
        QVERIFY(logs.contains(QStringLiteral("E2: エンジンが起動していません")));
    }
    void pvClickOpensActualBoard()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        auto* record = new ShogiInfoRecord({}, {}, {}, {}, QStringLiteral("▲７六歩"), QStringLiteral("7g7f"));
        record->setBaseSfen(SfenUtils::hirateSfen());
        p.thinking1->appendItem(record);
        emit p.analysisTab->pvRowClicked(0, 0);
        PvBoardDialog* board = nullptr;
        const auto widgets = QApplication::topLevelWidgets();
        for (auto* widget : widgets) {
            if (auto* dialog = qobject_cast<PvBoardDialog*>(widget)) board = dialog;
        }
        QVERIFY(board);
        QVERIFY(board->isVisible());
        board->reject();
    }
    void settingsSignalOpensActualDialog()
    {
        MainWindow window;
        m_seenDialog.clear();
        QTimer::singleShot(0, this, &TestWiringAnalysisTab::rejectModal);
        emit window.composer().parts().analysisTab->engineSettingsRequested(0, QStringLiteral("TestEngine"));
        QCOMPARE(m_seenDialog, QStringLiteral("ChangeEngineSettingsDialog"));
    }
    void considerationSignalReportsMissingEngine()
    {
        MainWindow window;
        m_seenDialog.clear();
        m_seenMessage.clear();
        const auto previousMode = window.composer().state().playMode;
        QTimer::singleShot(0, this, &TestWiringAnalysisTab::rejectModal);
        emit window.composer().parts().analysisTab->startConsiderationRequested();
        QCOMPARE(m_seenDialog, QStringLiteral("QMessageBox"));
        QCOMPARE(m_seenMessage, QStringLiteral("将棋エンジンが選択されていません。"));
        QCOMPARE(window.composer().state().playMode, previousMode);
    }
};
QTEST_MAIN(TestWiringAnalysisTab)
#include "tst_wiring_analysistab.moc"
