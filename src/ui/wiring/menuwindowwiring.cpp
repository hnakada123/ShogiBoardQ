/// @file menuwindowwiring.cpp
/// @brief メニューウィンドウ配線クラスの実装

#include "menuwindowwiring.h"
#include "menuwindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDebug>

MenuWindowWiring::MenuWindowWiring(const Dependencies& deps, QObject* parent)
    : QObject(parent)
    , m_parentWidget(deps.parentWidget)
    , m_menuBar(deps.menuBar)
{
}

void MenuWindowWiring::ensureMenuWindow()
{
    if (m_menuWindow) return;

    m_menuWindow = new MenuWindow(m_parentWidget);

    // シグナルを接続
    connect(m_menuWindow, &MenuWindow::actionTriggered,
            this, &MenuWindowWiring::actionTriggered);
    connect(m_menuWindow, &MenuWindow::favoritesChanged,
            this, &MenuWindowWiring::favoritesChanged);

    // カテゴリ情報を収集して設定
    collectCategoriesFromMenuBar();

    qDebug().noquote() << "[MenuWindowWiring] MenuWindow created and connected";
}

void MenuWindowWiring::displayMenuWindow()
{
    ensureMenuWindow();

    // ウィンドウを表示する
    m_menuWindow->show();
    m_menuWindow->raise();
    m_menuWindow->activateWindow();
}

void MenuWindowWiring::collectCategoriesFromMenuBar()
{
    if (!m_menuBar || !m_menuWindow) return;

    QList<MenuWindow::CategoryInfo> categories;

    // メニューバーの各メニューを走査
    for (QAction* menuAction : m_menuBar->actions()) {
        QMenu* menu = menuAction->menu();
        if (!menu) continue;

        // メニューウィンドウ自身を含むメニューはスキップ（表示メニュー内にactionMenuがある場合）
        QString menuTitle = menu->title();
        menuTitle.remove(QChar('&'));  // アクセラレータを除去

        MenuWindow::CategoryInfo category;
        category.name = menu->objectName();
        category.displayName = menuTitle;

        // メニュー内のアクションを収集
        collectActionsFromMenu(menu, category.actions);

        // アクションが1つ以上ある場合のみカテゴリに追加
        if (!category.actions.isEmpty()) {
            categories.append(category);
        }
    }

    m_menuWindow->setCategories(categories);
}

// 再帰的にメニューからアクションを収集するヘルパー関数
static void collectActionsFromMenuHelper(QMenu* menu, QList<QAction*>& actions)
{
    if (!menu) return;

    for (QAction* action : menu->actions()) {
        if (action->isSeparator()) continue;

        // サブメニューがある場合は再帰的に処理
        QMenu* subMenu = action->menu();
        if (subMenu) {
            collectActionsFromMenuHelper(subMenu, actions);
        } else {
            // 通常のアクションを追加
            // objectNameが空でないものだけを追加
            // actionToolBarはメニューウィンドウには表示しない
            if (!action->objectName().isEmpty() &&
                action->objectName() != QStringLiteral("actionToolBar")) {
                actions.append(action);
            }
        }
    }
}

void MenuWindowWiring::collectActionsFromMenu(QMenu* menu, QList<QAction*>& actions)
{
    collectActionsFromMenuHelper(menu, actions);
}
