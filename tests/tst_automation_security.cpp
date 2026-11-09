#include <QtTest>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QDialog>
#include <QLineEdit>
#include <QMainWindow>
#include <QTemporaryDir>

#include "appactivitystate.h"
#include "automationcommands.h"
#include "automationcontext.h"
#include "automationdispatcher.h"
#include "automationwidgets.h"
#include "branchtree_test_util.h"
#include "csagamedialog.h"
#include "errorkinds.h"
#include "operationpolicy.h"
#include "kifubranchtree.h"
#include "kifunavigationcontroller.h"
#include "kifunavigationstate.h"
#include "networksettings.h"

class TestAutomationSecurity : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;

    static QJsonObject call(AutomationDispatcher& dispatcher, const QString& method, const QJsonObject& params)
    {
        return dispatcher.handleRequest({{QStringLiteral("jsonrpc"), QStringLiteral("2.0")},
            {QStringLiteral("id"), 1}, {QStringLiteral("method"), method}, {QStringLiteral("params"), params}});
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    /// 自動化 API は GUI の規則を迂回しない。kifu.goto は棋譜欄と同じ規則で、対局中・棋譜解析中・局面編集中は
    /// 移らずに invalid_state を返す（監査 2026-11-01 F9。OperationPolicy::canBrowseRecord）
    void kifuGotoFollowsTheBrowseRule()
    {
        KifuBranchTree tree;
        tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        KifuBranchNode* first = TestTree::addMove(tree, tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
                                                  QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));
        KifuNavigationState state(&tree);
        KifuNavigationController nav(&state);
        nav.goToNode(first);
        AppActivityState activity;
        OperationPolicy::Deps policyDeps;
        policyDeps.activity = &activity;
        const OperationPolicy policy(policyDeps);
        AutomationContext context;
        context.branchTree = &tree;
        context.activity = &activity;
        context.navState = &state;
        context.navigationController = &nav;
        context.operationPolicy = &policy;
        AutomationDispatcher dispatcher;
        AutomationCommands::registerKifuCommands(dispatcher, context);
        const QJsonObject params{{QStringLiteral("ply"), 0}};

        QObject owner;
        for (const auto refused : {AppActivityState::Activity::DuringGame, AppActivityState::Activity::DuringAnalysis,
                                   AppActivityState::Activity::DuringPositionEdit}) {
            const ActivityLease lease = activity.begin(refused, &owner);
            const QJsonObject response = call(dispatcher, QStringLiteral("kifu.goto"), params);
            QCOMPARE(response.value(QStringLiteral("error")).toObject().value(QStringLiteral("data")).toObject()
                         .value(QStringLiteral("slug")).toString(),
                     ErrorKinds::slug(ErrorKind::InvalidState));
            QCOMPARE(state.currentNode(), first);
        }
        // 検討中は移れる（検討は表示中の局面を追う）
        const ActivityLease consideration = activity.begin(AppActivityState::Activity::DuringConsideration, &owner);
        const QJsonObject response = call(dispatcher, QStringLiteral("kifu.goto"), params);
        QVERIFY2(!response.contains(QStringLiteral("error")), QJsonDocument(response).toJson().constData());
        QCOMPARE(state.currentNode(), tree.root());
    }

    /// モーダルのダイアログが開いている間は、棋譜を移らない（ダイアログが持っているノードと食い違う）
    void kifuGotoIsRefusedWhileAModalDialogIsOpen()
    {
        KifuBranchTree tree;
        tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1"));
        KifuBranchNode* first = TestTree::addMove(tree, tree.root(), ShogiMove(), QStringLiteral("▲７六歩(77)"),
                                                  QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));
        KifuNavigationState state(&tree);
        KifuNavigationController nav(&state);
        state.setCurrentNode(first);
        AutomationContext context;
        context.branchTree = &tree;
        context.navState = &state;
        context.navigationController = &nav;
        AutomationDispatcher dispatcher;
        AutomationCommands::registerKifuCommands(dispatcher, context);

        QDialog dialog;
        dialog.setModal(true);
        dialog.show();
        QTRY_VERIFY(QApplication::activeModalWidget() == &dialog);
        const QJsonObject response = call(dispatcher, QStringLiteral("kifu.goto"), {{QStringLiteral("ply"), 0}});
        QCOMPARE(response.value(QStringLiteral("error")).toObject().value(QStringLiteral("data")).toObject()
                     .value(QStringLiteral("slug")).toString(),
                 ErrorKinds::slug(ErrorKind::InvalidState));
        QCOMPARE(state.currentNode(), first);
    }

    /// エンジンへのコマンドの欄とファイルの置き場所の欄は、非制限モードの書き込みでも使えない
    void forbiddenControlsRejectEvenSensitiveWrites()
    {
        QLineEdit edit;
        edit.setProperty("automationForbidden", true);
        edit.show();
        QVERIFY_THROWS_EXCEPTION(AutomationError, AutomationWidgets::requireInteractive(&edit, false, true));
    }

    void manuallyRevealedPasswordRemainsPrivate()
    {
        QMainWindow main;
        CsaGameDialog dialog(&main);
        auto* password = dialog.findChild<QLineEdit*>(QStringLiteral("lineEditPassword"));
        auto* reveal = dialog.findChild<QCheckBox*>(QStringLiteral("checkBoxShowPassword"));
        QVERIFY(password);
        QVERIFY(reveal);
        password->setText(QStringLiteral("audit-secret"));
        dialog.show();
        reveal->setChecked(true); // A user may reveal it locally; RPC still must not expose it.
        QCOMPARE(password->echoMode(), QLineEdit::Normal);
        AutomationContext context;
        context.mainWindow = &main;
        AutomationDispatcher dispatcher;
        AutomationCommands::registerScreenshotCommands(dispatcher, context);
        AutomationCommands::registerWidgetCommands(dispatcher, context);
        const QString target = AutomationWidgets::selector(&dialog);
        const auto text = call(dispatcher, QStringLiteral("widget.text"),
            {{QStringLiteral("target"), target}, {QStringLiteral("widget"), QStringLiteral("lineEditPassword")}});
        QVERIFY(!text.contains(QStringLiteral("error")));
        QCOMPARE(text[QStringLiteral("result")].toObject()[QStringLiteral("widgets")].toArray().first()
                     .toObject()[QStringLiteral("text")].toString(), QStringLiteral("*****"));
        const auto screenshot = call(dispatcher, QStringLiteral("screenshot.capture"),
            {{QStringLiteral("target"), target}, {QStringLiteral("output_dir"), m_config.path()}});
        QCOMPARE(screenshot[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(),
                 AutomationErrorCode::NotAllowed);
        const auto click = call(dispatcher, QStringLiteral("widget.click"),
            {{QStringLiteral("target"), target}, {QStringLiteral("widget"), QStringLiteral("checkBoxShowPassword")}});
        QCOMPARE(click[QStringLiteral("error")].toObject()[QStringLiteral("code")].toInt(),
                 AutomationErrorCode::NotAllowed);
    }

    void passwordAcceptsWriteOnlyInput()
    {
        QMainWindow main;
        CsaGameDialog dialog(&main);
        dialog.show();
        auto* password = dialog.findChild<QLineEdit*>(QStringLiteral("lineEditPassword"));
        QVERIFY(password);
        AutomationContext context;
        context.mainWindow = &main;
        AutomationDispatcher dispatcher;
        AutomationCommands::registerWidgetCommands(dispatcher, context);
        const auto response = call(dispatcher, QStringLiteral("widget.setValue"),
            {{QStringLiteral("target"), AutomationWidgets::selector(&dialog)},
             {QStringLiteral("widget"), QStringLiteral("lineEditPassword")},
             {QStringLiteral("value"), QStringLiteral("audit-secret")}});
        QVERIFY(!response.contains(QStringLiteral("error")));
        QVERIFY(!QJsonDocument(response).toJson().contains("audit-secret"));
        QTRY_COMPARE(password->text(), QStringLiteral("audit-secret"));
        QCOMPARE(password->echoMode(), QLineEdit::Password);
    }

    void tlsSettingDefaultsAndRoundTrips()
    {
        NetworkSettings::csaUseTls.remove();
        QVERIFY(!NetworkSettings::csaUseTls());
        CsaGameDialog local;
        QVERIFY(!(local.selection().start.transport == CsaClient::Transport::Tls));
        auto* transport = local.findChild<QComboBox*>(QStringLiteral("comboBoxTransport"));
        auto* warning = local.findChild<QLabel*>(QStringLiteral("labelPlaintextWarning"));
        QVERIFY(transport);
        QVERIFY(warning);
        QCOMPARE(transport->currentIndex(), 0);
        QVERIFY(!warning->isHidden());
        transport->setCurrentIndex(1);
        QVERIFY((local.selection().start.transport == CsaClient::Transport::Tls));
        QVERIFY(warning->isHidden());
        transport->setCurrentIndex(0);
        QVERIFY(!(local.selection().start.transport == CsaClient::Transport::Tls));
        QVERIFY(!warning->isHidden());
        NetworkSettings::csaUseTls.set(true);
        CsaGameDialog tls;
        QVERIFY((tls.selection().start.transport == CsaClient::Transport::Tls));
    }
};

QTEST_MAIN(TestAutomationSecurity)
#include "tst_automation_security.moc"
