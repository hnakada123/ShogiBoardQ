/// @file engine_shutdown_harness.cpp
/// @brief 実MainWindowと実エンジンで、アプリのイベントループ停止・破棄まで検証する。
#include <QtTest>
#include <QAction>
#include <QApplication>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryDir>
#include <QTimer>
#include <cstdio>
#define private public
#include "mainwindow.h"
#undef private
#include "applicationfonts.h"
#include "startgamedialog.h"
#include "settingscommon.h"
#include "enginepondersettings.h"
#include "shogiview.h"
#include "usicommlogmodel.h"
#include "matchcoordinator.h"

static void auditMessage(QtMsgType type, const QMessageLogContext&, const QString& message)
{
    if (type == QtDebugMsg) return;
    const QByteArray bytes = message.toUtf8();
    std::fprintf(stderr, "%s\n", bytes.constData());
    std::fflush(stderr);
}

class ShutdownScenario : public QObject
{
    Q_OBJECT
public:
    explicit ShutdownScenario(MainWindow& window) : m_window(window)
    {
        connect(&m_poll, &QTimer::timeout, this, &ShutdownScenario::poll);
        connect(&m_dialogs, &QTimer::timeout, this, &ShutdownScenario::handleDialogs);
        for (auto* model : window.findChildren<UsiCommLogModel*>())
            connect(model, &UsiCommLogModel::usiCommLogChanged, this, &ShutdownScenario::logLine);
        m_elapsed.start();
        m_poll.start(20);
        m_dialogs.start(10);
    }

private:
    MainWindow& m_window;
    QTimer m_poll, m_dialogs;
    QElapsedTimer m_elapsed;
    const QString m_mode = qEnvironmentVariable("SHUTDOWN_MODE", "human-white");
    const QString m_scenario = qEnvironmentVariable("SHUTDOWN_SCENARIO", "thinking-close");
    int m_stage = 0;
    int m_searches = 0;
    int m_ponder = 0;
    int m_info = 0;
    int m_bestmoves = 0;
    qint64 m_searchTime = 0;
    bool m_forcedMove = false;
    bool m_cancelClose = false;
    qint64 m_gameEndTime = 0;

    void trigger(const char* name)
    {
        auto* action = m_window.findChild<QAction*>(QString::fromLatin1(name));
        if (!action || !action->isEnabled()) {
            qCritical() << "DISABLED_ACTION" << name;
            QCoreApplication::exit(2);
            return;
        }
        qInfo() << "ACTION" << name;
        action->trigger();
    }
    void humanMove()
    {
        auto* board = m_window.findChild<ShogiView*>();
        for (const auto target : {QPoint(7, 7), QPoint(7, 6)}) {
            bool clicked = false;
            for (int y = 0; y < board->height() && !clicked; y += 4) {
                for (int x = 0; x < board->width(); x += 4) {
                    if (board->clickedSquare(QPoint(x, y)) != target) continue;
                    QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier,
                        QPoint(x, y) + QPoint(board->fieldSize().width() / 3, board->fieldSize().height() / 3));
                    clicked = true;
                    break;
                }
            }
        }
    }
    void closeWindow()
    {
        qInfo() << "CLOSE_REQUEST" << m_scenario << "searches" << m_searches
                << "ponder" << m_ponder << "info" << m_info;
        if (m_scenario.endsWith("menu")) trigger("actionQuit");
        else m_window.close();
    }

private slots:
    void logLine()
    {
        const auto* model = qobject_cast<UsiCommLogModel*>(sender());
        if (!model) return;
        const QString line = model->usiCommLog();
        qInfo().noquote() << "USI" << line;
        if (line.contains("go btime ")) { ++m_searches; m_searchTime = m_elapsed.elapsed(); }
        if (line.contains("go ponder ")) ++m_ponder;
        if (line.contains("info depth ")) ++m_info;
        if (line.contains("bestmove ")) ++m_bestmoves;
    }
    void handleDialogs()
    {
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (!widget->isVisible()) continue;
            if (auto* dialog = qobject_cast<StartGameDialog*>(widget)) {
                dialog->findChild<QComboBox*>("comboBoxPlayer1")->setCurrentIndex(m_mode == "human-black" ? 0 : 1);
                dialog->findChild<QComboBox*>("comboBoxPlayer2")->setCurrentIndex(m_mode == "human-white" ? 0 : 1);
                dialog->findChild<QDialogButtonBox*>()->button(QDialogButtonBox::Ok)->click();
            } else if (auto* message = qobject_cast<QMessageBox*>(widget)) {
                qInfo() << "DIALOG" << message->text();
                if (m_cancelClose && message->button(QMessageBox::Cancel)) {
                    m_cancelClose = false;
                    message->button(QMessageBox::Cancel)->click();
                } else if (auto* discard = message->button(QMessageBox::Discard)) discard->click();
                else if (auto* yes = message->button(QMessageBox::Yes)) yes->click();
                else message->accept();
            }
        }
    }
    void poll()
    {
        if (m_elapsed.elapsed() > 30000) {
            qCritical() << "SCENARIO_TIMEOUT" << m_stage << m_searches << m_ponder << m_info;
            QCoreApplication::exit(3);
            return;
        }
        if (m_stage == 0) {
            m_stage = 1;
            trigger("actionStartGame");
            if (m_scenario.startsWith("startup-")) {
                m_stage = 4;
                closeWindow();
            } else if (m_mode == "human-black") humanMove();
            return;
        }
        if (m_stage == 2) {
            m_stage = 3;
            m_searches = 0;
            m_info = 0;
            trigger("actionStartGame");
            if (m_mode == "human-black") humanMove();
            return;
        }
        if (m_stage == 5) {
            if (m_elapsed.elapsed() - m_gameEndTime >= 1500) {
                if (m_scenario.contains("resume-")) {
                    auto* match = m_window.m_match;
                    if (!match || !match->interruptedGame()) {
                        qCritical() << "MISSING_INTERRUPTED_GAME";
                        QCoreApplication::exit(5);
                        return;
                    }
                    const auto saved = *match->interruptedGame();
                    m_stage = 3;
                    m_searches = m_info = 0;
                    trigger("actionResumeGame");
                    auto* clock = match->clock();
                    if (match->gameOverState().isOver || *match->sfenRecordPtr() != saved.sfens
                        || qAbs(clock->getPlayer1TimeIntMs() - saved.clock.player1TimeMs) > 100
                        || qAbs(clock->getPlayer2TimeIntMs() - saved.clock.player2TimeMs) > 100) {
                        qCritical() << "RESUME_STATE_MISMATCH";
                        QCoreApplication::exit(6);
                    }
                    qInfo() << "RESUME_COMPLETE";
                    // 先読み中断では人間手番に戻ることもある。再開後の終了を検証する。
                    if (m_scenario.contains("ponder-")) {
                        m_stage = 6;
                        m_gameEndTime = m_elapsed.elapsed();
                    }
                    return;
                }
                m_stage = 4;
                closeWindow();
            }
            return;
        }
        if (m_stage == 6) {
            if (m_elapsed.elapsed() - m_gameEndTime >= 600) {
                m_stage = 4;
                closeWindow();
            }
            return;
        }
        if (m_stage == 4 || m_searches == 0 || m_info == 0
            || m_elapsed.elapsed() - m_searchTime < 400) return;
        if (m_scenario.contains("ponder-")) {
            if (!m_forcedMove) {
                m_forcedMove = true;
                trigger("actionMakeImmediateMove");
                m_searchTime = m_elapsed.elapsed();
                return;
            }
            if (m_ponder == 0) return;
        }
        if (m_scenario == "cancel-close" && m_stage == 1) {
            if (!m_forcedMove) {
                m_forcedMove = true;
                trigger("actionMakeImmediateMove");
                return;
            }
            if (m_bestmoves == 0) return;
            m_stage = 3;
            m_cancelClose = true;
            closeWindow();
            if (!m_window.isVisible() || m_cancelClose) {
                qCritical() << "CLOSE_NOT_CANCELLED";
                QCoreApplication::exit(4);
            }
            return;
        }
        if (m_scenario.startsWith("break-") && m_stage == 1) {
            m_stage = m_scenario == "break-restart-close" ? 2 : 4;
            trigger("actionBreakOffGame");
            if (m_scenario.contains("wait-")) {
                m_gameEndTime = m_elapsed.elapsed();
                qInfo() << "GAME_END_COMPLETE";
                m_stage = 5;
                return;
            }
            if (m_stage == 4) closeWindow();
            return;
        }
        m_stage = 4;
        if (m_scenario.startsWith("legacy-")) {
            // b9ea8b68 はUIの有効条件だけを変更した。旧UIと同じ投了経路を通す。
            m_window.findChild<QAction*>("actionResign")->setEnabled(true);
            trigger("actionResign");
            if (m_scenario.contains("wait-")) {
                m_gameEndTime = m_elapsed.elapsed();
                qInfo() << "GAME_END_COMPLETE";
                m_stage = 5;
                return;
            }
        }
        if (m_scenario == "resign-close") trigger("actionResign");
        closeWindow();
    }
};

int main(int argc, char** argv)
{
    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    qputenv("XDG_DATA_HOME", (config.path() + "/data").toUtf8());
    qputenv("XDG_CACHE_HOME", (config.path() + "/cache").toUtf8());
    qInstallMessageHandler(auditMessage);
    QApplication app(argc, argv);
    qInstallMessageHandler(auditMessage);
    app.setApplicationName("ShogiBoardQ-ShutdownAudit");
    ApplicationFonts::initialize();
    auto& settings = SettingsCommon::openSettings();
    const QString name = QStringLiteral("Shutdown audit engine");
    settings.beginWriteArray("Engines", 1);
    settings.setArrayIndex(0);
    settings.setValue("name", name);
    settings.setValue("path", qEnvironmentVariable("SHUTDOWN_ENGINE"));
    settings.endArray();
    const QList<QPair<QString, QString>> options = {
        {"Threads", "1"}, {"USI_Hash", "64"}, {"Hash", "64"},
        {"OwnBook", "false"}, {"BookEnable", "false"}, {"BookFile", "no_book"},
        {"USI_OwnBook", "false"}, {"BookMaxPly", "0"}, {"MaxBookPly", "0"}
    };
    settings.beginWriteArray(name, static_cast<int>(options.size()));
    for (int i = 0; i < options.size(); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("name", options[i].first);
        settings.setValue("value", options[i].second);
        settings.setValue("type", "string");
    }
    settings.endArray();
    settings.setValue("GameSettings/basicTimeMinutes1", 10);
    settings.setValue("GameSettings/basicTimeMinutes2", 10);
    settings.setValue("GameSettings/consecutiveGames", 1);
    settings.setValue("GameSettings/startingPositionNumber", 1);
    settings.sync();
    EnginePonderSettings::save(name, qEnvironmentVariable("SHUTDOWN_SCENARIO").contains("ponder-"), true);
    int result;
    {
        MainWindow window;
        window.resize(1400, 1000);
        window.show();
        ShutdownScenario scenario(window);
        result = app.exec();
        qInfo() << "EVENT_LOOP_EXIT" << result;
    }
    qInfo() << "MAIN_WINDOW_DESTROYED";
    return result;
}

#include "engine_shutdown_harness.moc"
