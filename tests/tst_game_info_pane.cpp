#include <QtTest>
#include <QClipboard>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include "gameinfopanecontroller.h"
#include "gamesettings.h"
#include "gameinfokeys.h"
#include "playerinfowiring.h"
#include "gamerecordmodel.h"
#include "engineanalysistab.h"
#include "engineinfowidget.h"

class TestGameInfoPane : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
    QScopedPointer<GameInfoPaneController> m_controller;
    QScopedPointer<QWidget> m_container;
    QTableWidget* m_table = nullptr;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void init()
    {
        GameSettings::setGameInfoFontSize(10);
        GameSettings::setGameInfoKeyColumnWidth(0);
        m_controller.reset(new GameInfoPaneController);
        m_container.reset(m_controller->containerWidget());
        m_controller->setGameInfo({{QStringLiteral("先手"), QStringLiteral("Black")},
                                  {QStringLiteral("後手"), QStringLiteral("White")},
                                  {QStringLiteral("棋戦"), QStringLiteral("Original")}});
        m_table = m_controller->tableWidget();
        m_container->resize(700, 300);
        m_container->show();
        m_container->activateWindow();
        m_table->setFocus();
        QTRY_VERIFY(m_table->hasFocus());
    }

    void cleanup()
    {
        m_container.reset();
        m_controller.reset();
    }

    void keyboardCellOperations()
    {
        m_table->setCurrentCell(0, 1);
        QTest::keyClick(m_table, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Black"));
        m_table->setCurrentCell(1, 1);
        QTest::keyClick(m_table, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(m_table->item(1, 1)->text(), QStringLiteral("Black"));
        QTest::keyClick(m_table, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(m_table->item(1, 1)->text(), QStringLiteral("White"));
        QTest::keyClick(m_table, Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(m_table->item(1, 1)->text(), QStringLiteral("Black"));
        QTest::keyClick(m_table, Qt::Key_X, Qt::ControlModifier);
        QCOMPARE(m_table->item(1, 1)->text(), QString());
        m_table->setCurrentCell(0, 0);
        QTest::keyClick(m_table, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(m_table->item(0, 0)->text(), QStringLiteral("先手"));
    }

    void toolbarRespectsEditorSelection()
    {
        m_table->setCurrentCell(0, 1);
        m_table->editItem(m_table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setSelection(1, 3);
        QTest::mouseClick(m_container->findChild<QToolButton*>(QStringLiteral("gameInfoCopy")), Qt::LeftButton);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("lac"));
        QTest::mouseClick(m_container->findChild<QToolButton*>(QStringLiteral("gameInfoCut")), Qt::LeftButton);
        QCOMPARE(editor->text(), QStringLiteral("Bk"));
        QTest::mouseClick(m_container->findChild<QToolButton*>(QStringLiteral("gameInfoPaste")), Qt::LeftButton);
        QCOMPARE(editor->text(), QStringLiteral("Black"));
        m_controller->commitPendingEditor();
        QVERIFY(!m_controller->isDirty());
    }

    void synchronizedNamesSurviveUndo()
    {
        m_table->item(2, 1)->setText(QStringLiteral("Edited event"));
        m_controller->updatePlayerNames(QStringLiteral("New black"), QStringLiteral("New white"));
        QVERIFY(m_controller->isDirty());
        m_controller->undo();
        QCOMPARE(m_table->item(0, 1)->text(), QStringLiteral("New black"));
        QCOMPARE(m_table->item(1, 1)->text(), QStringLiteral("New white"));
        QCOMPARE(m_table->item(2, 1)->text(), QStringLiteral("Original"));
        QVERIFY(!m_controller->isDirty());
        m_controller->redo();
        QCOMPARE(m_table->item(2, 1)->text(), QStringLiteral("Edited event"));
    }

    void commitIsSynchronous()
    {
        m_table->setCurrentCell(0, 1);
        m_table->editItem(m_table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(QStringLiteral("In progress"));
        QVERIFY(m_controller->isDirty());
        m_controller->commitPendingEditor();
        QCOMPARE(m_controller->gameInfo().at(0).value, QStringLiteral("In progress"));
        m_controller->applyChanges();
        QVERIFY(!m_controller->isDirty());
    }

    void endTimePreservesPendingEditorAndHistory()
    {
        m_table->setCurrentCell(0, 1);
        m_table->editItem(m_table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(QStringLiteral("Pending player"));
        QDialog dialog(m_container.data());
        QLineEdit otherEditor(&dialog);
        dialog.setModal(true);
        dialog.show();
        dialog.activateWindow();
        otherEditor.setFocus();
        QTRY_VERIFY(otherEditor.hasFocus());
        const QString endTime = QStringLiteral("2026/09/27 10:00:00");
        m_controller->updateGameInfoValue(QStringLiteral("終了日時"), endTime);
        QCOMPARE(m_table->item(0, 1)->text(), QStringLiteral("Pending player"));
        QVERIFY(m_controller->isDirty());
        m_controller->undo();
        QCOMPARE(m_table->item(0, 1)->text(), QStringLiteral("Black"));
        QCOMPARE(m_table->item(3, 1)->text(), endTime);
        QVERIFY(!m_controller->isDirty());
        m_controller->redo();
        QCOMPARE(m_table->item(0, 1)->text(), QStringLiteral("Pending player"));
        QCOMPARE(m_table->item(3, 1)->text(), endTime);
    }

    void loadResetsHistoryAndReadOnlyKeys()
    {
        m_controller->addRow();
        m_controller->commitPendingEditor();
        m_table->item(3, 0)->setText(QStringLiteral("Custom"));
        m_controller->undo();
        m_controller->redo();
        QVERIFY(m_table->item(3, 0)->flags() & Qt::ItemIsEditable);
        m_controller->setGameInfo({{QStringLiteral("先手"), QStringLiteral("Next game")}});
        m_controller->undo();
        m_controller->redo();
        QCOMPARE(m_table->rowCount(), 1);
        QCOMPARE(m_table->item(0, 1)->text(), QStringLiteral("Next game"));
        QVERIFY(!(m_table->item(0, 0)->flags() & Qt::ItemIsEditable));
        QVERIFY(!m_controller->isDirty());
    }

    void controlsFollowSelectionAndClipboard()
    {
        auto* cut = m_container->findChild<QToolButton*>(QStringLiteral("gameInfoCut"));
        auto* copy = m_container->findChild<QToolButton*>(QStringLiteral("gameInfoCopy"));
        auto* paste = m_container->findChild<QToolButton*>(QStringLiteral("gameInfoPaste"));
        auto* remove = m_container->findChild<QToolButton*>(QStringLiteral("gameInfoRemoveRow"));
        auto* undo = m_container->findChild<QToolButton*>(QStringLiteral("gameInfoUndo"));
        auto* redo = m_container->findChild<QToolButton*>(QStringLiteral("gameInfoRedo"));
        auto* apply = m_container->findChild<QPushButton*>(QStringLiteral("gameInfoApply"));
        QVERIFY(!undo->isEnabled());
        QVERIFY(!redo->isEnabled());
        QVERIFY(!apply->isEnabled());
        m_table->setCurrentCell(0, 0);
        QApplication::clipboard()->setText(QStringLiteral("new value"));
        QVERIFY(copy->isEnabled());
        QVERIFY(!cut->isEnabled());
        QVERIFY(!paste->isEnabled());
        m_table->setCurrentCell(0, 1);
        QVERIFY(cut->isEnabled());
        QVERIFY(paste->isEnabled());
        QApplication::clipboard()->clear();
        QVERIFY(!paste->isEnabled());
        QTest::mouseClick(cut, Qt::LeftButton);
        QVERIFY(undo->isEnabled());
        QVERIFY(apply->isEnabled());
        QTest::mouseClick(undo, Qt::LeftButton);
        QVERIFY(!undo->isEnabled());
        QVERIFY(redo->isEnabled());
        QVERIFY(!apply->isEnabled());
        m_table->clearSelection();
        QVERIFY(!remove->isEnabled());
        QVERIFY(!copy->isEnabled());
        QVERIFY(!cut->isEnabled());
        QVERIFY(!paste->isEnabled());
    }

    /// 案内文がボタンの次の行に回っても、余白があれば1行で表示する
    void editingHintStaysOnOneLine()
    {
        auto* status = m_container->findChild<QLabel*>(QStringLiteral("gameInfoEditing"));
        auto* apply = m_container->findChild<QPushButton*>(QStringLiteral("gameInfoApply"));
        QVERIFY(status && apply);
        // QLabel は折り返し可能だと、平均文字幅の 20 字分より長い1行を短く折り返した幅を sizeHint にする。
        // 訳文（英語など）と同じ程度の長さで確かめる。
        status->setText(QStringLiteral("Double-click a value to edit the game information"));
        const int textWidth = status->fontMetrics().horizontalAdvance(status->text());
        m_container->resize(qMax(apply->mapTo(m_container.data(), QPoint()).x() + apply->width(), textWidth) + 40, 300);
        QTRY_VERIFY(status->mapTo(m_container.data(), QPoint()).y() > apply->mapTo(m_container.data(), QPoint()).y());
        QVERIFY(status->height() < 2 * status->fontMetrics().lineSpacing());
        QVERIFY(status->width() >= status->fontMetrics().horizontalAdvance(status->text()));
    }

    void pendingEditEnablesApplyAndEscapeClearsIt()
    {
        auto* apply = m_container->findChild<QPushButton*>(QStringLiteral("gameInfoApply"));
        auto* status = m_container->findChild<QLabel*>(QStringLiteral("gameInfoEditing"));
        m_table->setCurrentCell(0, 1);
        m_table->editItem(m_table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QTest::keyClicks(editor, "Changed");
        QVERIFY(apply->isEnabled());
        QVERIFY(status->text().contains(QStringLiteral("未反映")));
        QTest::keyClick(editor, Qt::Key_Escape);
        QTRY_VERIFY(!apply->isEnabled());
        QCOMPARE(m_table->item(0, 1)->text(), QStringLiteral("Black"));
        m_table->editItem(m_table->currentItem());
        editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QTest::keyClicks(editor, "Updated");
        QSignalSpy updated(m_controller.data(), &GameInfoPaneController::gameInfoUpdated);
        QTest::mouseClick(apply, Qt::LeftButton);
        QCOMPARE(updated.count(), 1);
        QCOMPARE(m_table->item(0, 1)->text(), QStringLiteral("Updated"));
        QVERIFY(!m_controller->isDirty());
        QVERIFY(!apply->isEnabled());
    }

    void applyShortcutCommitsPendingEdit()
    {
        m_table->setCurrentCell(2, 1);
        m_table->editItem(m_table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QTest::keyClicks(editor, "New event");
        QSignalSpy updated(m_controller.data(), &GameInfoPaneController::gameInfoUpdated);
        QTest::keyClick(editor, Qt::Key_Return, Qt::ControlModifier);
        QTRY_COMPARE(updated.count(), 1);
        QCOMPARE(m_controller->gameInfo().at(2).value, QStringLiteral("New event"));
        QVERIFY(!m_controller->isDirty());
    }

    void removeRowsCanBeUndone()
    {
        auto* remove = m_container->findChild<QToolButton*>(QStringLiteral("gameInfoRemoveRow"));
        m_table->setCurrentCell(1, 1);
        QTest::mouseClick(remove, Qt::LeftButton);
        QCOMPARE(m_table->rowCount(), 2);
        QCOMPARE(m_table->item(1, 0)->text(), QStringLiteral("棋戦"));
        m_controller->undo();
        QCOMPARE(m_table->rowCount(), 3);
        QCOMPARE(m_table->item(1, 1)->text(), QStringLiteral("White"));
        QVERIFY(!(m_table->item(1, 0)->flags() & Qt::ItemIsEditable));
        QVERIFY(!m_controller->isDirty());
        m_controller->redo();
        QCOMPARE(m_table->rowCount(), 2);
        m_controller->addRow();
        m_controller->commitPendingEditor();
        QTest::mouseClick(remove, Qt::LeftButton);
        QCOMPARE(m_table->rowCount(), 2);
        m_controller->undo();
        QCOMPARE(m_table->rowCount(), 3);
        QVERIFY(m_table->item(2, 0)->flags() & Qt::ItemIsEditable);
        for (int i = 0; i < 3; ++i) QTest::mouseClick(remove, Qt::LeftButton);
        QCOMPARE(m_table->rowCount(), 0);
        QVERIFY(!remove->isEnabled());
        m_controller->undo();
        QCOMPARE(m_table->rowCount(), 1);
    }

    void longValuesWrapAndTooltipsStayCurrent()
    {
        const QString text = QStringLiteral("<長い備考> & ").repeated(35) + QStringLiteral("\n次の行");
        m_table->item(2, 1)->setText(text);
        const int wideHeight = m_table->rowHeight(2);
        QVERIFY(wideHeight > m_table->rowHeight(0));
        QVERIFY(m_table->item(2, 1)->toolTip().contains(QStringLiteral("&lt;長い備考&gt; &amp;")));
        m_container->resize(360, 400);
        QTRY_VERIFY(m_table->rowHeight(2) > wideHeight);
        m_controller->undo();
        QCOMPARE(m_table->item(2, 1)->toolTip(), QStringLiteral("<qt>Original</qt>"));
        m_controller->updateGameInfoValue(QStringLiteral("棋戦"), QStringLiteral("Automatic"));
        QCOMPARE(m_table->item(2, 1)->toolTip(), QStringLiteral("<qt>Automatic</qt>"));
        QVERIFY(!m_controller->isDirty());
    }

    void fontAndColumnWidthAreRestored()
    {
        m_controller->setGameInfo({{QStringLiteral("先手省略名"), QStringLiteral("Black")}});
        const int originalWidth = m_table->columnWidth(0);
        m_controller->setFontSize(24);
        QVERIFY(m_table->columnWidth(0) > originalWidth);
        QVERIFY(!m_container->findChild<QToolButton*>(QStringLiteral("gameInfoFontIncrease"))->isEnabled());
        m_controller->setFontSize(8);
        QVERIFY(!m_container->findChild<QToolButton*>(QStringLiteral("gameInfoFontDecrease"))->isEnabled());
        m_table->setColumnWidth(0, 180);
        m_controller->setGameInfo({{QStringLiteral("棋戦"), QStringLiteral("Next")}});
        QCOMPARE(m_table->columnWidth(0), 180);
        GameInfoPaneController restored;
        QScopedPointer<QWidget> restoredContainer(restored.containerWidget());
        QCOMPARE(restored.fontSize(), 8);
        QCOMPARE(restored.tableWidget()->columnWidth(0), 180);
    }

    void initialValuesAreEmptyWithDisplayOnlyHints()
    {
        m_controller->resetGameInfo();
        QCOMPARE(m_table->rowCount(), 9);
        const QStringList keys = {QStringLiteral("対局日"), QStringLiteral("開始日時"),
                                  QStringLiteral("先手"), QStringLiteral("後手"), QStringLiteral("手合割"),
                                  QStringLiteral("持ち時間"), QStringLiteral("棋戦"),
                                  QStringLiteral("場所"), QStringLiteral("備考")};
        for (int row = 0; row < keys.size(); ++row) {
            QCOMPARE(m_table->item(row, 0)->text(), keys.at(row));
            QCOMPARE(m_table->item(row, 1)->text(), row == 4 ? QStringLiteral("平手") : QString());
        }
        QVERIFY(!m_controller->isDirty());
        QVERIFY(m_table->item(1, 1)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("未開始")));
        m_table->setCurrentCell(2, 1);
        m_table->editItem(m_table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QVERIFY(editor->text().isEmpty());
        QVERIFY(editor->placeholderText().contains(QStringLiteral("名前を入力")));
        m_controller->commitPendingEditor();
        QVERIFY(!m_controller->isDirty());

        GameRecordModel record;
        GameRecordModel::ExportContext context;
        context.gameInfoItems = m_controller->gameInfo();
        context.gameInfoProvided = true;
        const QString kif = record.toKifLines(context).join(QLatin1Char('\n'));
        QVERIFY(!kif.contains(QStringLiteral("未開始")));
        QVERIFY(!kif.contains(QStringLiteral("未設定")));
        QVERIFY(!kif.contains(QStringLiteral("任意入力")));
        QVERIFY(kif.contains(QStringLiteral("開始日時：\n")));

        m_controller->setGameInfo({{GameInfoKeys::kStartDateTime, {}}});
        QVERIFY(!m_table->item(0, 1)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("未開始")));
    }

    void thinkingPanelsFollowMatchMode_data()
    {
        QTest::addColumn<bool>("tabFirst");
        QTest::newRow("tab-before-controller") << true;
        QTest::newRow("tab-after-controller") << false;
    }

    void thinkingPanelsFollowMatchMode()
    {
        QFETCH(bool, tabFirst);
        QWidget parent;
        EngineAnalysisTab tab;
        auto* page = tab.createThinkingPage(&parent);
        const auto views = page->findChildren<QTableView*>(QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(views.size(), 2);
        PlayerInfoWiring::Dependencies deps;
        deps.parentWidget = &parent;
        PlayerInfoWiring wiring(deps);
        if (tabFirst) wiring.setAnalysisTab(&tab);
        wiring.onPlayerNamesResolved({}, {}, "Engine A", "Engine B",
                                     static_cast<int>(PlayMode::EvenEngineVsEngine));
        if (!tabFirst) wiring.setAnalysisTab(&tab);
        QVERIFY(tab.info1()->isVisibleTo(page));
        QVERIFY(tab.info2()->isVisibleTo(page));
        QVERIFY(views.at(0)->isVisibleTo(page));
        QVERIFY(views.at(1)->isVisibleTo(page));

        wiring.onPlayerNamesResolved("Human", {}, {}, "Engine B",
                                     static_cast<int>(PlayMode::EvenHumanVsEngine));
        QVERIFY(!tab.info2()->isVisibleTo(page));
        QVERIFY(!views.at(1)->isVisibleTo(page));
        wiring.onPlayerNamesResolved({}, {}, "Engine A", "Engine B",
                                     static_cast<int>(PlayMode::HandicapEngineVsEngine));
        QVERIFY(tab.info2()->isVisibleTo(page));
        QVERIFY(views.at(1)->isVisibleTo(page));
    }

    void startingGameKeepsMetadataAndPendingEdits()
    {
        QWidget parent;
        PlayerInfoWiring::Dependencies deps;
        deps.parentWidget = &parent;
        PlayerInfoWiring wiring(deps);
        wiring.addGameInfoTabAtStartup();
        auto* controller = wiring.gameInfoController();
        QScopedPointer<QWidget> container(controller->containerWidget());
        auto* table = controller->tableWidget();
        container->resize(700, 400);
        container->show();
        container->activateWindow();
        table->setFocus();
        QTRY_VERIFY(table->hasFocus());
        table->item(6, 1)->setText(QStringLiteral("練習対局"));
        table->item(7, 1)->setText(QStringLiteral("自宅"));
        controller->updateGameInfoValue(GameInfoKeys::kEndDateTime, QStringLiteral("2025/01/01 10:00:00"));
        controller->updateGameInfoValue(QStringLiteral("消費時間"), QStringLiteral("前局の消費時間"));
        table->setCurrentCell(8, 1);
        table->editItem(table->currentItem());
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(QStringLiteral("入力途中の備考"));
        const QDateTime start(QDate(2026, 10, 2), QTime(15, 30, 5));
        wiring.setGameInfoForMatchStart(start, QStringLiteral("太郎"), QStringLiteral("花子"),
                                       QStringLiteral("平手"), true, {300000, 30000, 0}, {300000, 30000, 0});
        QCOMPARE(table->rowCount(), 9);
        QCOMPARE(table->item(0, 1)->text(), QStringLiteral("2026/10/02"));
        QCOMPARE(table->item(1, 1)->text(), QStringLiteral("2026/10/02 15:30:05"));
        QCOMPARE(table->item(2, 1)->text(), QStringLiteral("太郎"));
        QCOMPARE(table->item(3, 1)->text(), QStringLiteral("花子"));
        QCOMPARE(table->item(5, 1)->text(), QStringLiteral("05:00+30"));
        QCOMPARE(table->item(6, 1)->text(), QStringLiteral("練習対局"));
        QCOMPARE(table->item(7, 1)->text(), QStringLiteral("自宅"));
        QCOMPARE(table->item(8, 1)->text(), QStringLiteral("入力途中の備考"));
        QVERIFY(!controller->isDirty());
        wiring.setGameInfoForMatchStart(start.addSecs(3600), QStringLiteral("花子"), QStringLiteral("太郎"),
                                       QStringLiteral("平手"), false, {}, {});
        QCOMPARE(table->item(5, 1)->text(), QStringLiteral("無制限"));
        QCOMPARE(table->item(8, 1)->text(), QStringLiteral("入力途中の備考"));

        // 駒落ちは対局者を下手・上手で書き、前の対局の先手・後手の行を残さない。
        // 先後で違う持ち時間は両方を書く。
        wiring.setGameInfoForMatchStart(start, QStringLiteral("下手さん"), QStringLiteral("上手さん"),
                                       QStringLiteral("角落ち"), true, {60000, 2000, 0}, {120000, 3000, 0});
        QCOMPARE(table->rowCount(), 9);
        QCOMPARE(table->item(2, 0)->text(), GameInfoKeys::kShitatePlayer);
        QCOMPARE(table->item(2, 1)->text(), QStringLiteral("下手さん"));
        QCOMPARE(table->item(3, 0)->text(), GameInfoKeys::kUwatePlayer);
        QCOMPARE(table->item(3, 1)->text(), QStringLiteral("上手さん"));
        QCOMPARE(table->item(5, 1)->text(), QStringLiteral("下手 01:00+2 / 上手 02:00+3"));
        for (const auto& item : controller->gameInfo()) {
            QVERIFY(item.key != GameInfoKeys::kBlackPlayer);
            QVERIFY(item.key != GameInfoKeys::kWhitePlayer);
        }
        controller->updatePlayerNames(QStringLiteral("新しい下手"), QStringLiteral("新しい上手"));
        QCOMPARE(table->item(2, 1)->text(), QStringLiteral("新しい下手"));
        QCOMPARE(table->item(3, 1)->text(), QStringLiteral("新しい上手"));
        wiring.setGameInfoForMatchStart(start, QStringLiteral("太郎"), QStringLiteral("花子"),
                                       QStringLiteral("平手"), true, {60000, 2000, 0}, {120000, 3000, 0});
        QCOMPARE(table->rowCount(), 9);
        QCOMPARE(table->item(2, 0)->text(), GameInfoKeys::kBlackPlayer);
        QCOMPARE(table->item(3, 0)->text(), GameInfoKeys::kWhitePlayer);
        QCOMPARE(table->item(5, 1)->text(), QStringLiteral("先手 01:00+2 / 後手 02:00+3"));
        controller->resetGameInfo();
        QVERIFY(table->item(6, 1)->text().isEmpty());
        QVERIFY(table->item(1, 1)->text().isEmpty());
        QVERIFY(table->item(1, 1)->data(Qt::AccessibleTextRole).toString().contains(QStringLiteral("未開始")));
    }
};

QTEST_MAIN(TestGameInfoPane)
#include "tst_game_info_pane.moc"
