/// @file tst_csagameuicontroller.cpp
/// @brief CsaGameUiController が通信対局の司令塔の知らせを画面（ステータスバー・ダイアログ）に出すことを確かめる

#include <QtTest>
#include <QLabel>
#include <QMessageBox>
#include <QPointer>
#include <QTemporaryDir>
#include <QStatusBar>
#include "csagamecoordinator.h"
#include "csawaitingdialog.h"
#include "csagamedialog.h"
#include "csagameuicontroller.h"
#include "notificationpresenter.h"

class TestCsaGameUiController : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    /// 対局結果・終局の原因の表示名は ui が持つ。どれも空でなく、互いに違う
    void resultAndCauseTextsAreDistinct()
    {
        using Result = CsaClient::GameResult;
        using Cause = CsaClient::GameEndCause;
        QStringList results;
        for (const Result result : {Result::Win, Result::Lose, Result::Draw, Result::Censored, Result::Chudan,
                                    Result::Unknown}) {
            results << CsaGameUiController::resultText(result);
        }
        QStringList causes;
        for (const Cause cause : {Cause::Resign, Cause::TimeUp, Cause::IllegalMove, Cause::Sennichite,
                                  Cause::OuteSennichite, Cause::Jishogi, Cause::MaxMoves, Cause::Chudan,
                                  Cause::IllegalAction, Cause::Unknown}) {
            causes << CsaGameUiController::causeText(cause);
        }
        for (const QStringList& texts : {results, causes}) {
            QVERIFY(!texts.contains(QString()));
            QCOMPARE(QSet<QString>(texts.cbegin(), texts.cend()).size(), texts.size());
        }
        QCOMPARE(CsaGameUiController::resultText(Result::Win), QStringLiteral("勝ち"));
        QCOMPARE(CsaGameUiController::causeText(Cause::Resign), QStringLiteral("投了"));
    }

    /// 司令塔のログはステータスバーに出る
    void logMessageReachesStatusBar()
    {
        CsaGameCoordinator coordinator;
        QStatusBar status;
        CsaGameUiController::Deps deps;
        deps.coordinator = &coordinator;
        deps.statusBar = &status;
        CsaGameUiController session(deps);
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

    /// ui・app の部品が出す知らせ（static な information・warning・critical）も HTML として読まない。
    /// 自動化 API の呼び出し中は出さずに記録し、警告・情報は notices、エラーは errors に入れる
    void staticNotificationsAreCapturedAndPlain()
    {
        const QString text = QStringLiteral("<b>file</b> not found");
        Qt::TextFormat format = Qt::AutoText;
        QMessageBox::Icon icon = QMessageBox::NoIcon;
        QTimer::singleShot(0, this, [&format, &icon]() {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box) return;
            format = box->textFormat();
            icon = box->icon();
            box->accept();
        });
        NotificationPresenter::warning(nullptr, QStringLiteral("title"), text);
        QCOMPARE(format, Qt::PlainText);
        QCOMPARE(icon, QMessageBox::Warning);

        const NotificationPresenter::ScopedCapture capture;
        NotificationPresenter::information(nullptr, QStringLiteral("t"), QStringLiteral("info"));
        NotificationPresenter::warning(nullptr, QStringLiteral("t"), QStringLiteral("warn"));
        NotificationPresenter::critical(nullptr, QStringLiteral("t"), QStringLiteral("error"));
        QCOMPARE(capture.notices(), QStringList({QStringLiteral("info"), QStringLiteral("warn")}));
        QCOMPARE(capture.errors(), QStringList({QStringLiteral("error")}));
        QVERIFY(!QApplication::activeModalWidget());
    }

    /// 続けて対局するときの結果の表示は、閉じるのを待たずに戻る（次の対局の開始を止めない）
    void notificationWithoutWaitingReturnsAtOnce()
    {
        QWidget parent;
        NotificationPresenter::information(&parent, QStringLiteral("result"), QStringLiteral("win"),
                                           NotificationPresenter::Wait::No);
        auto* box = parent.findChild<QMessageBox*>();
        QVERIFY(box);
        QVERIFY(box->isVisible());
        QVERIFY(!box->isModal());
        QCOMPARE(box->textFormat(), Qt::PlainText);
        box->close();
    }

    /// 通信対局のエラーは待たずに出し、続けて届いても1つに差し替える（入れ子のイベントループに入らない）
    void errorsDoNotBlockAndReplaceThePreviousOne()
    {
        CsaGameCoordinator coordinator;
        QWidget parent;
        CsaGameUiController::Deps deps;
        deps.coordinator = &coordinator;
        deps.parentWidget = &parent;
        CsaGameUiController session(deps);
        for (int i = 0; i < 20; ++i) session.showError(QStringLiteral("error %1").arg(i));
        QVERIFY(!QApplication::activeModalWidget());
        QTest::qWait(0);   // close() で消えた箱の削除を走らせる
        QTRY_COMPARE(parent.findChildren<QMessageBox*>().size(), 1);
        auto* box = parent.findChild<QMessageBox*>();
        QCOMPARE(box->text(), QStringLiteral("error 19"));
        QCOMPARE(box->textFormat(), Qt::PlainText);
        box->close();
    }

    /// 接続ダイアログは CsaGameUiController が開き、入力した値を残すため使い回す。キャンセルなら始めない
    void gameDialogIsOpenedAndReused()
    {
        CsaGameCoordinator coordinator;
        QWidget parent;
        CsaGameUiController::Deps deps;
        deps.coordinator = &coordinator;
        deps.parentWidget = &parent;
        CsaGameUiController session(deps);
        for (int i = 0; i < 2; ++i) {
            QTimer::singleShot(0, this, []() {
                if (auto* dialog = qobject_cast<CsaGameDialog*>(QApplication::activeModalWidget())) dialog->reject();
            });
            session.openGameDialog();
        }
        QCOMPARE(parent.findChildren<CsaGameDialog*>().size(), 1);
        QVERIFY(!parent.findChild<CsaWaitingDialog*>());
    }

    /// 対局の開始はステータスバーに対局者名で出す。棋譜と動作状態は司令塔が整えるので、ここでは受けない
    void gameStartIsShownInStatusBar()
    {
        CsaGameCoordinator coordinator;
        QStatusBar status;
        CsaGameUiController::Deps deps;
        deps.coordinator = &coordinator;
        deps.statusBar = &status;
        CsaGameUiController session(deps);

        emit coordinator.gameStarted(QStringLiteral("Black"), QStringLiteral("White"));
        QVERIFY(status.currentMessage().contains(QStringLiteral("Black")));
        QVERIFY(status.currentMessage().contains(QStringLiteral("White")));
    }

    /// 続きの対局の結果の知らせは、前の知らせを閉じてから出す（積み重ねない。監査 2026-11-07 F13）
    void resultNoticeReplacesThePreviousOne()
    {
        QPointer<QMessageBox> notice;
        const auto wait = NotificationPresenter::Wait::No;
        NotificationPresenter::replaceInformation(notice, nullptr, QStringLiteral("t"), QStringLiteral("1局目"), wait);
        QVERIFY(notice);
        QPointer<QMessageBox> first = notice;
        NotificationPresenter::replaceInformation(notice, nullptr, QStringLiteral("t"), QStringLiteral("2局目"), wait);
        QVERIFY(notice && notice != first);
        QTRY_VERIFY(first.isNull());  // 閉じると消える
        QCOMPARE(notice->text(), QStringLiteral("2局目"));
        notice->close();
    }
};
QTEST_MAIN(TestCsaGameUiController)
#include "tst_csagameuicontroller.moc"
