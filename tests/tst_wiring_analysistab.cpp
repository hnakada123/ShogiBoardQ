/// 本体の解析タブと受け手を組み立て、シグナルによる画面・モデルの変化を検証する。
#include <QtTest>
#include <QTemporaryDir>
#include <QPlainTextEdit>
#include <QTimer>
#include <QMessageBox>
#include "mainwindow.h"
#include "mainwindowactions.h"
#include "mainwindowcomposer.h"
#include "branchtreeview.h"
#include "commenteditorpanel.h"
#include "considerationpanel.h"
#include "dialoglauncher.h"
#include "enginelistsettings.h"
#include "engineprofilestore.h"
#include "thinkingpanel.h"
#include "usilogpanel.h"
#include "kifudocument.h"
#include "kifubranchtree.h"
#include "branchtree_test_util.h"
#include "kifunavigationstate.h"
#include "kifunavigationcontroller.h"
#include "shogienginethinkingmodel.h"
#include "pvboarddialog.h"
#include "gamesession.h"
#include "sfenutils.h"
#include "test_replay.h"

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
    void branchAndCommentSignalsUpdateRecord()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        p.actions->startNewRecord();
        const QStringList usi{"7g7f"};
        const auto moves = TestReplay::buildGameMoves(SfenUtils::hirateSfen(), usi);
        const auto sfens = TestReplay::buildSfenRecord(SfenUtils::hirateSfen(), usi, false);
        auto* node = TestTree::addMove(*p.branchTree, p.branchTree->root(), moves.first(), QStringLiteral("▲７六歩"), sfens.last());
        QVERIFY(node);
        emit p.branchTreeView->nodeActivated(node->nodeId());
        QCOMPARE(p.navState->currentNode(), node);
        emit p.commentEditor->commentUpdated(QStringLiteral("実物の配線によるコメント"));
        QCOMPARE(node->comment(), QStringLiteral("実物の配線によるコメント"));
        QVERIFY(p.document->isModified());
        p.document->markSaved(QString());
    }
    void usiCommandReportsUnavailableEngine()
    {
        MainWindow window;
        auto& p = window.composer().parts();
        emit p.usiLog->usiCommandRequested(2, QStringLiteral("isready"));
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
        ThinkingInfoRow row;
        row.pv = PvText::play(SfenUtils::hirateSfen(), QStringLiteral("7g7f"));
        p.thinking1->prependRow(row, 10);
        emit p.thinkingPanel->pvRowClicked(p.thinking1, 0);
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
        emit window.composer().parts().considerationTab->engineSettingsRequested({QStringLiteral("TestEngine"), QString(), QString()});
        QCOMPARE(m_seenDialog, QStringLiteral("ChangeEngineSettingsDialog"));
    }
    /// エンジン登録ダイアログを閉じたら、検討タブのエンジンの一覧を読み直す（再起動しなくても選べる）
    void engineRegistrationReloadsConsiderationEngines()
    {
        MainWindow window;
        auto* tab = window.composer().parts().considerationTab;
        QVERIFY(tab->settings().engineName.isEmpty());
        EngineEntry engine;
        engine.name = QStringLiteral("NewEngine");
        engine.path = QStringLiteral("/engines/new/engine");
        EngineProfileStore::saveEngines({engine});
        QTimer::singleShot(0, this, &TestWiringAnalysisTab::rejectModal);
        window.composer().parts().dialogLauncher->displayEngineSettingsDialog();
        QCOMPARE(tab->settings().engineName, QStringLiteral("NewEngine"));
        EngineProfileStore::saveEngines({});
    }
    void considerationSignalReportsMissingEngine()
    {
        MainWindow window;
        m_seenDialog.clear();
        m_seenMessage.clear();
        const auto previousActivity = window.composer().activity().activity();
        QTimer::singleShot(0, this, &TestWiringAnalysisTab::rejectModal);
        emit window.composer().parts().considerationTab->startConsiderationRequested();
        QCOMPARE(m_seenDialog, QStringLiteral("QMessageBox"));
        QCOMPARE(m_seenMessage, QStringLiteral("将棋エンジンが選択されていません。"));
        QCOMPARE(window.composer().activity().activity(), previousActivity);
    }
};
QTEST_MAIN(TestWiringAnalysisTab)
#include "tst_wiring_analysistab.moc"
