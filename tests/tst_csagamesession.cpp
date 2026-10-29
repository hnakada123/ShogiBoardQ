/// @file tst_csagamesession.cpp
/// @brief CsaGameSession が通信対局の司令塔の知らせを画面（ステータスバー・ダイアログ）に出すことを確かめる

#include <QtTest>
#include <QLabel>
#include <QMessageBox>
#include <QTemporaryDir>
#include <QStatusBar>
#include "csagamecoordinator.h"
#include "csawaitingdialog.h"
#include "csagamesession.h"
#include "notificationpresenter.h"

class TestCsaGameSession : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    /// 司令塔のログはステータスバーに出る
    void logMessageReachesStatusBar()
    {
        CsaGameCoordinator coordinator;
        QStatusBar status;
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.statusBar = &status;
        CsaGameSession session(deps);
        emit coordinator.logMessage(QStringLiteral("connected"), false);
        QCOMPARE(status.currentMessage(), QStringLiteral("connected"));
    }

    /// サーバーから受け取った行は HTML として描かない（リンク・ローカルファイルの画像を出させない。セキュリティ監査 S4）。
    /// 司令塔のエラーは合成ルートが NotificationPresenter::displayErrorMessage へつなぐ
    void serverTextIsShownAsPlainText()
    {
        NotificationPresenter notifications({});
        const QString line = QStringLiteral("REJECT:<a href=\"file:///tmp/x\">click</a>");
        Qt::TextFormat format = Qt::AutoText;
        QString shown;
        QTimer::singleShot(0, this, [&format, &shown]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box) return;
            format = box->textFormat();
            shown = box->text();
            box->accept();
        });
        notifications.displayErrorMessage(line);
        QCOMPARE(format, Qt::PlainText);
        QCOMPARE(shown, line);

        CsaWaitingDialog waiting(nullptr);
        auto* detail = waiting.findChild<QLabel*>(QStringLiteral("csaWaitingDetail"));
        QVERIFY(detail);
        QCOMPARE(detail->textFormat(), Qt::PlainText);
    }

    /// 対局の開始はステータスバーに対局者名で出す。棋譜と動作状態は司令塔が整えるので、ここでは受けない
    void gameStartIsShownInStatusBar()
    {
        CsaGameCoordinator coordinator;
        QStatusBar status;
        CsaGameSession::Deps deps;
        deps.coordinator = &coordinator;
        deps.statusBar = &status;
        CsaGameSession session(deps);

        emit coordinator.gameStarted(QStringLiteral("Black"), QStringLiteral("White"));
        QVERIFY(status.currentMessage().contains(QStringLiteral("Black")));
        QVERIFY(status.currentMessage().contains(QStringLiteral("White")));
    }
};
QTEST_MAIN(TestCsaGameSession)
#include "tst_csagamesession.moc"
