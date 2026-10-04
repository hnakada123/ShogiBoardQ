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
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QTranslator>
#include <QLibraryInfo>
#include "startgamedialog.h"
#include "applicationfonts.h"
#include "gamesettings.h"
#include "settingscommon.h"
#include "kifupresentation.h"
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
#include "gameinfopanecontroller.h"
#include "gameinfokeys.h"
#include "gamerecordmodel.h"
#include <QTableView>
#include <QTableWidget>

class TestStartGameFlow : public QObject {
    Q_OBJECT
    std::unique_ptr<MainWindow> window;
    QTimer dialogTimer;
    QStringList messages;
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
    void editPosition(const QString& boardSfen, bool whiteToMove) {
        if (!window) {
            window = std::make_unique<MainWindow>();
            window->resize(1400, 1000);
            window->show();
            QTest::qWait(30);
        }
        child<QAction>(*window, "actionStartEditPosition")->trigger();
        child<QAction>(*window, "actionSetHiratePosition")->trigger();
        auto* view = window->findChild<ShogiView*>();
        QVERIFY(view->positionEditMode());
        view->board()->setSfen(boardSfen + QStringLiteral(" b - 1"));
        if (whiteToMove) child<QAction>(*window, "actionChangeTurn")->trigger();
        child<QAction>(*window, "actionEndEditPosition")->trigger();
        QVERIFY(!view->positionEditMode());
        QCOMPARE(window->m_state.currentSfenStr,
                 boardSfen + (whiteToMove ? QStringLiteral(" w - 1") : QStringLiteral(" b - 1")));
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
                messages.append(message->text());
                if (message->button(QMessageBox::Discard)) message->button(QMessageBox::Discard)->click();
                else if (message->button(QMessageBox::Yes)) message->button(QMessageBox::Yes)->click();
                else message->accept();
            }
        }
    }
private slots:
    void init() {
        requestedPreset = -1;
        messages.clear();
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
        KifuPresentation::configure("ja_JP", "auto", false);
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
    void dialogPresentation_data() {
        QTest::addColumn<bool>("english");
        QTest::newRow("japanese") << false;
        QTest::newRow("english") << true;
    }
    void dialogPresentation() {
        QFETCH(bool, english);
        QTranslator translator;
        QTranslator qtTranslator;
        if (english) {
            QVERIFY(translator.load(QStringLiteral(APP_BUILD "/ShogiBoardQ_en.qm")));
            QCoreApplication::installTranslator(&translator);
        } else if (qtTranslator.load(QStringLiteral("qt_ja"),
                                     QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            QCoreApplication::installTranslator(&qtTranslator);
        }
        auto& settings = SettingsCommon::openSettings();
        settings.beginWriteArray(QStringLiteral("Engines"));
        settings.setArrayIndex(0);
        settings.setValue(QStringLiteral("name"), QStringLiteral("Hayanagi 1.5.0"));
        settings.setValue(QStringLiteral("path"), QStringLiteral(REPO "/tests/gui/mock_usi.py"));
        settings.endArray();
        settings.sync();
        GameSettings::setStartGameDialogFontSize(12);
        StartGameDialog dialog;
        child<QComboBox>(dialog, "comboBoxPlayer2")->setCurrentIndex(1);
        child<QGroupBox>(dialog, "groupBoxSecondPlayerTimeSettings")->setChecked(true);
        child<QSpinBox>(dialog, "byoyomiSec1")->setValue(10);
        child<QSpinBox>(dialog, "byoyomiSec2")->setValue(4);
        dialog.resize(1000, 800);
        dialog.show();
        QVERIFY(QTest::qWaitForWindowExposed(&dialog));
        auto* scroll = child<QScrollArea>(dialog, "scrollArea");
        QTRY_COMPARE(scroll->horizontalScrollBar()->maximum(), 0);
        auto* start = child<QDialogButtonBox>(dialog, "buttonBox")->button(QDialogButtonBox::Ok);
        QVERIFY(start->isDefault());
        QTRY_COMPARE(scroll->verticalScrollBar()->maximum(), 0);
        const QString suffix = english ? QStringLiteral("-en") : QString();
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/start-game-dialog%1.png").arg(suffix)));
        auto* increase = child<QPushButton>(dialog, "pushButtonFontSizeUp");
        while (increase->isEnabled()) increase->click();
        dialog.resize(800, 480);
        QTest::qWait(50);
        const QRect startRect(start->mapTo(&dialog, QPoint()), start->size());
        QVERIFY(dialog.rect().contains(startRect));
        scroll->ensureWidgetVisible(increase);
        QVERIFY(increase->height() >= increase->sizeHint().height());
        QVERIFY(dialog.grab().save(QStringLiteral(AUDIT_DIR "/screenshots/start-game-dialog-large-font%1.png").arg(suffix)));
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
    void preparedGameInfoSurvivesStart_data() {
        QTest::addColumn<int>("preset");
        QTest::newRow("current-position") << 0;
        QTest::newRow("initial-position") << 1;
    }
    void preparedGameInfoSurvivesStart() {
        QFETCH(int, preset);
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/humanName1", QStringLiteral("対局者A"));
        settings.setValue("GameSettings/humanName2", QStringLiteral("対局者B"));
        settings.setValue("GameSettings/byoyomiSec1", 30);
        settings.sync();
        window = std::make_unique<MainWindow>();
        window->resize(1400, 1000);
        window->show();
        auto* controller = window->findChild<GameInfoPaneController*>();
        QVERIFY(controller);
        auto* table = controller->tableWidget();
        QCOMPARE(table->rowCount(), 9);
        QVERIFY(table->item(1, 1)->text().isEmpty());
        QVERIFY(table->item(2, 1)->text().isEmpty());
        table->item(6, 1)->setText(QStringLiteral("練習対局"));
        table->item(7, 1)->setText(QStringLiteral("自宅"));
        controller->applyChanges();
        window->activateWindow();
        table->setFocus();
        QTRY_VERIFY(table->hasFocus());
        table->setCurrentCell(8, 1);
        table->scrollToItem(table->currentItem());
        table->editItem(table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(QStringLiteral("更新ボタンを押す前の備考"));
        const QDateTime before = QDateTime::currentDateTime().addSecs(-1);
        requestedPreset = preset;
        startWindowGame();
        QCOMPARE(table->rowCount(), 9);
        const auto start = QDateTime::fromString(table->item(1, 1)->text(), QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        QVERIFY(start.isValid());
        QVERIFY(start >= before && start <= QDateTime::currentDateTime());
        QCOMPARE(table->item(2, 1)->text(), QStringLiteral("対局者A"));
        QCOMPARE(table->item(3, 1)->text(), QStringLiteral("対局者B"));
        QCOMPARE(table->item(5, 1)->text(), QStringLiteral("05:00+30"));
        QCOMPARE(table->item(6, 1)->text(), QStringLiteral("練習対局"));
        QCOMPARE(table->item(7, 1)->text(), QStringLiteral("自宅"));
        QCOMPARE(table->item(8, 1)->text(), QStringLiteral("更新ボタンを押す前の備考"));
        QVERIFY(!controller->isDirty());
        auto* record = window->findChild<GameRecordModel*>();
        QVERIFY(record);
        GameRecordModel::ExportContext context;
        context.gameInfoItems = controller->gameInfo();
        context.gameInfoProvided = true;
        const QString kif = record->toKifLines(context).join(QLatin1Char('\n'));
        QVERIFY(kif.contains(QStringLiteral("棋戦：練習対局")));
        QVERIFY(kif.contains(QStringLiteral("場所：自宅")));
        QVERIFY(kif.contains(QStringLiteral("備考：更新ボタンを押す前の備考")));
        QVERIFY(!kif.contains(QStringLiteral("未開始")));
        window->m_match->handleBreakOff();
    }
    void invalidEditedKings_data() {
        QTest::addColumn<QString>("boardSfen");
        QTest::addColumn<bool>("whiteToMove");
        QTest::addColumn<int>("opponents");
        const QStringList boards = {
            QStringLiteral("9/4+r4/9/9/9/9/9/9/9"), // 報告された龍1枚の局面
            QStringLiteral("9/9/9/9/9/9/9/9/9"),
            QStringLiteral("4k4/9/9/9/9/9/9/9/9"),
            QStringLiteral("9/9/9/9/9/9/9/9/4K4"),
            QStringLiteral("4K4/9/9/9/9/9/9/9/4K4"),
            QStringLiteral("4k4/9/9/9/9/9/9/9/4k4")
        };
        for (int i = 0; i < boards.size(); ++i)
            for (bool white : {false, true})
                for (int opponents = 0; opponents < 3; ++opponents)
                    QTest::newRow(qPrintable(QStringLiteral("position-%1-%2-mode-%3")
                        .arg(i).arg(white ? "white" : "black").arg(opponents)))
                        << boards.at(i) << white << opponents;
    }
    void invalidEditedKings() {
        QFETCH(QString, boardSfen);
        QFETCH(bool, whiteToMove);
        QFETCH(int, opponents);
        if (opponents != 0) {
            configureEngineGames(0);
            auto& settings = SettingsCommon::openSettings();
            settings.setValue("GameSettings/isHuman1", opponents == 1);
            settings.setValue("GameSettings/consecutiveGames", 1);
            settings.sync();
        }
        editPosition(boardSfen, whiteToMove);
        auto* view = window->findChild<ShogiView*>();
        auto* match = window->m_match;
        QVERIFY(match);
        auto* clock = match->clock();
        QVERIFY(clock);
        auto* record = window->findChild<RecordPane*>();
        const QString sfen = window->m_state.currentSfenStr;
        const QStringList history = *match->sfenRecordPtr();
        const auto box = view->board()->pieceBox();
        const auto hands = view->board()->pieceStand();
        const int rows = record->kifuView()->model()->rowCount();
        const qint64 blackTime = clock->remainingMainTimeMs(1);
        const qint64 whiteTime = clock->remainingMainTimeMs(2);
        const qint64 engineLogSize = QFile(qEnvironmentVariable("AUDIT_USI_LOG")).size();
        QSignalSpy ended(match, &MatchCoordinator::gameEnded);
        auto* wiring = window->m_matchWiring.get();
        QVERIFY(wiring);
        wiring->ensureMenuGameStartCoordinator();
        auto* gameStart = wiring->menuGameStartCoordinator();
        QVERIFY(gameStart);
        QSignalSpy started(gameStart, &GameStartCoordinator::started);
        QSignalSpy cleaned(gameStart, &GameStartCoordinator::requestPreStartCleanup);

        startWindowGame();
        QTest::qWait(50);

        QCOMPARE(messages.size(), 1);
        QVERIFY(messages.first().contains(QStringLiteral("対局を開始できません")));
        QVERIFY(messages.first().contains(QStringLiteral("先手 %1枚、後手 %2枚")
                     .arg(boardSfen.count('K')).arg(boardSfen.count('k'))));
        QCOMPARE(started.count(), 0);
        QCOMPARE(cleaned.count(), 0);
        QCOMPARE(ended.count(), 0);
        QVERIFY(!clock->isRunning());
        QCOMPARE(clock->remainingMainTimeMs(1), blackTime);
        QCOMPARE(clock->remainingMainTimeMs(2), whiteTime);
        QCOMPARE(QFile(qEnvironmentVariable("AUDIT_USI_LOG")).size(), engineLogSize);
        QCOMPARE(window->m_state.playMode, PlayMode::NotStarted);
        QVERIFY(!window->m_state.errorOccurred);
        QCOMPARE(view->board()->convertBoardToSfen(), boardSfen);
        QCOMPARE(view->board()->pieceBox(), box);
        QCOMPARE(view->board()->pieceStand(), hands);
        QCOMPARE(window->m_state.currentSfenStr, sfen);
        QCOMPARE(window->m_state.startSfenStr, sfen);
        QCOMPARE(*match->sfenRecordPtr(), history);
        QCOMPARE(record->kifuView()->model()->rowCount(), rows);
        QVERIFY(!record->isNavigationDisabled());
        QVERIFY(child<QAction>(*window, "actionStartGame")->isEnabled());
        QVERIFY(child<QAction>(*window, "actionStartEditPosition")->isEnabled());
        // 拒否後も編集を再開でき、収納していた王・玉も維持される。
        child<QAction>(*window, "actionStartEditPosition")->trigger();
        QVERIFY(view->positionEditMode());
        QVERIFY(!view->pieceBoxRect().isEmpty());
        QCOMPARE(view->board()->pieceBox(), box);
        child<QAction>(*window, "actionEndEditPosition")->trigger();
    }
    void validStartAfterInvalidEdit_data() {
        QTest::addColumn<int>("preset");
        QTest::addColumn<bool>("whiteToMove");
        QTest::newRow("corrected-black") << 0 << false;
        QTest::newRow("corrected-white") << 0 << true;
        QTest::newRow("hirate") << 1 << false;
        QTest::newRow("handicap") << 2 << true;
    }
    void validStartAfterInvalidEdit() {
        QFETCH(int, preset);
        QFETCH(bool, whiteToMove);
        editPosition(QStringLiteral("9/4+r4/9/9/9/9/9/9/9"), whiteToMove);
        startWindowGame();
        QCOMPARE(messages.size(), 1);
        QVERIFY(!window->m_match->clock()->isRunning());
        messages.clear();
        QString expectedBoard;
        if (preset == 0) {
            expectedBoard = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4");
            editPosition(expectedBoard, whiteToMove);
        } else {
            expectedBoard = GameStartOptionsBuilder::startingPositionSfen(preset).section(' ', 0, 0);
        }
        requestedPreset = preset;
        startWindowGame();
        QVERIFY2(!messages.join('\n').contains(QStringLiteral("対局を開始できません")),
                 qPrintable(messages.join('\n')));
        QVERIFY(window->m_match->clock()->isRunning());
        QCOMPARE(window->m_state.playMode, PlayMode::HumanVsHuman);
        auto* view = window->findChild<ShogiView*>();
        QCOMPARE(view->board()->convertBoardToSfen(), expectedBoard);
        if (preset == 0) {
            // 王・玉だけの正常な編集局面から、両手番とも実際に指せる。
            const int rank = whiteToMove ? 1 : 9;
            const int nextRank = whiteToMove ? 2 : 8;
            QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, square(5, rank));
            QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, square(5, nextRank));
            QTRY_COMPARE(window->findChild<RecordPane*>()->kifuView()->model()->rowCount(), 2);
            QCOMPARE(view->board()->pieceCharacter(5, nextRank),
                     whiteToMove ? Piece::WhiteKing : Piece::BlackKing);
        }
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
    void timeoutHasCorrectCause_data() {
        QTest::addColumn<QString>("language");
        QTest::addColumn<int>("plies");
        for (const auto* language : {"ja_JP", "en", "zh_CN", "zh_TW"})
            for (int plies : {0, 1, 2})
                QTest::newRow(qPrintable(QStringLiteral("%1-after-%2").arg(language).arg(plies)))
                    << QString::fromLatin1(language) << plies;
    }
    void timeoutHasCorrectCause() {
        QFETCH(QString, language);
        QFETCH(int, plies);
        QTranslator translator;
        QVERIFY(translator.load(QStringLiteral(APP_BUILD "/ShogiBoardQ_") + language + ".qm"));
        qApp->installTranslator(&translator);
        KifuPresentation::configure(language, "auto", false);
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
        auto* board = window->findChild<ShogiView*>();
        if (plies >= 1) {
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(7, 7));
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(7, 6));
        }
        if (plies == 2) {
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(3, 3));
            QTest::mouseClick(board, Qt::LeftButton, Qt::NoModifier, square(3, 4));
        }
        QCOMPARE(window->findChild<RecordPane*>()->kifuView()->model()->rowCount(), plies + 1);
        dialogTimer.start(10);
        QTest::qWait(1300);
        dialogTimer.stop();
        QVERIFY(match->gameOverState().isOver);
        QCOMPARE(match->gameOverState().lastInfo.cause,MatchCoordinator::Cause::Timeout);
        auto* model = window->findChild<RecordPane*>()->kifuView()->model();
        QCOMPARE(model->rowCount(), plies + 2);
        const QString terminal = (plies % 2 == 0 ? QStringLiteral("▲") : QStringLiteral("△"))
            + QStringLiteral("時間切れ");
        QVERIFY(model->index(plies + 1, 0).data().toString().endsWith(KifuPresentation::status(terminal)));
        if (language == "en") QVERIFY(model->index(plies + 1, 0).data().toString().endsWith("Time loss"));
        QVERIFY(window->grab().save(QStringLiteral(AUDIT_DIR "/screenshots/timeout-%1-%2.png")
            .arg(language).arg(plies)));
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
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) qputenv("QT_QPA_PLATFORM", "offscreen");
    QApplication app(argc,argv);
    ApplicationFonts::initialize();
    app.setApplicationName("TestStartGameFlow");
    TestStartGameFlow test;
    return QTest::qExec(&test,argc,argv);
}
#include "tst_start_game_flow.moc"
