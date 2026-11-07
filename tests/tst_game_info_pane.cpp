#include <QtTest>
#include <QAction>
#include <QCheckBox>
#include <QClipboard>
#include <QDialog>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QStandardItemModel>
#include <QStyledItemDelegate>
#include <QTranslator>
#include <QTableView>
#include <QTableWidget>
#include <QTemporaryDir>
#include <QToolButton>
#include "gameinfopane.h"
#include "buttonstyles.h"
#include "commenteditorpanel.h"
#include "game_info_table_helper.h"
#include "header_drag_helper.h"
#include "gamesettings.h"
#include "gameinfoeditmodel.h"
#include "gameinfokeys.h"
#include "playerinfopresenter.h"
#include "enginedisplaypresenter.h"
#include "engineslots.h"
#include "shogiclock.h"
#include "kifubranchtree.h"
#include "kifudocument.h"
#include "kifuexport.h"
#include "thinkingpanel.h"
#include "thinkingtableview.h"
#include "shogienginethinkingmodel.h"
#include "enginestatusmodel.h"
#include "engineinfowidget.h"
#include "analysissettings.h"
#include "elidelabel.h"
#include "shogiview.h"
#include "appactivitystate.h"
#include "sfenutils.h"
#include "matchplayers.h"
#include "checkable_action.h"
#include "plaintooltip.h"

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
        GameSettings::gameInfoColumnWidths().set({});
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

    /// 元に戻す〜貼り付けのボタンは、対局情報とコメント欄で同じ見た目と大きさ（ほかのパネルのボタンと同じ）
    void editButtonsLookTheSameInEveryPane()
    {
        CommentEditorPanel comment;
        const QStringList names = {QStringLiteral("gameInfoUndo"), QStringLiteral("gameInfoRedo"),
                                   QStringLiteral("gameInfoCut"), QStringLiteral("gameInfoCopy"),
                                   QStringLiteral("gameInfoPaste")};
        const auto commentButtons = comment.findChildren<QToolButton*>();
        for (const QString& name : names) {
            auto* gameInfo = m_controller->findChild<QToolButton*>(name);
            QVERIFY2(gameInfo && gameInfo->defaultAction(), qPrintable(name));
            QToolButton* inComment = nullptr;
            for (QToolButton* button : commentButtons) {
                if (button->defaultAction() && button->defaultAction()->text() == gameInfo->defaultAction()->text())
                    inComment = button;
            }
            QVERIFY2(inComment, qPrintable(name));
            QCOMPARE(gameInfo->styleSheet(), ButtonStyles::panelToolButton());
            QCOMPARE(inComment->styleSheet(), gameInfo->styleSheet());
            QCOMPARE(inComment->iconSize(), gameInfo->iconSize());
            QCOMPARE(inComment->minimumSize(), gameInfo->minimumSize());
        }
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

    /// 表に描く文字・項目名の列幅・読み上げ・ツールチップは、モデルの PresentedTextRole の1つから作る
    /// （項目名・内容の表記と、空の内容の案内文。監査 2026-11-01 M13）
    void cellsShowPresentedText()
    {
        // 項目名の表記を切り替える（訳の一覧の代わりに「棋戦」だけを訳す）
        class KeyTranslator : public QTranslator
        {
        public:
            bool isEmpty() const override { return false; }
            QString translate(const char* context, const char* source, const char*, int) const override
            {
                if (qstrcmp(context, "KifuPresentation") == 0 && QString::fromUtf8(source) == GameInfoKeys::kEvent)
                    return QStringLiteral("Tournament with a long translated name");
                return {};
            }
        } translator;
        QCoreApplication::installTranslator(&translator);
        m_controller->setGameInfo({{GameInfoKeys::kEvent, QStringLiteral("Original")}, {GameInfoKeys::kSite, QString()}});

        const QString translatedKey = QStringLiteral("Tournament with a long translated name");
        const QString placeholder = QStringLiteral("任意入力");
        const auto presented = [this](int row, int column) {
            return m_table.index(row, column).data(GameInfoEditModel::PresentedTextRole).toString();
        };
        QCOMPARE(presented(0, 0), translatedKey);
        QCOMPARE(presented(0, 1), QStringLiteral("Original"));
        QCOMPARE(presented(1, 0), GameInfoKeys::kSite);
        QCOMPARE(presented(1, 1), placeholder);
        QCOMPARE(m_table.text(1, 1), QString());   // 案内文は値に入れない

        // 描く文字: 表のデリゲートの大きさが、その文字だけを持つ項目の大きさと同じ
        QStyleOptionViewItem option;
        option.initFrom(m_table.view);
        option.widget = m_table.view;
        QStandardItemModel reference;
        QStyledItemDelegate plain;
        const auto drawnSize = [&](int row, int column) {
            return m_table.view->itemDelegateForIndex(m_table.index(row, column))->sizeHint(option, m_table.index(row, column));
        };
        const auto sizeOf = [&](const QString& text) {
            reference.clear();
            reference.appendRow(new QStandardItem(text));
            return plain.sizeHint(option, reference.index(0, 0));
        };
        QVERIFY(sizeOf(translatedKey) != sizeOf(GameInfoKeys::kEvent));
        QCOMPARE(drawnSize(0, 0), sizeOf(translatedKey));
        QCOMPARE(drawnSize(1, 1), sizeOf(placeholder));

        // 項目名の列幅も表記した項目名で測る（利用者が幅を決めていなければ）
        const int keyWidth = m_table.view->fontMetrics().horizontalAdvance(translatedKey) + 24;
        QCOMPARE(m_table.view->columnWidth(0), qBound(120, keyWidth, 320));
        QCOMPARE(m_table.accessibleText(0, 0), translatedKey);
        QCOMPARE(m_table.toolTip(1, 1), plainToolTip(placeholder));
        QCoreApplication::removeTranslator(&translator);
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
        QCOMPARE(m_table.toolTip(2, 1), plainToolTip(QStringLiteral("Original")));
        m_controller->updateGameInfoValue(QStringLiteral("棋戦"), QStringLiteral("Automatic"));
        QCOMPARE(m_table.toolTip(2, 1), plainToolTip(QStringLiteral("Automatic")));
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

    void initialValuesLeavePlayerNamesEmptyWithDisplayOnlyHints()
    {
        m_controller->setGameInfo(KifuDocument::defaultHeader(), true);
        QCOMPARE(m_table.rowCount(), 9);
        const QStringList keys = {QStringLiteral("対局日"), QStringLiteral("開始日時"),
                                  QStringLiteral("先手"), QStringLiteral("後手"), QStringLiteral("手合割"),
                                  QStringLiteral("持ち時間"), QStringLiteral("棋戦"),
                                  QStringLiteral("場所"), QStringLiteral("備考")};
        // 対局者の名前は空にし、入力の案内だけを出す（盤では「先手」「後手」と呼ぶ。表示の語はファイルに入れない）
        const QStringList values = {{}, {}, {}, {}, QStringLiteral("平手"), {}, {}, {}, {}};
        for (int row = 0; row < keys.size(); ++row) {
            QCOMPARE(m_table.text(row, 0), keys.at(row));
            QCOMPARE(m_table.text(row, 1), values.at(row));
        }
        QVERIFY(!m_controller->model()->canUndo());
        QVERIFY(m_table.accessibleText(1, 1).contains(QStringLiteral("未開始")));
        QVERIFY(m_table.accessibleText(2, 1).contains(QStringLiteral("名前を入力")));
        m_table.setCurrentCell(2, 1);
        m_table.editCurrent();
        auto* editor = qobject_cast<QLineEdit*>(QApplication::focusWidget());
        QVERIFY(editor);
        QCOMPARE(editor->text(), QString());
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
        QVERIFY(kif.contains(QStringLiteral("先手：\n")));
        QVERIFY(kif.contains(QStringLiteral("後手：\n")));

        m_controller->setGameInfo({{GameInfoKeys::kStartDateTime, {}}});
        QVERIFY(!m_table.accessibleText(0, 1).contains(QStringLiteral("未開始")));
    }

    void thinkingPanelsFollowMatchMode()
    {
        QWidget parent;
        CheckableAction match, ponder;
        ThinkingPanel tab({&match, &ponder}, &parent);
        auto* page = &tab;
        const auto views = page->findChildren<QTableView*>(QString(), Qt::FindDirectChildrenOnly);
        QCOMPARE(views.size(), 2);
        KifuBranchTree tree;
        KifuDocument document(&tree);
        // 2つ目のエンジン欄は、文書の対局者から EngineDisplayPresenter が決める
        ShogiClock clock;
        EngineSlots engineSlots(&clock);
        EngineDisplayPresenter::Deps deps;
        deps.engines = &engineSlots;
        deps.document = &document;
        deps.thinkingPanel = &tab;
        EngineDisplayPresenter displays(deps);
        MatchPlayers engines;
        engines.seat(Turn::Black).engine = true;
        engines.seat(Turn::White).engine = true;
        engines.seat(Turn::Black).engineName = QStringLiteral("Engine A");
        engines.seat(Turn::White).engineName = QStringLiteral("Engine B");
        document.beginMatch(engines, {}, false);
        QVERIFY(tab.info1()->isVisibleTo(page));
        QVERIFY(tab.info2()->isVisibleTo(page));
        QVERIFY(views.at(0)->isVisibleTo(page));
        QVERIFY(views.at(1)->isVisibleTo(page));

        MatchPlayers humanEngine;
        humanEngine.seat(Turn::White).engine = true;
        humanEngine.seat(Turn::Black).humanName = QStringLiteral("Human");
        humanEngine.seat(Turn::White).engineName = QStringLiteral("Engine B");
        document.beginMatch(humanEngine, {}, false);
        QVERIFY(!tab.info2()->isVisibleTo(page));
        QVERIFY(!views.at(1)->isVisibleTo(page));
        document.beginMatch(engines, {}, false);
        QVERIFY(tab.info2()->isVisibleTo(page));
        QVERIFY(views.at(1)->isVisibleTo(page));
    }

    // 情報帯の手番の印は、対局者から決める。E1 を片方の側の対局者として使わない動作が始まったら次の対局まで出さない
    void engineBandsShowTheSideEachEnginePlays()
    {
        QWidget parent;
        CheckableAction match, ponder;
        ThinkingPanel tab({&match, &ponder}, &parent);
        KifuBranchTree tree;
        KifuDocument document(&tree);
        ShogiClock clock;
        EngineSlots engineSlots(&clock);
        AppActivityState activity;
        EngineDisplayPresenter::Deps deps;
        deps.engines = &engineSlots;
        deps.document = &document;
        deps.activity = &activity;
        deps.thinkingPanel = &tab;
        EngineDisplayPresenter displays(deps);
        const auto mark = [](EngineInfoWidget* info) {
            auto* label = info->findChild<QLabel*>(QStringLiteral("engineInfoSide"));
            return label->isHidden() ? QString() : label->text();
        };
        const QString black = QStringLiteral("▲");
        const QString white = QStringLiteral("△");
        QCOMPARE(mark(tab.info1()), QString());
        QCOMPARE(mark(tab.info2()), QString());

        MatchPlayers engines;
        engines.seat(Turn::Black).engine = true;
        engines.seat(Turn::White).engine = true;
        document.beginMatch(engines, {}, false);
        QCOMPARE(mark(tab.info1()), black);
        QCOMPARE(mark(tab.info2()), white);

        // エンジンが1台なら、どちらの側でも E1 の帯に出す
        MatchPlayers engineAsWhite;
        engineAsWhite.seat(Turn::White).engine = true;
        document.beginMatch(engineAsWhite, {}, false);
        QCOMPARE(mark(tab.info1()), white);
        QCOMPARE(mark(tab.info2()), QString());
        MatchPlayers engineAsBlack;
        engineAsBlack.seat(Turn::Black).engine = true;
        document.beginMatch(engineAsBlack, {}, false);
        QCOMPARE(mark(tab.info1()), black);
        document.beginMatch(MatchPlayers{}, {}, false);
        QCOMPARE(mark(tab.info1()), QString());

        // 通信対局の文書の対局者はサーバーの名前だけなので、司令塔の知らせで決める
        displays.showMatchEngine(Turn::White, QStringLiteral("Engine"));
        QCOMPARE(mark(tab.info1()), white);
        QCOMPARE(mark(tab.info2()), QString());

        // 検討・詰み探索・棋譜解析の E1 は片方の側を指さない。終わっても次の対局まで出さない
        QObject owner;
        for (const auto running : {AppActivityState::Activity::DuringConsideration,
                                   AppActivityState::Activity::DuringTsumeSearch,
                                   AppActivityState::Activity::DuringAnalysis}) {
            document.beginMatch(engines, {}, false);
            QCOMPARE(mark(tab.info1()), black);
            ActivityLease lease = activity.begin(running, &owner);
            QCOMPARE(mark(tab.info1()), QString());
            QCOMPARE(mark(tab.info2()), QString());
            lease.reset();
            QCOMPARE(mark(tab.info1()), QString());
        }

        // 対局の動作状態では消さない
        document.beginMatch(engines, {}, false);
        ActivityLease game = activity.begin(AppActivityState::Activity::DuringGame, &owner);
        QCOMPARE(mark(tab.info1()), black);
        game.reset();
        QCOMPARE(mark(tab.info1()), black);   // 終局後も、対局した側のまま

        // 棋譜を置き換えたら消す
        document.startNew(SfenUtils::hirateSfen());
        QCOMPARE(mark(tab.info1()), QString());
        QCOMPARE(mark(tab.info2()), QString());
    }

    // 2つのエンジンの表は1つの列幅を使う。どちらの表で変えても両方が同じ幅になり、次に作る表にも残る
    void thinkingTablesShareColumnWidths()
    {
        const auto saved = AnalysisSettings::thinkingViewColumnWidths(AnalysisSettings::ThinkingTable::Match);
        const QList<int> before = saved();
        saved.set({});
        ShogiEngineThinkingModel thinking1;
        ShogiEngineThinkingModel thinking2;
        EngineStatusModel log1;
        EngineStatusModel log2;
        CheckableAction match, ponder;
        ThinkingPanel tab({&match, &ponder});
        tab.setModels(&thinking1, &thinking2, &log1, &log2);
        tab.setSecondEngineVisible(true);
        tab.resize(1000, 600);
        tab.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tab));
        ThinkingTableView* view1 = tab.view1();
        ThinkingTableView* view2 = tab.view2();
        for (int column = 0; column < ShogiEngineThinkingModel::PvColumn; ++column)
            QCOMPARE(view1->columnWidth(column), view2->columnWidth(column));

        HeaderDrag::resizeSection(view1->horizontalHeader(), ShogiEngineThinkingModel::NodesColumn, 140);
        QCOMPARE(view1->columnWidth(ShogiEngineThinkingModel::NodesColumn), 140);
        QCOMPARE(view2->columnWidth(ShogiEngineThinkingModel::NodesColumn), 140);

        HeaderDrag::resizeSection(view2->horizontalHeader(), ShogiEngineThinkingModel::ScoreColumn, 95);
        QCOMPARE(view2->columnWidth(ShogiEngineThinkingModel::ScoreColumn), 95);
        QCOMPARE(view1->columnWidth(ShogiEngineThinkingModel::ScoreColumn), 95);
        QCOMPARE(view1->columnWidth(ShogiEngineThinkingModel::NodesColumn), 140);
        QCOMPARE(view1->columnWidth(ShogiEngineThinkingModel::PvColumn), view2->columnWidth(ShogiEngineThinkingModel::PvColumn));

        ThinkingPanel restored({&match, &ponder});
        restored.setModels(&thinking1, &thinking2, &log1, &log2);
        QCOMPARE(restored.view1()->columnWidth(ShogiEngineThinkingModel::ScoreColumn), 95);
        QCOMPARE(restored.view2()->columnWidth(ShogiEngineThinkingModel::NodesColumn), 140);
        saved.set(before);
    }

    // 情報帯は思考の表に続けて置き、幅も表と揃える
    void engineBandsSitDirectlyAboveThinkingTables()
    {
        CheckableAction match, ponder;
        ThinkingPanel tab({&match, &ponder});
        tab.setSecondEngineVisible(true);
        tab.resize(1000, 600);
        tab.show();
        QVERIFY(QTest::qWaitForWindowExposed(&tab));
        for (const auto& [info, view] : {std::pair<EngineInfoWidget*, QWidget*>{tab.info1(), tab.view1()},
                                         {tab.info2(), tab.view2()}}) {
            // 幅によっては帯が折り返すので、行の増加と配置を待つ
            QTRY_COMPARE(info->geometry().bottom() + 1, view->geometry().top());
            QCOMPARE(info->geometry().left(), view->geometry().left());
            QCOMPARE(info->width(), view->width());
        }
        QCOMPARE(tab.view1()->height(), tab.view2()->height());
    }

    void thinkingFontButtonsPrecedeArrowControls()
    {
        AnalysisSettings::thinkingFontSize.set(10);
        QWidget parent;
        CheckableAction match, ponder;
        ThinkingPanel tab({&match, &ponder}, &parent);
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

    /// アプリの既定の書体が 10pt でない環境（Windows 9pt・macOS 13pt）でも、思考ドックの表と情報欄は
    /// 起動直後から保存した大きさ（未保存なら 10pt）で出す。A+→A- で同じ「10」に戻したときと同じ大きさ
    void thinkingPanelStartsAtTheSavedSizeWhateverTheAppFont()
    {
        const QFont appFont = QApplication::font();
        for (const int appSize : {9, 13}) {
            QFont font = appFont;
            font.setPointSize(appSize);
            QApplication::setFont(font);
            AnalysisSettings::thinkingFontSize.remove();
            QWidget parent;
            CheckableAction match, ponder;
            ThinkingPanel tab({&match, &ponder}, &parent);
            const auto views = tab.findChildren<QTableView*>(QString(), Qt::FindDirectChildrenOnly);
            QCOMPARE(views.size(), 2);
            for (QTableView* view : views) QCOMPARE(view->font().pointSize(), 10);
            for (EngineInfoWidget* info : {tab.info1(), tab.info2()}) QCOMPARE(info->font().pointSize(), 10);
        }
        QApplication::setFont(appFont);
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
        PlayerInfoPresenter wiring(deps);   // 文書と動作状態の知らせを自分で受ける
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲先手"));
        // 対局情報欄で名前を書き換えると、対局していなければすぐ盤の名前も変わる
        QList<KifGameInfoItem> edited = document.header();
        for (KifGameInfoItem& item : edited)
            if (item.key == GameInfoKeys::kBlackPlayer) item.value = QStringLiteral("一郎");
        document.editHeader(edited);
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲一郎"));
        document.editHeader(KifuDocument::defaultHeader());
        QCOMPARE(view.chrome()->card(Turn::White)->nameLabel()->fullText(), QStringLiteral("▽後手"));
        QVERIFY(document.headerPlayerName(Turn::Black).isEmpty());   // 盤の「先手」「後手」は表示の補い

        // 対局中は対局者の名前（対局情報を書き換えても変えない）
        MatchPlayers humans;
        humans.seat(Turn::Black).humanName = QStringLiteral("太郎");
        humans.seat(Turn::White).humanName = QStringLiteral("花子");
        document.beginMatch(humans, TimeControl{}, false);
        ActivityLease game = activity.begin(AppActivityState::Activity::DuringGame, &document);
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲太郎"));
        edited = document.header();
        for (KifGameInfoItem& item : edited)
            if (item.key == GameInfoKeys::kBlackPlayer) item.value = QStringLiteral("一郎");
        document.editHeader(edited);
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲太郎"));
        // 終局したら対局情報の名前
        game.reset();
        QCOMPARE(view.chrome()->card(Turn::Black)->nameLabel()->fullText(), QStringLiteral("▲一郎"));

        // 対局後の「新規」でも前の対局者名を残さない
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
        document.addPendingEditor(hooks);
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
        players.seat(Turn::Black).humanName = QStringLiteral("太郎");
        players.seat(Turn::White).humanName = QStringLiteral("花子");
        const QDateTime before = QDateTime::currentDateTime();
        document.beginMatch(players, TimeControl{{300000, 30000, 0}, {300000, 30000, 0}}, false);
        const QDateTime after = QDateTime::currentDateTime();
        QCOMPARE(table.rowCount(), 9);
        // 対局日と開始日時は対局を始めた時刻
        const QDateTime start = QDateTime::fromString(table.text(1, 1), QStringLiteral("yyyy/MM/dd HH:mm:ss"));
        QVERIFY2(start.isValid(), qPrintable(table.text(1, 1)));
        QVERIFY(start >= before.addMSecs(-before.time().msec()) && start <= after);
        QCOMPARE(table.text(0, 1), start.toString(QStringLiteral("yyyy/MM/dd")));
        QCOMPARE(table.text(2, 1), QStringLiteral("太郎"));
        QCOMPARE(table.text(3, 1), QStringLiteral("花子"));
        QCOMPARE(table.text(5, 1), QStringLiteral("05:00+30"));
        QCOMPARE(table.text(6, 1), QStringLiteral("練習対局"));
        QCOMPARE(table.text(7, 1), QStringLiteral("自宅"));
        QCOMPARE(table.text(8, 1), QStringLiteral("入力途中の備考"));
        QVERIFY(!controller->hasPendingEditor());
        std::swap(players.seat(Turn::Black).humanName, players.seat(Turn::White).humanName);
        document.beginMatch(players, TimeControl{}, false);
        QCOMPARE(table.text(5, 1), QStringLiteral("無制限"));
        QCOMPARE(table.text(8, 1), QStringLiteral("入力途中の備考"));

        // 駒落ちは対局者を下手・上手で書き、前の対局の先手・後手の行を残さない。
        // 先後で違う持ち時間は両方を書く。
        tree.setRootSfen(QStringLiteral("lnsgkgsnl/1r7/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL w - 1"));  // 角落ち
        players.seat(Turn::Black).humanName = QStringLiteral("下手さん");
        players.seat(Turn::White).humanName = QStringLiteral("上手さん");
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
        players.seat(Turn::Black).humanName = QStringLiteral("新しい下手");
        players.seat(Turn::White).humanName = QStringLiteral("新しい上手");
        document.beginMatch(players, TimeControl{}, true);
        QCOMPARE(table.text(2, 1), QStringLiteral("新しい下手"));
        QCOMPARE(table.text(3, 1), QStringLiteral("新しい上手"));
        QCOMPARE(table.text(5, 1), QStringLiteral("下手 01:00+2 / 上手 02:00+3"));
        QVERIFY(!controller->hasPendingEditor());

        tree.setRootSfen(SfenUtils::hirateSfen());
        players.seat(Turn::Black).humanName = QStringLiteral("太郎");
        players.seat(Turn::White).humanName = QStringLiteral("花子");
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
