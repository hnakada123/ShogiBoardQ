/// @file tst_menu_window.cpp
/// @brief メニューウィンドウテスト

#include <QtTest>
#include <QSignalSpy>
#include <QAction>
#include <QTabWidget>
#include <QToolButton>
#include <QTemporaryDir>
#include <QSettings>
#include <QLineEdit>
#include <QScrollBar>
#include <QMimeData>
#include <QDropEvent>
#include <QDragEnterEvent>

#include "menubuttonwidget.h"
#include "appsettings.h"

#include "menuwindow.h"
#include "settingscommon.h"

class TestMenuWindow : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_config;

    /// テスト用アクションを作成
    QList<QAction*> makeTestActions(QObject* parent)
    {
        QList<QAction*> actions;
        for (const auto& name : {
            QStringLiteral("actionNew"),
            QStringLiteral("actionOpen"),
            QStringLiteral("actionSave"),
            QStringLiteral("actionClose")
        }) {
            auto* action = new QAction(name, parent);
            action->setObjectName(name);
            actions.append(action);
        }
        return actions;
    }

    /// テスト用カテゴリを作成
    QList<MenuWindow::CategoryInfo> makeTestCategories(const QList<QAction*>& actions)
    {
        MenuWindow::CategoryInfo cat;
        cat.displayName = QStringLiteral("File");
        cat.actions = actions;
        return {cat};
    }

private slots:
    void initTestCase()
    {
        QVERIFY(m_config.isValid());
        qputenv("XDG_CONFIG_HOME", m_config.path().toUtf8());
        qputenv("SHOGIBOARDQ_CONFIG_HOME", m_config.path().toUtf8());
        QCoreApplication::setApplicationName(QStringLiteral("MenuWindowTest"));
    }
    void init() { SettingsCommon::openSettings().clear(); }
    void setFavorites_roundTrip();
    void setFavorites_empty();
    void setFavorites_unknownAction_skipped();
    void favoritesChanged_emittedOnAdd();
    void setCategories_createsTabs();
    void setCategories_empty_noExtraTabs();
    void responsiveLayoutAndFilter();
    void fullLabelAndActionState();
    void buttonHeightsMatchWithinTab();
    void favoritesEditingAndDrop();
    void favoriteDragStartsOnButton();
    void settingsSavedWithoutClosingDock();
    void destroyedActionIsSafe();
};

// ----------------------------------------------------------
// お気に入りラウンドトリップテスト
// ----------------------------------------------------------

void TestMenuWindow::setFavorites_roundTrip()
{
    MenuWindow w;
    auto actions = makeTestActions(&w);
    w.setCategories(makeTestCategories(actions));

    QStringList favs = {QStringLiteral("actionNew"), QStringLiteral("actionSave")};
    QSignalSpy spy(&w, &MenuWindow::favoritesChanged);
    w.setFavorites(favs);

    QCOMPARE(w.favorites(), favs);
    QCOMPARE(spy.count(), 1);
    MenuWindow reopened;
    QCOMPARE(reopened.favorites(), favs);
}

void TestMenuWindow::setFavorites_empty()
{
    MenuWindow w;
    auto actions = makeTestActions(&w);
    w.setCategories(makeTestCategories(actions));

    // 初期状態で空のお気に入りを設定
    w.setFavorites({});
    QCOMPARE(w.favorites(), QStringList());

    // お気に入りを設定してからクリア
    w.setFavorites({QStringLiteral("actionNew")});
    QCOMPARE(w.favorites().size(), 1);

    w.setFavorites({});
    QCOMPARE(w.favorites(), QStringList());
}

void TestMenuWindow::setFavorites_unknownAction_skipped()
{
    MenuWindow w;
    auto actions = makeTestActions(&w);
    w.setCategories(makeTestCategories(actions));

    // 未登録のアクション名を含むリストを設定
    QStringList favs = {
        QStringLiteral("actionNew"),
        QStringLiteral("actionUnknown"),
        QStringLiteral("actionSave")
    };
    w.setFavorites(favs);

    // setFavorites はリストをそのまま保持する（findActionByName で解決できないものはタブに表示されないだけ）
    QCOMPARE(w.favorites(), favs);
}

// ----------------------------------------------------------
// シグナルテスト
// ----------------------------------------------------------

void TestMenuWindow::favoritesChanged_emittedOnAdd()
{
    MenuWindow w;
    auto actions = makeTestActions(&w);
    w.setCategories(makeTestCategories(actions));

    QSignalSpy spy(&w, &MenuWindow::favoritesChanged);
    QVERIFY(spy.isValid());

    // onAddToFavorites をトリガーする（private slot なので invokeMethod で呼び出し）
    QMetaObject::invokeMethod(&w, "onAddToFavorites",
                              Q_ARG(QString, QStringLiteral("actionNew")));

    QCOMPARE(spy.count(), 1);
    auto args = spy.first();
    QStringList emitted = args.at(0).value<QStringList>();
    QVERIFY(emitted.contains(QStringLiteral("actionNew")));
}

// ----------------------------------------------------------
// カテゴリ表示テスト
// ----------------------------------------------------------

void TestMenuWindow::setCategories_createsTabs()
{
    MenuWindow w;
    auto actions = makeTestActions(&w);

    // 2カテゴリ作成
    MenuWindow::CategoryInfo cat1;
    cat1.displayName = QStringLiteral("File");
    cat1.actions = {actions[0], actions[1]};

    MenuWindow::CategoryInfo cat2;
    cat2.displayName = QStringLiteral("Edit");
    cat2.actions = {actions[2], actions[3]};

    w.setCategories({cat1, cat2});

    // お気に入りタブ（1） + カテゴリタブ（2） = 3タブ
    auto* tabWidget = w.findChild<QTabWidget*>();
    QVERIFY(tabWidget);
    QCOMPARE(tabWidget->count(), 3);
}

void TestMenuWindow::setCategories_empty_noExtraTabs()
{
    MenuWindow w;
    w.setCategories({});

    // お気に入りタブ（1） のみ
    auto* tabWidget = w.findChild<QTabWidget*>();
    QVERIFY(tabWidget);
    QCOMPARE(tabWidget->count(), 1);
}


void TestMenuWindow::responsiveLayoutAndFilter()
{
    MenuWindow w;
    auto actions = makeTestActions(&w);
    actions[0]->setText(QStringLiteral("USI形式（現局面まで）"));
    actions[1]->setText(QStringLiteral("USI形式（全指し手）"));
    w.setCategories(makeTestCategories(actions));
    auto* tabs = w.findChild<QTabWidget*>();
    tabs->setCurrentIndex(1);
    auto* area = qobject_cast<QScrollArea*>(tabs->currentWidget());
    auto buttons = area->findChildren<MenuButtonWidget*>();
    QCOMPARE(buttons.size(), 4);
    w.resize(650, 420);
    w.show();
    QTRY_VERIFY(buttons[0]->isVisible());
    QTRY_COMPARE(buttons[0]->y(), buttons[3]->y());
    w.resize(330, 420);
    QTRY_VERIFY(buttons[3]->y() > buttons[0]->y());
    for (const auto* button : buttons) {
        const auto position = button->mapTo(area->viewport(), QPoint(0, 0));
        QVERIFY(position.x() >= 0);
        QVERIFY(position.x() + button->width() <= area->viewport()->width());
    }
    QCOMPARE(area->horizontalScrollBar()->maximum(), 0);

    auto* search = w.findChild<QLineEdit*>(QStringLiteral("menuSearch"));
    QVERIFY(search);
    search->setText(QStringLiteral("全指し手"));
    QTRY_VERIFY(buttons[0]->isHidden());
    QVERIFY(!buttons[1]->isHidden());
    QTRY_COMPARE(buttons[1]->pos(), QPoint(8, 8));
    search->setText(QStringLiteral("usi"));
    QTRY_VERIFY(!buttons[0]->isHidden());
    QVERIFY(!buttons[1]->isHidden());
    search->setText(QStringLiteral("存在しない項目"));
    auto* empty = area->findChild<QLabel*>(QStringLiteral("menuEmptyMessage"));
    QTRY_VERIFY(empty->isVisible());
    search->clear();
    QTRY_VERIFY(!empty->isVisible());

    // QAction の非表示指定も詰めて配置し、再表示を反映する。
    actions[0]->setVisible(false);
    QTRY_VERIFY(buttons[0]->isHidden());
    QTRY_COMPARE(buttons[1]->pos(), QPoint(8, 8));
    actions[0]->setVisible(true);
    QTRY_VERIFY(buttons[0]->isVisible());
}

void TestMenuWindow::fullLabelAndActionState()
{
    MenuWindow w;
    QAction action(QStringLiteral("USI形式（現局面まで）(&U)"), &w);
    action.setObjectName(QStringLiteral("actionUSI"));
    action.setCheckable(true);
    action.setShortcut(QKeySequence(QStringLiteral("Ctrl+U")));
    w.setCategories({{QStringLiteral("編集(E)"), {&action}}});
    auto* tabs = w.findChild<QTabWidget*>();
    QCOMPARE(tabs->tabText(1), QStringLiteral("編集"));
    tabs->setCurrentIndex(1);
    w.show();
    auto* card = tabs->currentWidget()->findChild<MenuButtonWidget*>();
    auto* button = card->findChild<QPushButton*>();
    auto* label = card->findChild<QLabel*>(QStringLiteral("menuActionText"));
    QCOMPARE(label->text(), QStringLiteral("USI形式（現局面まで）"));
    QVERIFY(button->toolTip().contains(QStringLiteral("Ctrl+U")));
    QSignalSpy triggered(&action, &QAction::triggered);
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(triggered.count(), 1);
    QVERIFY(action.isChecked());
    QVERIFY(button->isChecked());
    QVERIFY(card->findChild<QLabel*>(QStringLiteral("menuActionChecked"))->isVisible());
    action.setChecked(false);
    QVERIFY(!button->isChecked());
    action.setText(QStringLiteral("将棋盤と評価値グラフを画像に保存する"));
    QCOMPARE(label->text(), action.text());
    QCOMPARE(button->accessibleName(), action.text());
    card->updateSizes(48, 24, 16);
    QCoreApplication::processEvents();
    QVERIFY(label->rect().height() >= label->heightForWidth(label->width()));
    QVERIFY(button->rect().contains(label->geometry()));
    action.setEnabled(false);
    QVERIFY(!button->isEnabled());
    QTest::mouseClick(button, Qt::LeftButton);
    QCOMPARE(triggered.count(), 1);
}

void TestMenuWindow::buttonHeightsMatchWithinTab()
{
    MenuWindow w;
    auto actions = makeTestActions(&w);
    actions[0]->setText(QStringLiteral("新規"));
    actions[1]->setText(QStringLiteral("評価値グラフの画像をファイルに保存する長い名前の項目…"));
    QAction help(QStringLiteral("終了"), &w);
    help.setObjectName(QStringLiteral("actionQuit"));
    w.setCategories({{QStringLiteral("File"), actions}, {QStringLiteral("Help"), {&help}}});
    auto* tabs = w.findChild<QTabWidget*>();
    tabs->setCurrentIndex(1);
    w.resize(650, 420);
    w.show();
    const auto file = tabs->widget(1)->findChildren<MenuButtonWidget*>();
    QCOMPARE(file.size(), 4);
    QTRY_VERIFY(file[0]->isVisible());

    // 行数の違うボタンも同じタブでは高さと文字の位置を揃え、1 行の名前も省略しない
    const auto check = [&file]() {
        for (auto* card : file) {
            auto* label = card->findChild<QLabel*>(QStringLiteral("menuActionText"));
            QCOMPARE(card->height(), file[1]->height());
            QCOMPARE(label->y(), file[1]->findChild<QLabel*>(QStringLiteral("menuActionText"))->y());
            QVERIFY(label->height() >= label->heightForWidth(label->width()));
        }
    };
    QCoreApplication::processEvents();
    check();
    QVERIFY(file[1]->naturalHeight() > file[0]->naturalHeight());

    // 別のタブは自分のタブの項目だけで高さを決める
    auto* quit = tabs->widget(2)->findChild<MenuButtonWidget*>();
    QVERIFY(quit);
    QCOMPARE(quit->height(), quit->naturalHeight());
    QVERIFY(quit->height() < file[1]->height());

    // 文字やボタンの大きさを変えても揃ったままにする
    QTest::mouseClick(w.findChild<QToolButton*>(QStringLiteral("menuFontSizeIncrease")), Qt::LeftButton);
    QTest::mouseClick(w.findChild<QToolButton*>(QStringLiteral("menuButtonSizeDecrease")), Qt::LeftButton);
    QCoreApplication::processEvents();
    check();
}

void TestMenuWindow::favoritesEditingAndDrop()
{
    MenuWindow w;
    auto actions = makeTestActions(&w);
    w.setCategories(makeTestCategories(actions));
    auto* tabs = w.findChild<QTabWidget*>();
    w.show();
    auto* empty = tabs->widget(0)->findChild<QLabel*>(QStringLiteral("menuEmptyMessage"));
    QTRY_VERIFY(empty->isVisible());
    QTest::mouseClick(w.findChild<QToolButton*>(QStringLiteral("menuCustomize")), Qt::LeftButton);
    tabs->setCurrentIndex(1);
    auto cards = tabs->currentWidget()->findChildren<MenuButtonWidget*>();
    QSignalSpy triggered(actions[0], &QAction::triggered);
    actions[0]->setCheckable(true);
    // 無効な機能も実行せずにお気に入りへ登録できる。
    actions[0]->setEnabled(false);
    QTest::mouseClick(cards[0]->findChild<QPushButton*>(), Qt::LeftButton);
    QTest::mouseClick(cards[1]->findChild<QPushButton*>(QStringLiteral("menuAddFavorite")), Qt::LeftButton);
    QCOMPARE(triggered.count(), 0);
    QVERIFY(!cards[0]->findChild<QPushButton*>()->isChecked());
    QCOMPARE(w.favorites(), QStringList({actions[0]->objectName(), actions[1]->objectName()}));
    tabs->setCurrentIndex(0);
    QTRY_VERIFY(!empty->isVisible());
    auto favorites = tabs->currentWidget()->findChildren<MenuButtonWidget*>();
    // 再構築で deleteLater になったボタンを先に破棄する。
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    favorites = tabs->currentWidget()->findChildren<MenuButtonWidget*>();
    QCOMPARE(favorites.size(), 2);
    QMimeData mime;
    mime.setData("application/x-shogiboardq-menu-action", actions[0]->objectName().toUtf8());
    QDragEnterEvent enter(QPoint(10, 10), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(favorites[1], &enter);
    QVERIFY(enter.isAccepted());
    QDropEvent drop(QPointF(10, 10), Qt::MoveAction, &mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(favorites[1], &drop);
    QVERIFY(drop.isAccepted());
    QCOMPARE(w.favorites(), QStringList({actions[1]->objectName(), actions[0]->objectName()}));
    QCOMPARE(AppSettings::menuWindowFavorites(), w.favorites());
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    favorites = tabs->currentWidget()->findChildren<MenuButtonWidget*>();
    QTest::mouseClick(favorites[0]->findChild<QPushButton*>(QStringLiteral("menuRemoveFavorite")), Qt::LeftButton);
    QCOMPARE(w.favorites(), QStringList({actions[0]->objectName()}));
}

void TestMenuWindow::settingsSavedWithoutClosingDock()
{
    MenuWindow w;
    const auto actions = makeTestActions(&w);
    w.setCategories(makeTestCategories(actions));
    w.findChild<QTabWidget*>()->setCurrentIndex(1);
    auto* bigger = w.findChild<QToolButton*>(QStringLiteral("menuButtonSizeIncrease"));
    auto* font = w.findChild<QToolButton*>(QStringLiteral("menuFontSizeIncrease"));
    const int oldFontSize = AppSettings::menuWindowFontSize();
    font->click();
    QCOMPARE(AppSettings::menuWindowFontSize(), oldFontSize + 1);
    while (bigger->isEnabled()) bigger->click();
    QCOMPARE(AppSettings::menuWindowButtonSize(), 120);
    MenuWindow reopened;
    reopened.setCategories(makeTestCategories(actions));
    QCOMPARE(reopened.findChild<QTabWidget*>()->currentIndex(), 1);
    QVERIFY(!reopened.findChild<QToolButton*>(QStringLiteral("menuButtonSizeIncrease"))->isEnabled());
}

void TestMenuWindow::favoriteDragStartsOnButton()
{
    MenuWindow w;
    const auto actions = makeTestActions(&w);
    w.setCategories(makeTestCategories(actions));
    w.setFavorites({actions[0]->objectName()});
    w.show();
    w.findChild<QToolButton*>(QStringLiteral("menuCustomize"))->click();
    auto* card = w.findChild<QTabWidget*>()->widget(0)->findChild<MenuButtonWidget*>();
    auto* button = card->findChild<QPushButton*>();
    QSignalSpy started(card, &MenuButtonWidget::dragStarted);
    QSignalSpy triggered(actions[0], &QAction::triggered);
    QTimer cancel;
    cancel.setSingleShot(true);
    connect(&cancel, &QTimer::timeout, &QDrag::cancel);
    cancel.start(50);
    QTest::mousePress(button, Qt::LeftButton, Qt::NoModifier, QPoint(10, 10));
    const QPoint destination(10 + QApplication::startDragDistance(), 10);
    QMouseEvent move(QEvent::MouseMove, destination, button->mapToGlobal(destination),
                     Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(button, &move);
    QTest::mouseRelease(button, Qt::LeftButton);
    QCOMPARE(started.count(), 1);
    QCOMPARE(triggered.count(), 0);
}

void TestMenuWindow::destroyedActionIsSafe()
{
    MenuWindow w;
    auto* action = new QAction(QStringLiteral("Temporary"));
    action->setObjectName(QStringLiteral("temporary"));
    w.setCategories({{QStringLiteral("File"), {action}}});
    delete action;
    w.setFavorites({QStringLiteral("temporary")});
    w.setCategories({});
    QCOMPARE(w.findChild<QTabWidget*>()->count(), 1);
}

QTEST_MAIN(TestMenuWindow)
#include "tst_menu_window.moc"
