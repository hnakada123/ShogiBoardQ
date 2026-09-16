/// @file tst_ponder_flow.cpp
/// @brief 実GUI・CSAコントローラと模擬USIエンジンによる先読みの回帰テスト
#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTableView>
#include <QTemporaryDir>
#include <QTimer>
#include "mainwindow.h"
#include "startgamedialog.h"
#include "promotedialog.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "recordpane.h"
#include "settingscommon.h"
#include "enginepondersettings.h"
#include "csaenginecontroller.h"
#include "shogigamecontroller.h"
#include "sfenpositiontracer.h"

class TestPonderFlow : public QObject
{
    Q_OBJECT
    std::unique_ptr<MainWindow> m_window;
    QTemporaryDir m_logs;
    QString m_logPath;
    QTimer m_dialogTimer;
    QTimer m_immediateTimer;
    bool m_humanBlack = true;
    bool m_bothEngines = false;
    const QString m_initial = QStringLiteral("lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 1");

    ShogiView* view() const { return m_window->findChild<ShogiView*>(); }
    RecordPane* record() const { return m_window->findChild<RecordPane*>(); }
    int rows() const { return record()->kifuView()->model()->rowCount(); }
    QStringList commands() const
    {
        QFile file(m_logPath);
        if (!file.open(QIODevice::ReadOnly)) return {};
        QStringList result;
        for (const auto& line : QString::fromUtf8(file.readAll()).split('\n', Qt::SkipEmptyParts))
            result.append(line.section(' ', 1)); // プロセスIDを除く
        return result;
    }
    void trigger(const char* name)
    {
        auto* action = m_window->findChild<QAction*>(QString::fromLatin1(name));
        QVERIFY(action);
        QVERIFY(action->isEnabled());
        action->trigger();
        QCoreApplication::processEvents();
    }
    QPoint square(int file, int rank) const
    {
        for (int y = 0; y < view()->height(); y += 8)
            for (int x = 0; x < view()->width(); x += 8)
                if (view()->clickedSquare(QPoint(x, y)) == QPoint(file, rank))
                    return QPoint(x, y) + QPoint(view()->fieldSize().width()/3, view()->fieldSize().height()/3);
        return {};
    }
    void move(const QPoint& from, const QPoint& to)
    {
        QTest::mouseClick(view(), Qt::LeftButton, Qt::NoModifier, square(from.x(), from.y()));
        QTest::mouseClick(view(), Qt::LeftButton, Qt::NoModifier, square(to.x(), to.y()));
    }
    void start()
    {
        m_window = std::make_unique<MainWindow>();
        m_window->resize(1400, 1000);
        m_window->show();
        QTest::qWait(50);
        m_dialogTimer.start(20);
        trigger("actionStartGame");
        QVERIFY(record()->isNavigationDisabled());
    }
    void finish()
    {
        trigger("actionBreakOffGame");
        QTRY_VERIFY(!record()->isNavigationDisabled());
    }

public slots:
    void forceEngineMove()
    {
        if (!commands().join('\n').contains("go btime ")) return;
        m_immediateTimer.stop();
        trigger("actionMakeImmediateMove");
    }
    void handleDialog()
    {
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (!widget->isVisible()) continue;
            if (auto* dialog = qobject_cast<StartGameDialog*>(widget)) {
                dialog->findChild<QComboBox*>("comboBoxPlayer1")->setCurrentIndex(m_bothEngines || !m_humanBlack ? 1 : 0);
                dialog->findChild<QComboBox*>("comboBoxPlayer2")->setCurrentIndex(m_bothEngines || m_humanBlack ? 1 : 0);
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            } else if (auto* promotion = qobject_cast<PromoteDialog*>(widget)) {
                promotion->accept();
            } else if (auto* message = qobject_cast<QMessageBox*>(widget)) {
                if (auto* discard = message->button(QMessageBox::Discard)) discard->click();
                else if (auto* yes = message->button(QMessageBox::Yes)) yes->click();
                else message->accept();
            }
        }
    }
private slots:
    void initTestCase()
    {
        QVERIFY(m_logs.isValid());
        connect(&m_dialogTimer, &QTimer::timeout, this, &TestPonderFlow::handleDialog);
        connect(&m_immediateTimer, &QTimer::timeout, this, &TestPonderFlow::forceEngineMove);
    }
    void init()
    {
        m_humanBlack = true;
        m_bothEngines = false;
        m_logPath = m_logs.filePath(QString::fromLatin1(QTest::currentTestFunction()) + ".log");
        QFile file(m_logPath);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        qputenv("AUDIT_USI_LOG", m_logPath.toUtf8());
        qputenv("AUDIT_ENGINE_PONDER", "1");
        qunsetenv("AUDIT_PONDER_PROMOTION");
        qunsetenv("AUDIT_ENGINE_DELAY");
        qunsetenv("AUDIT_ENGINE_WAIT_FOR_STOP");
        auto& settings = SettingsCommon::openSettings();
        settings.clear();
        settings.beginWriteArray("Engines", 1);
        settings.setArrayIndex(0);
        settings.setValue("name", "Audit USI");
        settings.setValue("path", QStringLiteral(AUDIT_DIR "/mock_usi.py"));
        settings.endArray();
        settings.sync();
        EnginePonderSettings::save("Audit USI", true, true);
    }
    void cleanup()
    {
        m_immediateTimer.stop();
        qInfo().noquote() << commands().join('\n');
        if (m_window) {
            if (QTest::currentTestFailed())
                m_window->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/ponder-failure.png"));
            m_window->close();
            m_window.reset();
        }
        m_dialogTimer.stop();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        qunsetenv("AUDIT_ENGINE_PONDER");
        qunsetenv("AUDIT_PONDER_PROMOTION");
        qunsetenv("AUDIT_ENGINE_WAIT_FOR_STOP");
    }
    void humanPromotion_data()
    {
        QTest::addColumn<bool>("humanBlack");
        QTest::addColumn<bool>("hit");
        QTest::newRow("black-hit") << true << true;
        QTest::newRow("black-miss") << true << false;
        QTest::newRow("white-hit") << false << true;
        QTest::newRow("white-miss") << false << false;
    }
    void humanPromotion()
    {
        QFETCH(bool, humanBlack);
        QFETCH(bool, hit);
        m_humanBlack = humanBlack;
        if (hit) qputenv("AUDIT_PONDER_PROMOTION", "1");
        start();
        if (!humanBlack) QTRY_COMPARE_WITH_TIMEOUT(rows(), 2, 5000);
        move(humanBlack ? QPoint(7, 7) : QPoint(3, 3), humanBlack ? QPoint(7, 6) : QPoint(3, 4));
        QTRY_COMPARE_WITH_TIMEOUT(rows(), humanBlack ? 3 : 4, 5000);
        const auto before = commands().size();
        move(humanBlack ? QPoint(8, 8) : QPoint(2, 2), humanBlack ? QPoint(2, 2) : QPoint(8, 8));
        QTRY_COMPARE_WITH_TIMEOUT(rows(), humanBlack ? 5 : 6, 5000);
        const auto sent = commands().mid(before);
        const QString position = humanBlack ? QStringLiteral("position startpos moves 7g7f 3c3d 8h2b+")
            : QStringLiteral("position startpos moves 7g7f 3c3d 2g2f 2b8h+");
        if (hit) {
            QVERIFY(sent.contains("ponderhit"));
            QVERIFY(!sent.contains("stop"));
        } else {
            QVERIFY(sent.contains("stop"));
            QVERIFY(sent.contains(position));
        }
        QVERIFY(!commands().join('\n').contains("ERROR"));
        finish();
        trigger("actionCopyUSIAll");
        QVERIFY(QApplication::clipboard()->text().contains(humanBlack ? "8h2b+ 3a2b" : "2b8h+ 7i8h"));
    }
    void immediateDuringHumanTurn_data()
    {
        humanPromotion_data();
    }
    void immediateDuringHumanTurn()
    {
        QFETCH(bool, humanBlack);
        QFETCH(bool, hit);
        m_humanBlack = humanBlack;
        start();
        if (humanBlack) move(QPoint(7, 7), QPoint(7, 6));
        const int initialRows = humanBlack ? 3 : 2;
        QTRY_COMPARE_WITH_TIMEOUT(rows(), initialRows, 5000);
        QTRY_VERIFY(commands().join('\n').contains("go ponder "));
        trigger("actionMakeImmediateMove");
        QTest::qWait(100);
        QVERIFY(!commands().contains("stop"));
        QCOMPARE(rows(), initialRows);
        const int file = humanBlack ? (hit ? 2 : 5) : (hit ? 3 : 4);
        move(QPoint(file, humanBlack ? 7 : 3), QPoint(file, humanBlack ? 6 : 4));
        QTRY_COMPARE_WITH_TIMEOUT(rows(), initialRows + 2, 5000);
        QVERIFY(commands().contains(hit ? "ponderhit" : "stop"));
        QVERIFY(!commands().join('\n').contains("ERROR"));
        for (int row = 0; row < rows(); ++row)
            QVERIFY(!record()->kifuView()->model()->index(row, 0).data().toString().contains("-1"));
        finish();
    }
    void immediateDuringEngineTurn_data()
    {
        QTest::addColumn<bool>("humanBlack");
        QTest::newRow("black-engine") << false;
        QTest::newRow("white-engine") << true;
    }
    void immediateDuringEngineTurn()
    {
        QFETCH(bool, humanBlack);
        m_humanBlack = humanBlack;
        qputenv("AUDIT_ENGINE_WAIT_FOR_STOP", "1");
        m_immediateTimer.start(20);
        start();
        if (humanBlack) move(QPoint(7, 7), QPoint(7, 6));
        QTRY_COMPARE_WITH_TIMEOUT(rows(), humanBlack ? 3 : 2, 5000);
        QVERIFY(commands().contains("stop"));
        QVERIFY(!commands().join('\n').contains("ERROR"));
        finish();
    }
    void engineVsEngine()
    {
        m_bothEngines = true;
        start();
        QTRY_VERIFY_WITH_TIMEOUT(!record()->isNavigationDisabled(), 7000);
        QVERIFY(rows() >= 6);
        QVERIFY(commands().count("ponderhit") >= 2);
        QVERIFY(!commands().join('\n').contains("ERROR"));
    }
    void csaPonder_data()
    {
        QTest::addColumn<bool>("hit");
        QTest::newRow("hit") << true;
        QTest::newRow("miss") << false;
    }
    void csaPonder()
    {
        QFETCH(bool, hit);
        ShogiGameController gc;
        QString sfen = m_initial;
        gc.newGame(sfen);
        CsaEngineController controller;
        CsaEngineController::InitParams init;
        init.enginePath = QStringLiteral(AUDIT_DIR "/mock_usi.py");
        init.engineName = "Audit USI";
        init.gameController = &gc;
        controller.initialize(init);
        QVERIFY(controller.isInitialized());
        CsaEngineController::ThinkingParams params;
        params.positionCmd = "position startpos";
        params.btimeStr = "60000";
        params.wtimeStr = "60000";
        auto result = controller.think(params);
        QVERIFY(result.valid);
        QCOMPARE(result.to, QPoint(7, 6));
        QTRY_VERIFY(commands().join('\n').contains("go ponder "));
        const auto before = commands().size();
        const QString opponent = hit ? QStringLiteral("3c3d") : QStringLiteral("4c4d");
        sfen = SfenPositionTracer::buildSfenRecord(m_initial, {"7g7f", opponent}, false).last();
        gc.newGame(sfen);
        params.positionCmd = "position startpos moves 7g7f " + opponent;
        result = controller.think(params);
        QVERIFY(result.valid);
        QVERIFY(!result.resign);
        QCOMPARE(result.to, QPoint(2, 6));
        const auto sent = commands().mid(before);
        if (hit) {
            QCOMPARE(sent.first(), QStringLiteral("ponderhit"));
            QVERIFY(!sent.contains("stop"));
        } else {
            QCOMPARE(sent.first(), QStringLiteral("stop"));
            QVERIFY(sent.contains(params.positionCmd));
        }
        // 再初期化では前局の予測を引き継がない。
        controller.cleanup();
        sfen = m_initial;
        gc.newGame(sfen);
        controller.initialize(init);
        params.positionCmd = "position startpos";
        const auto restart = commands().size();
        QVERIFY(controller.think(params).valid);
        QVERIFY(!commands().mid(restart).contains("ponderhit"));
        controller.sendQuit();
        QVERIFY(!commands().join('\n').contains("ERROR"));
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir settingsDir;
    if (!settingsDir.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", (settingsDir.path() + "/config").toUtf8());
    qputenv("XDG_DATA_HOME", (settingsDir.path() + "/data").toUtf8());
    qputenv("XDG_CACHE_HOME", (settingsDir.path() + "/cache").toUtf8());
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication app(argc, argv);
    app.setApplicationName("ShogiBoardQ-PonderTest");
    app.setQuitOnLastWindowClosed(false);
    TestPonderFlow test;
    return QTest::qExec(&test, argc, argv);
}

#include "tst_ponder_flow.moc"
