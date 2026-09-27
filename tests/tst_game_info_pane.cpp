#include <QtTest>
#include <QClipboard>
#include <QDialog>
#include <QLineEdit>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include "gameinfopanecontroller.h"

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
    }

    void init()
    {
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
};

QTEST_MAIN(TestGameInfoPane)
#include "tst_game_info_pane.moc"
