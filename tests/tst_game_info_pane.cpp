#include <QtTest>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QTableView>
#include <QTemporaryDir>
#include <QToolButton>
#include "gameinfopane.h"
#include "game_info_table_helper.h"
#include "header_drag_helper.h"
#include "gamesettings.h"
#include "gameinfoeditmodel.h"
#include "gameinfokeys.h"
#include "playerinfopresenter.h"
#include "enginedisplaypresenter.h"
#include "enginelifecyclemanager.h"
#include "shogiclock.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include "thinkingpanel.h"
#include "engineinfowidget.h"
#include "analysissettings.h"
#include "elidelabel.h"
#include "shogiview.h"
#include "appactivitystate.h"
#include "sfenutils.h"
#include "matchplayers.h"

class TestGameInfoPane : public QObject
{
    Q_OBJECT
    QTemporaryDir m_config;
    QScopedPointer<GameInfoPane> m_controller;
    GameInfoTable m_table;

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
    }

    void init()
    {
        GameSettings::gameInfoFontSize.set(10);
        GameSettings::gameInfoKeyColumnWidth.set(0);
        m_controller.reset(new GameInfoPane);
        m_controller->setGameInfo({{QStringLiteral("先手"), QStringLiteral("Black")},
                                  {QStringLiteral("後手"), QStringLiteral("White")},
                                  {QStringLiteral("棋戦"), QStringLiteral("Original")}});
        m_table.view = m_controller->tableView();
        m_controller->resize(700, 300);
        m_controller->show();
        m_controller->activateWindow();
        m_table.view->setFocus();
        QTRY_VERIFY(m_table.view->hasFocus());
    }

    void cleanup()
    {
        m_controller.reset();
    }

    void keyboardCellOperations()
    {
        m_table.setCurrentCell(0, 1);
        QTest::keyClick(m_table.view, Qt::Key_C, Qt::ControlModifier);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("Black"));
        m_table.setCurrentCell(1, 1);
        QTest::keyClick(m_table.view, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(m_table.text(1, 1), QStringLiteral("Black"));
        QTest::keyClick(m_table.view, Qt::Key_Z, Qt::ControlModifier);
        QCOMPARE(m_table.text(1, 1), QStringLiteral("White"));
        QTest::keyClick(m_table.view, Qt::Key_Y, Qt::ControlModifier);
        QCOMPARE(m_table.text(1, 1), QStringLiteral("Black"));
        QTest::keyClick(m_table.view, Qt::Key_X, Qt::ControlModifier);
        QCOMPARE(m_table.text(1, 1), QString());
        m_table.setCurrentCell(0, 0);
        QTest::keyClick(m_table.view, Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(m_table.text(0, 0), QStringLiteral("先手"));
    }

    void toolbarRespectsEditorSelection()
    {
        m_table.setCurrentCell(0, 1);
        m_table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setSelection(1, 3);
        QTest::mouseClick(m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoCopy")), Qt::LeftButton);
        QCOMPARE(QApplication::clipboard()->text(), QStringLiteral("lac"));
        QTest::mouseClick(m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoCut")), Qt::LeftButton);
        QCOMPARE(editor->text(), QStringLiteral("Bk"));
        QTest::mouseClick(m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoPaste")), Qt::LeftButton);
        QCOMPARE(editor->text(), QStringLiteral("Black"));
        m_controller->commitPendingEditor();
        QVERIFY(!m_controller->hasPendingEditor());
    }

    void synchronizedNamesSurviveUndo()
    {
        m_table.setText(2, 1, QStringLiteral("Edited event"));
        // 対局者名は文書が自動で書く（続ける対局の対局者）。元に戻す・やり直すで古い名前に戻さない
        m_controller->updateGameInfoValue(QStringLiteral("先手"), QStringLiteral("New black"));
        m_controller->updateGameInfoValue(QStringLiteral("後手"), QStringLiteral("New white"));
        m_controller->undo();
        QCOMPARE(m_table.text(0, 1), QStringLiteral("New black"));
        QCOMPARE(m_table.text(1, 1), QStringLiteral("New white"));
        QCOMPARE(m_table.text(2, 1), QStringLiteral("Original"));
        m_controller->redo();
        QCOMPARE(m_table.text(2, 1), QStringLiteral("Edited event"));
    }

    void commitIsSynchronous()
    {
        m_table.setCurrentCell(0, 1);
        m_table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(QStringLiteral("In progress"));
        QVERIFY(m_controller->hasPendingEditor());
        m_controller->commitPendingEditor();
        QCOMPARE(m_controller->gameInfo().at(0).value, QStringLiteral("In progress"));
        QVERIFY(!m_controller->hasPendingEditor());
    }

    void endTimePreservesPendingEditorAndHistory()
    {
        m_table.setCurrentCell(0, 1);
        m_table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(QStringLiteral("Pending player"));
        QDialog dialog(m_controller.data());
        QLineEdit otherEditor(&dialog);
        dialog.setModal(true);
        dialog.show();
        dialog.activateWindow();
        otherEditor.setFocus();
        QTRY_VERIFY(otherEditor.hasFocus());
        const QString endTime = QStringLiteral("2026/09/27 10:00:00");
        m_controller->updateGameInfoValue(QStringLiteral("終了日時"), endTime);
        QCOMPARE(m_table.text(0, 1), QStringLiteral("Pending player"));
        // 自動で書く前に入力中のセルを確定する
        QVERIFY(!m_controller->hasPendingEditor());
        m_controller->undo();
        QCOMPARE(m_table.text(0, 1), QStringLiteral("Black"));
        QCOMPARE(m_table.text(3, 1), endTime);
        m_controller->redo();
        QCOMPARE(m_table.text(0, 1), QStringLiteral("Pending player"));
        QCOMPARE(m_table.text(3, 1), endTime);
    }

    void loadResetsHistoryAndReadOnlyKeys()
    {
        m_controller->addRow();
        m_controller->commitPendingEditor();
        m_table.setText(3, 0, QStringLiteral("Custom"));
        m_controller->undo();
        m_controller->redo();
        QVERIFY(m_table.editable(3, 0));
        m_controller->setGameInfo({{QStringLiteral("先手"), QStringLiteral("Next game")}});
        m_controller->undo();
        m_controller->redo();
        QCOMPARE(m_table.rowCount(), 1);
        QCOMPARE(m_table.text(0, 1), QStringLiteral("Next game"));
        QVERIFY(!(m_table.editable(0, 0)));
        QVERIFY(!m_controller->model()->canUndo());
    }

    void controlsFollowSelectionAndClipboard()
    {
        auto* cut = m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoCut"));
        auto* copy = m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoCopy"));
        auto* paste = m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoPaste"));
        auto* remove = m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoRemoveRow"));
        auto* undo = m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoUndo"));
        auto* redo = m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoRedo"));
        // 編集は表に入れた時点で棋譜に入るので、「対局情報更新」のボタンは無い
        QVERIFY(!m_controller->findChild<QPushButton*>(QStringLiteral("gameInfoApply")));
        QVERIFY(!undo->isEnabled());
        QVERIFY(!redo->isEnabled());
        m_table.setCurrentCell(0, 0);
        QApplication::clipboard()->setText(QStringLiteral("new value"));
        QVERIFY(copy->isEnabled());
        QVERIFY(!cut->isEnabled());
        QVERIFY(!paste->isEnabled());
        m_table.setCurrentCell(0, 1);
        QVERIFY(cut->isEnabled());
        QVERIFY(paste->isEnabled());
        QApplication::clipboard()->clear();
        QVERIFY(!paste->isEnabled());
        QTest::mouseClick(cut, Qt::LeftButton);
        QVERIFY(undo->isEnabled());
        QTest::mouseClick(undo, Qt::LeftButton);
        QVERIFY(!undo->isEnabled());
        QVERIFY(redo->isEnabled());
        m_table.view->clearSelection();
        QVERIFY(!remove->isEnabled());
        QVERIFY(!copy->isEnabled());
        QVERIFY(!cut->isEnabled());
        QVERIFY(!paste->isEnabled());
    }

    /// 案内文がボタンの次の行に回っても、余白があれば1行で表示する
    void editingHintStaysOnOneLine()
    {
        auto* status = m_controller->findChild<QLabel*>(QStringLiteral("gameInfoEditing"));
        // 案内文の前の最後のボタン
        auto* apply = m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoRemoveRow"));
        QVERIFY(status && apply);
        // QLabel は折り返し可能だと、平均文字幅の 20 字分より長い1行を短く折り返した幅を sizeHint にする。
        // 訳文（英語など）と同じ程度の長さで確かめる。
        status->setText(QStringLiteral("Double-click a value to edit the game information"));
        const int textWidth = status->fontMetrics().horizontalAdvance(status->text());
        m_controller->resize(qMax(apply->mapTo(m_controller.data(), QPoint()).x() + apply->width(), textWidth) + 40, 300);
        QTRY_VERIFY(status->mapTo(m_controller.data(), QPoint()).y() > apply->mapTo(m_controller.data(), QPoint()).y());
        QVERIFY(status->height() < 2 * status->fontMetrics().lineSpacing());
        QVERIFY(status->width() >= status->fontMetrics().horizontalAdvance(status->text()));
    }

    /// 入力中のセルは確定するまで未保存に数え、Esc で捨てると数えない。確定すると表と文書に入る
    void pendingEditIsCountedUntilEscape()
    {
        m_table.setCurrentCell(0, 1);
        m_table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QTest::keyClicks(editor, "Changed");
        QVERIFY(m_controller->hasPendingEditor());
        QTest::keyClick(editor, Qt::Key_Escape);
        QTRY_VERIFY(!m_controller->hasPendingEditor());
        QCOMPARE(m_table.text(0, 1), QStringLiteral("Black"));
        m_table.editCurrent();
        editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QTest::keyClicks(editor, "Updated");
        QSignalSpy edited(m_controller.data(), &GameInfoPane::gameInfoEdited);
        m_controller->commitPendingEditor();
        QVERIFY(edited.count() >= 1);
        QCOMPARE(m_table.text(0, 1), QStringLiteral("Updated"));
        QVERIFY(!m_controller->hasPendingEditor());
    }

    /// Ctrl+Enter は入力中のセルを確定する
    void commitShortcutCommitsPendingEdit()
    {
        m_table.setCurrentCell(2, 1);
        m_table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QTest::keyClicks(editor, "New event");
        QSignalSpy edited(m_controller.data(), &GameInfoPane::gameInfoEdited);
        QTest::keyClick(editor, Qt::Key_Return, Qt::ControlModifier);
        QTRY_VERIFY(edited.count() >= 1);
        QCOMPARE(m_controller->gameInfo().at(2).value, QStringLiteral("New event"));
        QVERIFY(!m_controller->hasPendingEditor());
    }

    void removeRowsCanBeUndone()
    {
        auto* remove = m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoRemoveRow"));
        m_table.setCurrentCell(1, 1);
        QTest::mouseClick(remove, Qt::LeftButton);
        QCOMPARE(m_table.rowCount(), 2);
        QCOMPARE(m_table.text(1, 0), QStringLiteral("棋戦"));
        m_controller->undo();
        QCOMPARE(m_table.rowCount(), 3);
        QCOMPARE(m_table.text(1, 1), QStringLiteral("White"));
        QVERIFY(!(m_table.editable(1, 0)));
        m_controller->redo();
        QCOMPARE(m_table.rowCount(), 2);
        m_controller->addRow();
        m_controller->commitPendingEditor();
        QTest::mouseClick(remove, Qt::LeftButton);
        QCOMPARE(m_table.rowCount(), 2);
        m_controller->undo();
        QCOMPARE(m_table.rowCount(), 3);
        QVERIFY(m_table.editable(2, 0));
        for (int i = 0; i < 3; ++i) QTest::mouseClick(remove, Qt::LeftButton);
        QCOMPARE(m_table.rowCount(), 0);
        QVERIFY(!remove->isEnabled());
        m_controller->undo();
        QCOMPARE(m_table.rowCount(), 1);
    }

    void longValuesWrapAndTooltipsStayCurrent()
    {
        const QString text = QStringLiteral("<長い備考> & ").repeated(35) + QStringLiteral("\n次の行");
        m_table.setText(2, 1, text);
        const int wideHeight = m_table.view->rowHeight(2);
        QVERIFY(wideHeight > m_table.view->rowHeight(0));
        QVERIFY(m_table.toolTip(2, 1).contains(QStringLiteral("&lt;長い備考&gt; &amp;")));
        m_controller->resize(360, 400);
        QTRY_VERIFY(m_table.view->rowHeight(2) > wideHeight);
        m_controller->undo();
        QCOMPARE(m_table.toolTip(2, 1), QStringLiteral("<qt>Original</qt>"));
        m_controller->updateGameInfoValue(QStringLiteral("棋戦"), QStringLiteral("Automatic"));
        QCOMPARE(m_table.toolTip(2, 1), QStringLiteral("<qt>Automatic</qt>"));
    }

    void fontAndColumnWidthAreRestored()
    {
        m_controller->setGameInfo({{QStringLiteral("先手省略名"), QStringLiteral("Black")}});
        const int originalWidth = m_table.view->columnWidth(0);
        m_controller->setFontSize(24);
        QVERIFY(m_table.view->columnWidth(0) > originalWidth);
        QVERIFY(!m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoFontIncrease"))->isEnabled());
        m_controller->setFontSize(8);
        QVERIFY(!m_controller->findChild<QToolButton*>(QStringLiteral("gameInfoFontDecrease"))->isEnabled());
        HeaderDrag::resizeSection(m_table.view->horizontalHeader(), 0, 180);
        m_controller->setGameInfo({{QStringLiteral("棋戦"), QStringLiteral("Next")}});
        QCOMPARE(m_table.view->columnWidth(0), 180);
        GameInfoPane restored;
        QCOMPARE(restored.fontSize(), 8);
        QCOMPARE(restored.tableView()->columnWidth(0), 180);
    }

    void initialValuesHaveDefaultNamesAndDisplayOnlyHints()
    {
        m_controller->setGameInfo(KifuDocument::defaultHeader(), true);
        QCOMPARE(m_table.rowCount(), 9);
        const QStringList keys = {QStringLiteral("対局日"), QStringLiteral("開始日時"),
                                  QStringLiteral("先手"), QStringLiteral("後手"), QStringLiteral("手合割"),
                                  QStringLiteral("持ち時間"), QStringLiteral("棋戦"),
                                  QStringLiteral("場所"), QStringLiteral("備考")};
        // 対局者は名前が決まるまで「先手」「後手」と呼ぶ
        const QStringList values = {{}, {}, QStringLiteral("先手"), QStringLiteral("後手"),
                                    QStringLiteral("平手"), {}, {}, {}, {}};
        for (int row = 0; row < keys.size(); ++row) {
            QCOMPARE(m_table.text(row, 0), keys.at(row));
            QCOMPARE(m_table.text(row, 1), values.at(row));
        }
        QVERIFY(!m_controller->model()->canUndo());
        QVERIFY(m_table.accessibleText(1, 1).contains(QStringLiteral("未開始")));
        m_table.setCurrentCell(2, 1);
        m_table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QCOMPARE(editor->text(), QStringLiteral("先手"));
        QVERIFY(editor->placeholderText().contains(QStringLiteral("名前を入力")));
        m_controller->commitPendingEditor();
        QVERIFY(!m_controller->hasPendingEditor());
        QVERIFY(!m_controller->model()->canUndo());  // 同じ文字のままなら編集に数えない

        KifuBranchTree tree;
        KifuExportMetadata context;
        context.gameInfoItems = m_controller->gameInfo();
        const QString kif = KifuExport::lines(tree, context, KifuFormat::Kif).join(QLatin1Char('\n'));
        QVERIFY(!kif.contains(QStringLiteral("未開始")));
        QVERIFY(!kif.contains(QStringLiteral("未設定")));
        QVERIFY(!kif.contains(QStringLiteral("任意入力")));
        QVERIFY(kif.contains(QStringLiteral("開始日時：\n")));
        QVERIFY(kif.contains(QStringLiteral("先手：先手\n")));
        QVERIFY(kif.contains(QStringLiteral("後手：後手\n")));

        m_controller->setGameInfo({{GameInfoKeys::kStartDateTime, {}}});
        QVERIFY(!m_table.accessibleText(0, 1).contains(QStringLiteral("未開始")));
    }

    void thinkingPanelsFollowMatchMode()
    {
        QWidget parent;
        ThinkingPanel tab(&parent);
        auto* page = &tab;
        const auto views = page->findChildren<QTableView*>(QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(views.size(), 2);
        KifuBranchTree tree;
        KifuDocument document(&tree);
        // 2つ目のエンジン欄は、文書の対局者から EngineDisplayPresenter が決める
        ShogiClock clock;
        EngineLifecycleManager lifecycle(&clock);
        EngineDisplayPresenter::Deps deps;
        deps.engines = &lifecycle;
        deps.document = &document;
        deps.thinkingPanel = &tab;
        EngineDisplayPresenter displays(deps);
        MatchPlayers engines;
        engines.blackIsEngine = true;
        engines.whiteIsEngine = true;
        engines.blackEngineName = QStringLiteral("Engine A");
        engines.whiteEngineName = QStringLiteral("Engine B");
        document.beginMatch(engines, {}, false);
        QVERIFY(tab.info1()->isVisibleTo(page));
        QVERIFY(tab.info2()->isVisibleTo(page));
        QVERIFY(views.at(0)->isVisibleTo(page));
        QVERIFY(views.at(1)->isVisibleTo(page));

        MatchPlayers humanEngine;
        humanEngine.whiteIsEngine = true;
        humanEngine.blackHumanName = QStringLiteral("Human");
        humanEngine.whiteEngineName = QStringLiteral("Engine B");
        document.beginMatch(humanEngine, {}, false);
        QVERIFY(!tab.info2()->isVisibleTo(page));
        QVERIFY(!views.at(1)->isVisibleTo(page));
        document.beginMatch(engines, {}, false);
        QVERIFY(tab.info2()->isVisibleTo(page));
        QVERIFY(views.at(1)->isVisibleTo(page));
    }

    void thinkingFontButtonsPrecedeArrowControls()
    {
        AnalysisSettings::thinkingFontSize.set(10);
        QWidget parent;
        ThinkingPanel tab(&parent);
        auto* page = &tab;
        auto* decrease = page->findChild<QToolButton*>(QStringLiteral("thinkingFontDecrease"));
        auto* increase = page->findChild<QToolButton*>(QStringLiteral("thinkingFontIncrease"));
        auto* arrows = page->findChild<QCheckBox*>(QStringLiteral("matchArrows"));
        QVERIFY(decrease && increase && arrows);
        QVERIFY(tab.info1()->findChildren<QToolButton*>().isEmpty());
        page->resize(900, 400);
        parent.show();
        QTRY_VERIFY(arrows->isVisible());
        QVERIFY(decrease->geometry().right() < increase->geometry().left());
        QVERIFY(increase->geometry().right() < arrows->geometry().left());
        QVERIFY(qAbs(decrease->geometry().center().y() - arrows->geometry().center().y()) <= 2);
        QVERIFY(arrows->geometry().bottom() < tab.info1()->geometry().top());

        QTest::mouseClick(increase, Qt::LeftButton);
        QCOMPARE(AnalysisSettings::thinkingFontSize(), 11);
        QCOMPARE(tab.info1()->fontSize(), 11);
        QTest::mouseClick(decrease, Qt::LeftButton);
        QCOMPARE(AnalysisSettings::thinkingFontSize(), 10);
        QCOMPARE(tab.info1()->fontSize(), 10);
    }

    void startupShowsDefaultPlayerNamesOnBoard()
    {
        ShogiView view;
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        KifuDocument document(&tree);
        AppActivityState activity;
        PlayerInfoPresenter::Deps deps;
        deps.document = &document;
        deps.activity = &activity;
        deps.shogiView = &view;
        PlayerInfoPresenter wiring(deps);
        connect(&document, &KifuDocument::headerReset, &wiring, &PlayerInfoPresenter::syncBoardNamesWithGameInfo);
        connect(&document, &KifuDocument::headerEdited, &wiring, &PlayerInfoPresenter::showMatchPlayers);
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲先手"));
        // 対局情報欄で名前を書き換えると、対局していなければすぐ盤の名前も変わる
        QList<KifGameInfoItem> edited = document.header();
        for (KifGameInfoItem& item : edited)
            if (item.key == GameInfoKeys::kBlackPlayer) item.value = QStringLiteral("一郎");
        document.editHeader(edited);
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲一郎"));
        document.editHeader(KifuDocument::defaultHeader());
        QCOMPARE(view.chrome()->card(Turn::White)->nameLabel()->fullText(), QStringLiteral("▽後手"));
        // 対局していないときの表示でも対局情報の名前を残す
        wiring.showMatchPlayers();
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲先手"));
        QCOMPARE(document.headerPlayerName(Turn::Black), QStringLiteral("先手"));

        // 対局後の「新規」でも前の対局者名を残さない
        MatchPlayers humans;
        humans.blackHumanName = QStringLiteral("太郎");
        humans.whiteHumanName = QStringLiteral("花子");
        wiring.showPlayers(humans);
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲太郎"));
        document.startNew(SfenUtils::hirateSfen());
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲先手"));
        QCOMPARE(view.chrome()->card(Turn::White)->nameLabel()->fullText(), QStringLiteral("▽後手"));
    }

    /// 対局を始めると、文書が対局情報を作り直して対局情報欄に出す（入力中のセルと利用者の項目は残す）
    void startingGameKeepsMetadataAndPendingEdits()
    {
        KifuBranchTree tree;
        tree.setRootSfen(SfenUtils::hirateSfen());
        KifuDocument document(&tree);
        GameInfoPane gameInfo;
        auto* controller = &gameInfo;
        // 合成ルートと同じつなぎ方（mainwindowcomposer_wire.cpp）
        KifuDocument::Hooks hooks;
        hooks.commitPendingEdits = std::bind(&GameInfoPane::commitPendingEditor, controller);
        hooks.hasPendingEdits = std::bind(&GameInfoPane::hasPendingEditor, controller);
        document.setHooks(hooks);
        connect(&document, &KifuDocument::headerReset, controller, &GameInfoPane::setGameInfo);
        connect(&document, &KifuDocument::headerValueSet, controller, &GameInfoPane::updateGameInfoValue);
        connect(controller, &GameInfoPane::gameInfoEdited, &document, &KifuDocument::editHeader);
        controller->setGameInfo(document.header(), true);

        GameInfoTable table{controller->tableView()};
        controller->resize(700, 400);
        controller->show();
        controller->activateWindow();
        table.view->setFocus();
        QTRY_VERIFY(table.view->hasFocus());
        table.setText(6, 1, QStringLiteral("練習対局"));
        table.setText(7, 1, QStringLiteral("自宅"));
        controller->updateGameInfoValue(GameInfoKeys::kEndDateTime, QStringLiteral("2025/01/01 10:00:00"));
        controller->updateGameInfoValue(QStringLiteral("消費時間"), QStringLiteral("前局の消費時間"));
        table.setCurrentCell(8, 1);
        table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        editor->setText(QStringLiteral("入力途中の備考"));

        MatchPlayers players;
        players.blackHumanName = QStringLiteral("太郎");
        players.whiteHumanName = QStringLiteral("花子");
        document.beginMatch(players, TimeControl{{300000, 30000, 0}, {300000, 30000, 0}}, false);
        const QDateTime start = document.timeRecord().startDateTime();
        QCOMPARE(table.rowCount(), 9);
        QCOMPARE(table.text(0, 1), start.toString(QStringLiteral("yyyy/MM/dd")));
        QCOMPARE(table.text(1, 1), start.toString(QStringLiteral("yyyy/MM/dd HH:mm:ss")));
        QCOMPARE(table.text(2, 1), QStringLiteral("太郎"));
        QCOMPARE(table.text(3, 1), QStringLiteral("花子"));
        QCOMPARE(table.text(5, 1), QStringLiteral("05:00+30"));
        QCOMPARE(table.text(6, 1), QStringLiteral("練習対局"));
        QCOMPARE(table.text(7, 1), QStringLiteral("自宅"));
        QCOMPARE(table.text(8, 1), QStringLiteral("入力途中の備考"));
        QVERIFY(!controller->hasPendingEditor());
        std::swap(players.blackHumanName, players.whiteHumanName);
        document.beginMatch(players, TimeControl{}, false);
        QCOMPARE(table.text(5, 1), QStringLiteral("無制限"));
        QCOMPARE(table.text(8, 1), QStringLiteral("入力途中の備考"));

        // 駒落ちは対局者を下手・上手で書き、前の対局の先手・後手の行を残さない。
        // 先後で違う持ち時間は両方を書く。
        tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));  // 角落ち
        players.blackHumanName = QStringLiteral("下手さん");
        players.whiteHumanName = QStringLiteral("上手さん");
        document.beginMatch(players, TimeControl{{60000, 2000, 0}, {120000, 3000, 0}}, false);
        QCOMPARE(table.rowCount(), 9);
        QCOMPARE(table.text(2, 0), GameInfoKeys::kShitatePlayer);
        QCOMPARE(table.text(2, 1), QStringLiteral("下手さん"));
        QCOMPARE(table.text(3, 0), GameInfoKeys::kUwatePlayer);
        QCOMPARE(table.text(3, 1), QStringLiteral("上手さん"));
        QCOMPARE(table.text(5, 1), QStringLiteral("下手 01:00+2 / 上手 02:00+3"));
        const QList<KifGameInfoItem> handicapItems = controller->gameInfo();
        for (const auto& item : handicapItems) {
            QVERIFY(item.key != GameInfoKeys::kBlackPlayer);
            QVERIFY(item.key != GameInfoKeys::kWhitePlayer);
        }
        // 棋譜の途中から続ける対局は、対局情報を残して対局者名だけを書き換える
        players.blackHumanName = QStringLiteral("新しい下手");
        players.whiteHumanName = QStringLiteral("新しい上手");
        document.beginMatch(players, TimeControl{}, true);
        QCOMPARE(table.text(2, 1), QStringLiteral("新しい下手"));
        QCOMPARE(table.text(3, 1), QStringLiteral("新しい上手"));
        QCOMPARE(table.text(5, 1), QStringLiteral("下手 01:00+2 / 上手 02:00+3"));
        QVERIFY(!controller->hasPendingEditor());

        tree.setRootSfen(SfenUtils::hirateSfen());
        players.blackHumanName = QStringLiteral("太郎");
        players.whiteHumanName = QStringLiteral("花子");
        document.beginMatch(players, TimeControl{{60000, 2000, 0}, {120000, 3000, 0}}, false);
        QCOMPARE(table.rowCount(), 9);
        QCOMPARE(table.text(2, 0), GameInfoKeys::kBlackPlayer);
        QCOMPARE(table.text(3, 0), GameInfoKeys::kWhitePlayer);
        QCOMPARE(table.text(5, 1), QStringLiteral("先手 01:00+2 / 後手 02:00+3"));

        // 「新規」は既定の対局情報に戻す
        document.startNew(SfenUtils::hirateSfen());
        QVERIFY(table.text(6, 1).isEmpty());
        QVERIFY(table.text(1, 1).isEmpty());
        QVERIFY(table.accessibleText(1, 1).contains(QStringLiteral("未開始")));
    }
};

QTEST_MAIN(TestGameInfoPane)
#include "tst_game_info_pane.moc"
