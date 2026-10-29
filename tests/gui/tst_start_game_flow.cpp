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
#include "recordentry.h"
#include "gamestartoptionsbuilder.h"
#include "mainwindow.h"
#include "mainwindowcomposer.h"
#include "matchcoordinator.h"
#include "engineslots.h"
#include "shogiviewhighlighting.h"
#include "usi.h"
#include "kifurecordlistmodel.h"
#include "kifunavigationstate.h"
#include "kifubranchtree.h"
#include "kifubranchnode.h"
#include "matchtimerecord.h"
#include "consecutivegameseries.h"
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include "boardchromewidget.h"
#include "shogiview.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "recordpane.h"
#include "gameinfopane.h"
#include "gameinfokeys.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include <QTableView>
#include <QTableWidget>
#include "../game_info_table_helper.h"
#include <utility>

class TestStartGameFlow : public QObject {
    Q_OBJECT
    QList<QRect> paintedBoardRects;
    QList<QSize> paintedWindowSizes;
    QStringList paintedBoards;
    bool eventFilter(QObject* object, QEvent* event) override {
        if (event->type() == QEvent::Paint) {
            if (auto* view = qobject_cast<ShogiView*>(object)) {
                paintedBoardRects.append(QRect(view->mapToGlobal(QPoint()), view->size()));
                paintedWindowSizes.append(view->window()->size());
                paintedBoards.append(view->board()->position().boardSfen());
            }
        }
        return QObject::eventFilter(object, event);
    }
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
        QCOMPARE(window->composer().parts().navState->currentSfen(),
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
        qunsetenv("AUDIT_ENGINE_WAIT_FOR_STOP");
        dialogTimer.stop();
        if (window) {
            if (window->composer().parts().consecutiveGames) window->composer().parts().consecutiveGames->reset();
            if (auto* match = window->composer().parts().match) match->handleBreakOff();
            window.reset();
        }
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
        NotationStyle::configure("ja_JP", "auto", false);
    }
    // 対局中に盤を反転しても、時計の残り時間と手番の側の表示はそのまま
    void flipBoardKeepsClockDisplay() {
        startWindowGame();
        auto* view = window->findChild<ShogiView*>();
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, square(7, 7));
        QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, square(7, 6));
        QTRY_COMPARE(window->composer().parts().match->record().sfens().size(), 2);
        QTRY_VERIFY(!(view->chrome()->activeSide() == Turn::Black));
        const QString blackText = view->chrome()->card(Turn::Black)->clockLabel()->text();
        const QString whiteText = view->chrome()->card(Turn::White)->clockLabel()->text();
        QVERIFY(blackText.startsWith(QStringLiteral("00:0")) && blackText != QStringLiteral("00:00:00"));
        QVERIFY(whiteText != QStringLiteral("00:00:00"));

        child<QAction>(*window, "actionFlipBoard")->trigger();

        QVERIFY(view->flipMode());
        QCOMPARE(view->chrome()->card(Turn::Black)->clockLabel()->text(), blackText);
        QCOMPARE(view->chrome()->card(Turn::White)->clockLabel()->text(), whiteText);
        QVERIFY(!(view->chrome()->activeSide() == Turn::Black));
    }
    void resumeInterruptedGame_data() {
        QTest::addColumn<int>("mode");
        QTest::addColumn<int>("timing");
        QTest::addColumn<int>("pausePly");
        for (int mode = 0; mode < 4; ++mode)
            for (int timing = 0; timing < 3; ++timing)
                for (int ply : {0, 1, 2})
                    QTest::newRow(qPrintable(QString("mode%1-time%2-ply%3").arg(mode).arg(timing).arg(ply)))
                        << mode << timing << ply;
    }
    void resumeKeepsBoardStable_data() {
        QTest::addColumn<int>("mode");
        for (int mode = 0; mode < 4; ++mode)
            QTest::newRow(qPrintable(QString("mode%1").arg(mode))) << mode;
    }
    void resumeKeepsBoardStable() {
        QFETCH(int, mode);
        if (mode) configureEngineGames(0);
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/isHuman1", mode == 0 || mode == 1);
        settings.setValue("GameSettings/isHuman2", mode == 0 || mode == 2);
        settings.setValue("GameSettings/consecutiveGames", 1);
        settings.sync();
        qputenv("AUDIT_ENGINE_WAIT_FOR_STOP", "1");
        startWindowGame();
        auto* view = window->findChild<ShogiView*>();
        const QList<QPair<QPoint, QPoint>> moves = {{{7,7},{7,6}}, {{3,3},{3,4}}};
        for (int ply = 0; ply < 2; ++ply) {
            if (mode == 0 || (ply == 0 ? mode == 1 : mode == 2)) {
                QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, square(moves[ply].first.x(), moves[ply].first.y()));
                QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, square(moves[ply].second.x(), moves[ply].second.y()));
            } else {
                QTest::qWait(150);
                child<QAction>(*window, "actionMakeImmediateMove")->trigger();
            }
            QTRY_COMPARE(window->composer().parts().match->record().sfens().size(), ply + 2);
        }
        QTest::qWait(250);
        child<QAction>(*window, "actionBreakOffGame")->trigger();
        QTest::qWait(250);
        const QRect expectedRect(view->mapToGlobal(QPoint()), view->size());
        const QSize expectedWindowSize = window->size();
        const QString expectedBoard = view->board()->position().boardSfen();
        const auto* record = window->composer().parts().kifuRecord;
        QStringList expectedRecordText;
        // 中断行を除く全列の表示（開始局面の時間見出し、手数など）を保持する。
        for (int row = 0; row < record->rowCount() - 1; ++row)
            for (int column = 0; column < record->columnCount(); ++column)
                expectedRecordText.append(record->index(row, column).data().toString());
        paintedBoardRects.clear();
        paintedWindowSizes.clear();
        paintedBoards.clear();
        view->installEventFilter(this);
        child<QAction>(*window, "actionResumeGame")->trigger();
        QTest::qWait(500);
        view->removeEventFilter(this);
        QVERIFY(!paintedBoardRects.isEmpty());
        // 遅延レイアウトを含む各描画時点で、盤とウィンドウが動かないことを確認する。
        for (const auto& rect : std::as_const(paintedBoardRects)) QCOMPARE(rect, expectedRect);
        for (const auto& size : std::as_const(paintedWindowSizes)) QCOMPARE(size, expectedWindowSize);
        for (const auto& board : std::as_const(paintedBoards)) QCOMPARE(board, expectedBoard);
        QStringList resumedRecordText;
        for (int row = 0; row < record->rowCount(); ++row)
            for (int column = 0; column < record->columnCount(); ++column)
                resumedRecordText.append(record->index(row, column).data().toString());
        QCOMPARE(resumedRecordText, expectedRecordText);
    }
    void resumeInterruptedGame() {
        QFETCH(int, mode);
        QFETCH(int, timing);
        QFETCH(int, pausePly);
        if (mode) configureEngineGames(0);
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/isHuman1", mode == 0 || mode == 1);
        settings.setValue("GameSettings/isHuman2", mode == 0 || mode == 2);
        settings.setValue("GameSettings/consecutiveGames", 1);
        settings.setValue("GameSettings/basicTimeMinutes1", timing == 1 ? 0 : 5);
        settings.setValue("GameSettings/byoyomiSec1", timing == 1 ? 10 : 0);
        settings.setValue("GameSettings/addEachMoveSec1", timing == 2 ? 3 : 0);
        settings.sync();
        qputenv("AUDIT_ENGINE_WAIT_FOR_STOP", "1");
        startWindowGame();
        auto* match = window->composer().parts().match;
        auto* clock = window->composer().parts().clock;
        auto* view = window->findChild<ShogiView*>();
        auto* resume = child<QAction>(*window, "actionResumeGame");
        QVERIFY(!resume->isEnabled());
        const auto playNext = [&]() {
            const int ply = static_cast<int>(match->record().sfens().size() - 1);
            const bool human = mode == 0 || (ply % 2 == 0 ? mode == 1 : mode == 2);
            if (human) {
                const QList<QPair<QPoint, QPoint>> moves = {
                    {{7,7},{7,6}}, {{3,3},{3,4}}, {{2,7},{2,6}}, {{8,3},{8,4}}};
                QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, square(moves[ply].first.x(), moves[ply].first.y()));
                QTest::mouseClick(view, Qt::LeftButton, Qt::NoModifier, square(moves[ply].second.x(), moves[ply].second.y()));
            } else {
                QTest::qWait(120);
                child<QAction>(*window, "actionMakeImmediateMove")->trigger();
            }
        };
        for (int ply = 0; ply < pausePly; ++ply) {
            playNext();
            QTRY_COMPARE(match->record().sfens().size(), ply + 2);
        }
        QTest::qWait(160);
        const auto history = match->record().sfens();
        const auto board = view->board()->position().boardSfen();
        child<QAction>(*window, "actionBreakOffGame")->trigger();
        QVERIFY(match->isGameOver());
        QVERIFY(resume->isEnabled());
        QVERIFY(match->interruptedGame());
        const auto saved = match->interruptedGame()->clock;
        QTest::qWait(250);
        QVERIFY(!clock->isRunning());
        auto* engines = window->composer().parts().engines;
        for (int slot : {1, 2}) {
            if (Usi* engine = engines->engineInSlot(slot)) QVERIFY(!engine->isRunning());
        }
        // 中断中の棋譜閲覧が再開地点に影響しない。
        auto* table = window->findChild<RecordPane*>()->kifuView();
        table->selectRow(0);
        QTest::qWait(25);
        resume->trigger();
        QVERIFY(!match->isGameOver());
        QVERIFY(!resume->isEnabled());
        QCOMPARE(match->record().sfens(), history);
        QCOMPARE(view->board()->position().boardSfen(), board);
        QCOMPARE(clock->currentPlayer(), saved.currentPlayer);
        QVERIFY(qAbs(clock->remainingMs(Turn::Black) - saved.of(Turn::Black).remainingMs) < 100);
        QVERIFY(qAbs(clock->remainingMs(Turn::White) - saved.of(Turn::White).remainingMs) < 100);
        QCOMPARE(window->composer().parts().kifuRecord->rowCount(), pausePly + 1);
        QVERIFY(!window->composer().parts().navState->currentNode()->isTerminal());
        QCOMPARE(window->composer().parts().navState->currentPly(), pausePly);
        // 棋譜欄の強調と行は表示中の手と揃っている
        QCOMPARE(window->composer().parts().kifuRecord->currentHighlightRow(), pausePly);
        QCOMPARE(window->composer().parts().kifuRecord->nodeAt(pausePly), window->composer().parts().navState->currentNode());
        QTest::qWait(150);
        QVERIFY(clock->isRunning());
        playNext();
        QTRY_COMPARE(match->record().sfens().size(), pausePly + 2);
        QCOMPARE(match->record().sfens().mid(0, history.size()), history);
        QVERIFY(!match->isGameOver());
        child<QAction>(*window, "actionBreakOffGame")->trigger();
        QVERIFY(resume->isEnabled());
        const qint64 left = clock->remainingTurnTimeMs(clock->currentPlayer());
        QTest::qWait(100);
        resume->trigger();
        QVERIFY(!match->isGameOver());
        QVERIFY(qAbs(clock->remainingTurnTimeMs(clock->currentPlayer()) - left) < 100);
    }

    void resumeInvalidatedByNewRecord_data() {
        QTest::addColumn<bool>("edit");
        QTest::newRow("new-record") << false;
        QTest::newRow("position-edit") << true;
    }
    void resumeInvalidatedByNewRecord() {
        QFETCH(bool, edit);
        startWindowGame();
        child<QAction>(*window, "actionBreakOffGame")->trigger();
        QVERIFY(child<QAction>(*window, "actionResumeGame")->isEnabled());
        dialogTimer.start(10);
        child<QAction>(*window, edit ? "actionStartEditPosition" : "actionNewGame")->trigger();
        dialogTimer.stop();
        QVERIFY(!child<QAction>(*window, "actionResumeGame")->isEnabled());
        QVERIFY(!window->composer().parts().match->interruptedGame());
    }

    void screenshotValuesRoundtrip() {
        StartGameDialog d;
        child<QGroupBox>(d,"groupBoxSecondPlayerTimeSettings")->setChecked(true);
        child<QSpinBox>(d,"basicTimeMinutes1")->setValue(5);
        child<QSpinBox>(d,"byoyomiSec1")->setValue(10);
        child<QSpinBox>(d,"basicTimeMinutes2")->setValue(0);
        child<QSpinBox>(d,"byoyomiSec2")->setValue(3);
        accept(d);
        QCOMPARE(d.data().players[0].time.mainMs,5 * 60000); QCOMPARE(d.data().players[0].time.byoyomiMs,10000);
        QCOMPARE(d.data().players[1].time.mainMs,0); QCOMPARE(d.data().players[1].time.byoyomiMs,3000);
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
        GameSettings::startGameDialogFontSize.set(12);
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
        QCOMPARE(d.data().players[1].time.mainMs,5 * 60000); QCOMPARE(d.data().players[1].time.byoyomiMs,10000);
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
        QCOMPARE(d.data().players[0].humanName,"B"); QCOMPARE(d.data().players[1].humanName,"A");
        QCOMPARE(d.data().players[0].time.byoyomiMs,3000); QCOMPARE(d.data().players[1].time.byoyomiMs,10000);
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
        qInfo() << "accepted: player2 human=" << d.data().players[1].isHuman() << "engine=" << d.data().players[1].engine.name;
        QCOMPARE(p2->currentIndex(),0);
        QVERIFY(d.data().players[1].isHuman());
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
        auto* match = window->composer().parts().match;
        QVERIFY(match); QVERIFY(window->composer().parts().clock);
        qInfo() << "unchecked timeout: enforcesTimeout=" << window->composer().parts().clock->enforcesTimeout();
        dialogTimer.start(10);
        QTest::qWait(1300);
        dialogTimer.stop();
        qInfo() << "unchecked timeout: gameOver=" << match->isGameOver();
        QVERIFY(!window->composer().parts().clock->enforcesTimeout());
        QVERIFY(!match->isGameOver());
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
        auto* controller = window->findChild<GameInfoPane*>();
        QVERIFY(controller);
        GameInfoTable table{controller->tableView()};
        QCOMPARE(table.rowCount(), 9);
        QVERIFY(table.text(1, 1).isEmpty());
        // 対局者の名前は対局を始めるまで空（KifuDocument::defaultHeader。欄には入力の案内だけを出す）
        QVERIFY(table.text(2, 1).isEmpty());
        QVERIFY(table.text(3, 1).isEmpty());
        table.setText(6, 1, QStringLiteral("練習対局"));
        table.setText(7, 1, QStringLiteral("自宅"));
        window->activateWindow();
        table.view->setFocus();
        QTRY_VERIFY(table.view->hasFocus());
        table.setCurrentCell(8, 1);
        table.view->scrollTo(table.view->currentIndex());
        table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(QStringLiteral("確定する前の備考"));
        const QDateTime before = QDateTime::currentDateTime().addSecs(-1);
        requestedPreset = preset;
        startWindowGame();
        QCOMPARE(table.rowCount(), 9);
        const auto start = QDateTime::fromString(table.text(1, 1), QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        QVERIFY(start.isValid());
        QVERIFY(start >= before && start <= QDateTime::currentDateTime());
        QCOMPARE(table.text(2, 1), QStringLiteral("対局者A"));
        QCOMPARE(table.text(3, 1), QStringLiteral("対局者B"));
        QCOMPARE(table.text(5, 1), QStringLiteral("05:00+30"));
        QCOMPARE(table.text(6, 1), QStringLiteral("練習対局"));
        QCOMPARE(table.text(7, 1), QStringLiteral("自宅"));
        QCOMPARE(table.text(8, 1), QStringLiteral("確定する前の備考"));
        QVERIFY(!controller->hasPendingEditor());
        // 書き出しは文書の対局情報（入力中だったセルも確定して入っている）
        const QString kif = KifuExport::lines(*window->composer().parts().document, KifuFormat::Kif)
                                .join(QLatin1Char('\n'));
        QVERIFY(kif.contains(QStringLiteral("棋戦：練習対局")));
        QVERIFY(kif.contains(QStringLiteral("場所：自宅")));
        QVERIFY(kif.contains(QStringLiteral("備考：確定する前の備考")));
        QVERIFY(!kif.contains(QStringLiteral("未開始")));
        window->composer().parts().match->handleBreakOff();
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
        auto* match = window->composer().parts().match;
        QVERIFY(match);
        auto* clock = window->composer().parts().clock;
        QVERIFY(clock);
        auto* record = window->findChild<RecordPane*>();
        const QString sfen = window->composer().parts().navState->currentSfen();
        const QStringList history = match->record().sfens();
        const auto box = view->board()->pieceBox();
        const auto hands = view->board()->position().handsSfen();
        const int rows = record->kifuView()->model()->rowCount();
        const qint64 blackTime = clock->remainingMainTimeMs(Turn::Black);
        const qint64 whiteTime = clock->remainingMainTimeMs(Turn::White);
        const qint64 engineLogSize = QFile(qEnvironmentVariable("AUDIT_USI_LOG")).size();
        QSignalSpy ended(match, &MatchCoordinator::gameEnded);
        // 始められない局面では棋譜を作り直さない
        QSignalSpy cleaned(window->composer().parts().branchTree, &KifuBranchTree::reset);

        startWindowGame();
        QTest::qWait(50);

        QCOMPARE(messages.size(), 1);
        QVERIFY(messages.first().contains(QStringLiteral("対局を開始できません")));
        QVERIFY(messages.first().contains(QStringLiteral("先手 %1枚、後手 %2枚")
                     .arg(boardSfen.count('K')).arg(boardSfen.count('k'))));
        QCOMPARE(cleaned.count(), 0);
        QCOMPARE(ended.count(), 0);
        QVERIFY(!clock->isRunning());
        QCOMPARE(clock->remainingMainTimeMs(Turn::Black), blackTime);
        QCOMPARE(clock->remainingMainTimeMs(Turn::White), whiteTime);
        QCOMPARE(QFile(qEnvironmentVariable("AUDIT_USI_LOG")).size(), engineLogSize);
        QCOMPARE(window->composer().activity().activity(), AppActivityState::Activity::Idle);
        QCOMPARE(view->board()->position().boardSfen(), boardSfen);
        QCOMPARE(view->board()->pieceBox(), box);
        QCOMPARE(view->board()->position().handsSfen(), hands);
        QCOMPARE(window->composer().parts().navState->currentSfen(), sfen);
        QCOMPARE(window->composer().parts().branchTree->root()->sfen(), sfen);
        QCOMPARE(match->record().sfens(), history);
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
        QVERIFY(!window->composer().parts().clock->isRunning());
        messages.clear();
        QString expectedBoard;
        if (preset == 0) {
            expectedBoard = QStringLiteral("4k4/9/9/9/9/9/9/9/4K4");
            editPosition(expectedBoard, whiteToMove);
        } else {
            expectedBoard = QString::fromLatin1(Handicaps::kAll[preset - 1].sfen).section(' ', 0, 0);
        }
        requestedPreset = preset;
        startWindowGame();
        QVERIFY2(!messages.join('\n').contains(QStringLiteral("対局を開始できません")),
                 qPrintable(messages.join('\n')));
        QVERIFY(window->composer().parts().clock->isRunning());
        QCOMPARE(window->composer().activity().activity(), AppActivityState::Activity::DuringGame);
        QVERIFY(window->composer().parts().document->players().humanVsHuman());
        auto* view = window->findChild<ShogiView*>();
        QCOMPARE(view->board()->position().boardSfen(), expectedBoard);
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
        auto* match = window->composer().parts().match;
        QVERIFY(match);
        auto* board = window->findChild<ShogiView*>()->board();
        const QString handicap = QString::fromLatin1(Handicaps::kAll[2 - 1].sfen).section(' ',0,0);
        QCOMPARE(board->position().boardSfen(),handicap);
        match->handleBreakOff();
        settings.setValue("GameSettings/startingPositionNumber",1);
        settings.sync();
        // The modal handler explicitly chooses Hirate after forceCurrentPositionSelection().
        requestedPreset = 1;
        startWindowGame();
        requestedPreset = -1;
        const QString hirate = QString::fromLatin1(Handicaps::kAll[1 - 1].sfen).section(' ',0,0);
        QCOMPARE(window->findChild<ShogiView*>()->board()->position().boardSfen(),hirate);
    }
    void humanGameHonorsMaxMoves() {
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/maxMoves",1);
        settings.sync();
        startWindowGame();
        auto* match = window->composer().parts().match;
        QVERIFY(match); QCOMPARE(match->options().maxMoves,1);
        auto* board = window->findChild<ShogiView*>();
        auto* record = window->findChild<RecordPane*>();
        QVERIFY(record);
        QSignalSpy ended(match, &MatchCoordinator::gameEnded);
        dialogTimer.start(10);
        QTest::mouseClick(board,Qt::LeftButton,Qt::NoModifier,square(7,7));
        QTest::mouseClick(board,Qt::LeftButton,Qt::NoModifier,square(7,6));
        dialogTimer.stop();
        QVERIFY(record->kifuView()->model()->rowCount() >= 2);
        qInfo() << "maxMoves=1, after first move: gameOver=" << match->isGameOver();
        QVERIFY(match->isGameOver());
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
        NotationStyle::configure(language, "auto", false);
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
        auto* match = window->composer().parts().match;
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
        QVERIFY(match->isGameOver());
        QCOMPARE(match->gameEnd()->cause,MatchCoordinator::Cause::Timeout);
        auto* model = window->findChild<RecordPane*>()->kifuView()->model();
        QCOMPARE(model->rowCount(), plies + 2);
        const RecordEntry terminal = RecordEntry::ending(TerminalType::Timeout, plies % 2 == 0 ? Turn::Black : Turn::White);
        QVERIFY(model->index(plies + 1, 0).data().toString().endsWith(KifuPresentation::entryLabel(terminal, QString())));
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
        QTRY_COMPARE_WITH_TIMEOUT(window->composer().parts().consecutiveGames->currentGameNumber(), 2, 5000);
        dialogTimer.stop();
        QVERIFY(window->composer().parts().consecutiveGames);
        qInfo() << "consecutive maxMoves: game=" << window->composer().parts().consecutiveGames->currentGameNumber();
        QCOMPARE(window->composer().parts().consecutiveGames->currentGameNumber(),2);
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
        QTRY_VERIFY_WITH_TIMEOUT(window->composer().parts().match->isGameOver()
                                 && window->composer().parts().consecutiveGames->currentGameNumber() == 2, 10000);
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
    void consecutiveSwapKeepsEngineTime() {
        // 1局ごとに手番を入れ替えると、持ち時間もエンジンと一緒に入れ替わる
        configureEngineGames(1);
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/isSwitchTurnEachGame", true);
        settings.setValue("GameSettings/isGroupBoxSecondPlayerTimeSettingsChecked", true);
        settings.setValue("GameSettings/basicTimeMinutes1", 5);
        settings.setValue("GameSettings/basicTimeMinutes2", 3);
        settings.sync();
        startWindowGame();
        const auto first = window->composer().parts().document->timeRecord().timeControl();
        QCOMPARE(first.black.mainMs, qint64(300000));
        QCOMPARE(first.white.mainMs, qint64(180000));
        GameInfoTable table{window->findChild<GameInfoPane*>()->tableView()};
        QCOMPARE(gameInfoValue(table, GameInfoKeys::kTimeControl), QStringLiteral("先手 05:00 / 後手 03:00"));
        dialogTimer.start(10);
        QTRY_COMPARE_WITH_TIMEOUT(window->composer().parts().consecutiveGames->currentGameNumber(), 2, 5000);
        dialogTimer.stop();
        const auto second = window->composer().parts().document->timeRecord().timeControl();
        QCOMPARE(second.black.mainMs, qint64(180000));
        QCOMPARE(second.white.mainMs, qint64(300000));
        QTRY_COMPARE(gameInfoValue(table, GameInfoKeys::kTimeControl), QStringLiteral("先手 03:00 / 後手 05:00"));
        // 2局目もダイアログ経由と同じく対局者と開始日時を対局情報に入れる
        QCOMPARE(gameInfoValue(table, GameInfoKeys::kBlackPlayer), QStringLiteral("Audit USI"));
        QCOMPARE(gameInfoValue(table, GameInfoKeys::kWhitePlayer), QStringLiteral("Audit USI"));
        QVERIFY(!gameInfoValue(table, GameInfoKeys::kStartDateTime).isEmpty());
        QVERIFY(window->findChild<ShogiView*>()->chrome()->card(Turn::Black)->nameLabel()->fullText().contains(QStringLiteral("Audit USI")));
    }
    void handicapGameInfoUsesShitateUwate() {
        auto& settings = SettingsCommon::openSettings();
        settings.setValue("GameSettings/humanName1", QStringLiteral("下手さん"));
        settings.setValue("GameSettings/humanName2", QStringLiteral("上手さん"));
        settings.setValue("GameSettings/isGroupBoxSecondPlayerTimeSettingsChecked", true);
        settings.setValue("GameSettings/basicTimeMinutes1", 1);
        settings.setValue("GameSettings/byoyomiSec1", 2);
        settings.setValue("GameSettings/basicTimeMinutes2", 2);
        settings.setValue("GameSettings/byoyomiSec2", 3);
        settings.sync();
        requestedPreset = 4;  // 角落ち
        startWindowGame();
        auto* controller = window->findChild<GameInfoPane*>();
        GameInfoTable table{controller->tableView()};
        QCOMPARE(gameInfoValue(table, GameInfoKeys::kShitatePlayer), QStringLiteral("下手さん"));
        QCOMPARE(gameInfoValue(table, GameInfoKeys::kUwatePlayer), QStringLiteral("上手さん"));
        QCOMPARE(gameInfoValue(table, GameInfoKeys::kTimeControl), QStringLiteral("下手 01:00+2 / 上手 02:00+3"));
        for (const auto& item : controller->gameInfo())
            QVERIFY(item.key != GameInfoKeys::kBlackPlayer && item.key != GameInfoKeys::kWhitePlayer);
        const KifuDocument& document = *window->composer().parts().document;
        const QString kif = KifuExport::lines(document, KifuFormat::Kif).join(QLatin1Char('\n'));
        QVERIFY2(kif.contains(QStringLiteral("下手：下手さん")), qPrintable(kif));
        QVERIFY(kif.contains(QStringLiteral("上手：上手さん")));
        QVERIFY(kif.contains(QStringLiteral("持ち時間：下手 01:00+2 / 上手 02:00+3")));
        QVERIFY(!kif.contains(QStringLiteral("先手：")));
        const QString csa = KifuExport::lines(document, KifuFormat::Csa).join(QLatin1Char('\n'));
        QVERIFY2(csa.contains(QStringLiteral("N+下手さん")), qPrintable(csa));
        QVERIFY(csa.contains(QStringLiteral("N-上手さん")));
        QVERIFY(csa.contains(QStringLiteral("$TIME+:60+2+0")));
        QVERIFY(csa.contains(QStringLiteral("$TIME-:120+3+0")));
    }
private:
    static QString gameInfoValue(const GameInfoTable& table, const QString& key) {
        for (int row = 0; row < table.rowCount(); ++row) {
            if (table.text(row, 0) == key) return table.text(row, 1);
        }
        return {};
    }
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
