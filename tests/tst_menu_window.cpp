/// @file tst_menu_window.cpp
/// @brief メニューウィンドウテスト

#include <QtTest>
#include <QSignalSpy>
#include <QAction>
#include <QTabWidget>

#include "menuwindow.h"

class TestMenuWindow : public QObject
{
    Q_OBJECT

private:
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
        cat.name = QStringLiteral("file");
        cat.displayName = QStringLiteral("File");
        cat.actions = actions;
        return {cat};
    }

private slots:
    void setFavorites_roundTrip();
    void setFavorites_empty();
    void setFavorites_unknownAction_skipped();
    void favoritesChanged_emittedOnAdd();
    void setCategories_createsTabs();
    void setCategories_empty_noExtraTabs();
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
    w.setFavorites(favs);

    QCOMPARE(w.favorites(), favs);
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
    cat1.name = QStringLiteral("file");
    cat1.displayName = QStringLiteral("File");
    cat1.actions = {actions[0], actions[1]};

    MenuWindow::CategoryInfo cat2;
    cat2.name = QStringLiteral("edit");
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

QTEST_MAIN(TestMenuWindow)
#include "tst_menu_window.moc"
