/// @file tst_start_game_flow.cpp
/// @brief 実MainWindowと模擬USIエンジンによる対局ダイアログ・連続対局の回帰テスト
#include <QtTest>
#include <QApplication>
#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include "startgamedialog.h"
#include "settingscommon.h"
#include "gamestartcoordinator.h"
#include "gamestartoptionsbuilder.h"
#define private public
#include "mainwindow.h"
#undef private
#include "matchcoordinator.h"
#include "matchcoordinatorwiring.h"
#include "consecutivegamescontroller.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "shogiview.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "recordpane.h"
#include <QTableView>

class TestStartGameFlow : public QObject {
    Q_OBJECT
    std::unique_ptr<MainWindow> window;
    QTimer dialogTimer;
    template<class T> T* child(QObject& parent, const char* name) {
        auto* result = parent.findChild<T*>(QString::fromLatin1(name));
        Q_ASSERT(result);
        return result;
    }
    void accept(StartGameDialog& dialog) {
        child<QDialogButtonBox>(dialog, "buttonBox")->button(QDialogButtonBox::Ok)->click();
    }
    void startWindowGame() {
        if (!window) {
            window = std::make_unique<MainWindow>();
            window->resize(1400, 1000);
            window->show();
            QTest::qWait(30);
        }
        dialogTimer.start(10);
        child<QAction>(*window, "actionStartGame")->trigger();
        dialogTimer.stop();
    }
    QPoint square(int file, int rank) {
        auto* board = window->findChild<ShogiView*>();
        for (int y = 0; y < board->height(); y += 8)
            for (int x = 0; x < board->width(); x += 8)
                if (board->clickedSquare(QPoint(x, y)) == QPoint(file, rank))
                    return QPoint(x, y) + QPoint(board->fieldSize().width()/3, board->fieldSize().height()/3);
        return {};
    }
private slots:
    void initTestCase() {
        connect(&dialogTimer, &QTimer::timeout, this, &TestStartGameFlow::handleDialog);
    }
public slots:
    void handleDialog() {
        for (auto* widget : QApplication::topLevelWidgets()) {
            if (!widget->isVisible()) continue;
            if (auto* dialog = qobject_cast<StartGameDialog*>(widget)) {
                if (requestedPreset >= 0) child<QComboBox>(*dialog,"comboBoxStartingPosition")->setCurrentIndex(requestedPreset);
                accept(*dialog);
            }
            else if (auto* message = qobject_cast<QMessageBox*>(widget)) {
                qInfo() << "message:" << message->text();
                if (message->button(QMessageBox::Discard)) message->button(QMessageBox::Discard)->click();
                else if (message->button(QMessageBox::Yes)) message->button(QMessageBox::Yes)->click();
                else message->accept();
            }
        }
    }
private slots:
    void init() {
        requestedPreset = -1;
        auto& settings = SettingsCommon::openSettings();
        settings.clear();
        settings.setValue("GameSettings/isHuman1", true);
        settings.setValue("GameSettings/isHuman2", true);
        settings.setValue("GameSettings/startingPositionNumber", 1);
        settings.setValue("GameSettings/basicTimeMinutes1", 5);
        settings.sync();
    }
    void cleanup() {
        qunsetenv("AUDIT_ENGINE_DELAY");
        dialogTimer.stop();
        if (window) {
            if (window->m_consecutiveGamesController) window->m_consecutiveGamesController->reset();
            if (auto* match = window->m_match) match->handleBreakOff();
            window.reset();
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
    void screenshotValuesRoundtrip() {
        StartGameDialog d;
        child<QGroupBox>(d,"groupBoxSecondPlayerTimeSettings")->setChecked(true);
        child<QSpinBox>(d,"basicTimeMinutes1")->setValue(5);
        child<QSpinBox>(d,"byoyomiSec1")->setValue(10);
        child<QSpinBox>(d,"basicTimeMinutes2")->setValue(0);
        child<QSpinBox>(d,"byoyomiSec2")->setValue(3);
        accept(d);
        QCOMPARE(d.basicTimeMinutes1(),5); QCOMPARE(d.byoyomiSec1(),10);
        QCOMPARE(d.basicTimeMinutes2(),0); QCOMPARE(d.byoyomiSec2(),3);
        StartGameDialog reopened;
        QCOMPARE(child<QSpinBox>(reopened,"byoyomiSec2")->value(),3);
        QVERIFY(child<QGroupBox>(reopened,"groupBoxSecondPlayerTimeSettings")->isChecked());
    }
    void sameTimeCopiesFirstPlayer() {
        StartGameDialog d;
        child<QGroupBox>(d,"groupBoxSecondPlayerTimeSettings")->setChecked(false);
        child<QSpinBox>(d,"byoyomiSec1")->setValue(10);
        child<QSpinBox>(d,"byoyomiSec2")->setValue(3);
        accept(d);
        QCOMPARE(d.basicTimeMinutes2(),5); QCOMPARE(d.byoyomiSec2(),10);
    }
    void swapTimesAndNames() {
        StartGameDialog d;
        child<QGroupBox>(d,"groupBoxSecondPlayerTimeSettings")->setChecked(true);
        child<QLineEdit>(d,"lineEditHumanName1")->setText("A");
        child<QLineEdit>(d,"lineEditHumanName2")->setText("B");
        child<QSpinBox>(d,"byoyomiSec1")->setValue(10);
        child<QSpinBox>(d,"byoyomiSec2")->setValue(3);
        child<QPushButton>(d,"pushButtonSwapSides")->click();
        accept(d);
        QCOMPARE(d.humanName1(),"B"); QCOMPARE(d.humanName2(),"A");
        QCOMPARE(d.byoyomiSec1(),3); QCOMPARE(d.byoyomiSec2(),10);
    }
    void cancelDoesNotSaveGameSettings() {
        StartGameDialog d;
        child<QSpinBox>(d,"basicTimeMinutes1")->setValue(12);
        child<QDialogButtonBox>(d,"buttonBox")->button(QDialogButtonBox::Cancel)->click();
        QCOMPARE(SettingsCommon::openSettings().value("GameSettings/basicTimeMinutes1").toInt(),5);
    }
    void saveOnlyKeepsDialogOpen() {
        StartGameDialog d;
        d.show();
        child<QSpinBox>(d,"basicTimeMinutes1")->setValue(12);
        child<QPushButton>(d,"pushButtonSaveSettingsOnly")->click();
        QVERIFY(d.isVisible());
        QCOMPARE(SettingsCommon::openSettings().value("GameSettings/basicTimeMinutes1").toInt(),12);
    }
    void missingEngineFallsBackToHuman() {
        SettingsCommon::openSettings().clear();
        SettingsCommon::openSettings().sync();
        StartGameDialog d;
        auto* p2 = child<QComboBox>(d,"comboBoxPlayer2");
        qInfo() << "no engines: count=" << p2->count() << "index=" << p2->currentIndex();
        accept(d);
        qInfo() << "accepted: isHuman2=" << d.isHuman2() << "isEngine2=" << d.isEngine2() << "engineNumber2=" << d.engineNumber2();
        QCOMPARE(p2->currentIndex(),0);
        QVERIFY(d.isHuman2());
    }
    void resetClearsSeparateTimeFlag() {
        StartGameDialog d;
        child<QGroupBox>(d,"groupBoxSecondPlayerTimeSettings")->setChecked(true);
        child<QPushButton>(d,"pushButtonResetToDefault")->click();
        QVERIFY(!child<QGroupBox>(d,"groupBoxSecondPlayerTimeSettings")->isChecked());
    }
    void oneMinuteByoyomi() {
        StartGameDialog d;
        child<QSpinBox>(d,"byoyomiSec1")->setValue(60);
        QCOMPARE(child<QSpinBox>(d,"byoyomiSec1")->value(),60);
    }
    void uncheckedTimeoutIsRespected() {
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/basicTimeMinutes1",0);
        settings.setValue("GameSettings/byoyomiSec1",1);
        settings.setValue("GameSettings/isLoseOnTimeout",false);
        settings.sync();
        startWindowGame();
        auto* match = window->m_match;
        QVERIFY(match); QVERIFY(match->clock());
        qInfo() << "unchecked timeout: enforcesTimeout=" << match->clock()->enforcesTimeout();
        dialogTimer.start(10);
        QTest::qWait(1300);
        dialogTimer.stop();
        qInfo() << "unchecked timeout: gameOver=" << match->gameOverState().isOver;
        QVERIFY(!match->clock()->enforcesTimeout());
        QVERIFY(!match->gameOverState().isOver);
    }
    void newPresetAfterHandicapGame() {
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/startingPositionNumber",2);
        settings.sync();
        startWindowGame();
        auto* match = window->m_match;
        QVERIFY(match);
        auto* board = window->findChild<ShogiView*>()->board();
        const QString handicap = GameStartOptionsBuilder::startingPositionSfen(2).section(' ',0,0);
        QCOMPARE(board->convertBoardToSfen(),handicap);
        match->handleBreakOff();
        settings.setValue("GameSettings/startingPositionNumber",1);
        settings.sync();
        // The modal handler explicitly chooses Hirate after forceCurrentPositionSelection().
        requestedPreset = 1;
        startWindowGame();
        requestedPreset = -1;
        const QString hirate = GameStartOptionsBuilder::startingPositionSfen(1).section(' ',0,0);
        QCOMPARE(window->findChild<ShogiView*>()->board()->convertBoardToSfen(),hirate);
    }
    void humanGameHonorsMaxMoves() {
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/maxMoves",1);
        settings.sync();
        startWindowGame();
        auto* match = window->m_match;
        QVERIFY(match); QCOMPARE(match->maxMoves(),1);
        auto* board = window->findChild<ShogiView*>();
        auto* record = window->findChild<RecordPane*>();
        QVERIFY(record);
        QSignalSpy ended(match, &MatchCoordinator::gameEnded);
        dialogTimer.start(10);
        QTest::mouseClick(board,Qt::LeftButton,Qt::NoModifier,square(7,7));
        QTest::mouseClick(board,Qt::LeftButton,Qt::NoModifier,square(7,6));
        dialogTimer.stop();
        QVERIFY(record->kifuView()->model()->rowCount() >= 2);
        qInfo() << "maxMoves=1, after first move: gameOver=" << match->gameOverState().isOver;
        QVERIFY(match->gameOverState().isOver);
        QCOMPARE(ended.count(),1);
    }
    void timeoutHasCorrectCause() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/basicTimeMinutes1",0);
        settings.setValue("GameSettings/byoyomiSec1",1);
        settings.setValue("GameSettings/isLoseOnTimeout",true);
        settings.setValue("GameSettings/isAutoSaveKifu",true);
        settings.setValue("GameSettings/kifuSaveDir",dir.path());
        settings.sync();
        startWindowGame();
        auto* match = window->m_match;
        QVERIFY(match);
        dialogTimer.start(10);
        QTest::qWait(1300);
        dialogTimer.stop();
        QVERIFY(match->gameOverState().isOver);
        QCOMPARE(match->gameOverState().lastInfo.cause,MatchCoordinator::Cause::Timeout);
        auto* model = window->findChild<RecordPane*>()->kifuView()->model();
        QCOMPARE(model->rowCount(), 2);
        QVERIFY(model->index(1, 0).data().toString().contains(QStringLiteral("時間切れ")));
        const QStringList files = QDir(dir.path()).entryList({"*.kifu"}, QDir::Files);
        QCOMPARE(files.size(), 1);
        QFile saved(QDir(dir.path()).filePath(files.first()));
        QVERIFY(saved.open(QIODevice::ReadOnly));
        QVERIFY(saved.readAll().contains(QStringLiteral("時間切れ").toUtf8()));
    }
    void consecutiveGamesContinueAfterMaxMoves() {
        configureEngineGames(1);
        startWindowGame();
        dialogTimer.start(10);
        QTRY_COMPARE_WITH_TIMEOUT(window->m_consecutiveGamesController->currentGameNumber(), 2, 5000);
        dialogTimer.stop();
        QVERIFY(window->m_consecutiveGamesController);
        qInfo() << "consecutive maxMoves: game=" << window->m_consecutiveGamesController->currentGameNumber();
        QCOMPARE(window->m_consecutiveGamesController->currentGameNumber(),2);
    }
    void consecutiveAutoSaveKeepsMoves() {
        // 1局1秒以上にして、秒単位の保存ファイル名が衝突しないようにする。
        qputenv("AUDIT_ENGINE_DELAY", "0.22");
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        configureEngineGames(0);
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/isAutoSaveKifu",true);
        settings.setValue("GameSettings/kifuSaveDir",dir.path());
        settings.sync();
        startWindowGame();
        dialogTimer.start(10);
        QTRY_VERIFY_WITH_TIMEOUT(window->m_match->gameOverState().isOver
                                 && window->m_consecutiveGamesController->currentGameNumber() == 2, 10000);
        dialogTimer.stop();
        const QStringList files = QDir(dir.path()).entryList({"*.kifu"},QDir::Files);
        qInfo() << "autosave dir=" << dir.path() << "files=" << files;
        QCOMPARE(files.size(),2);
        for (const auto& filename : files) {
            QFile f(QDir(dir.path()).filePath(filename));
            QVERIFY(f.open(QIODevice::ReadOnly));
            const QByteArray contents=f.readAll();
            QVERIFY(contents.contains("(77)"));
            QVERIFY(!contents.contains(QStringLiteral("変化：").toUtf8()));
        }
    }
private:
    void configureEngineGames(int maxMoves) {
        auto& settings = SettingsCommon::openSettings();
        settings.beginWriteArray("Engines"); settings.setArrayIndex(0);
        settings.setValue("name","Audit USI");
        settings.setValue("path",QStringLiteral(AUDIT_DIR "/mock_usi.py"));
        settings.endArray();
        settings.setValue("GameSettings/isHuman1",false);
        settings.setValue("GameSettings/isHuman2",false);
        settings.setValue("GameSettings/engineNumber1",0);
        settings.setValue("GameSettings/engineNumber2",0);
        settings.setValue("GameSettings/consecutiveGames",2);
        settings.setValue("GameSettings/maxMoves",maxMoves);
        settings.sync();
    }

    int requestedPreset = -1;
};
int main(int argc,char** argv) {
    QTemporaryDir config;
    if (!config.isValid()) return 1;
    qputenv("XDG_CONFIG_HOME", config.path().toUtf8());
    qputenv("AUDIT_USI_LOG", (config.path() + QStringLiteral("/usi.log")).toUtf8());
    qputenv("QT_QPA_PLATFORM","offscreen");
    QApplication app(argc,argv);
    app.setApplicationName("TestStartGameFlow");
    TestStartGameFlow test;
    return QTest::qExec(&test,argc,argv);
}
#include "tst_start_game_flow.moc"
