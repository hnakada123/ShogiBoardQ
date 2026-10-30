/// @file menuwindowcontroller.cpp
/// @brief メニューウィンドウの表示の実装

#include "menuwindowcontroller.h"
#include "menuwindow.h"

#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QDockWidget>
#include <utility>
#include "logcategories.h"

MenuWindowController::MenuWindowController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_menuBar(deps.menuBar)
    , m_menuWindow(new MenuWindow(deps.parentWidget))
{
    // カテゴリ情報を収集して設定（ドックの表示の切り替えはドックを作った後に collectCategoriesFromMenuBar() で足す）
    collectCategoriesFromMenuBar();
}

void MenuWindowController::toggleWindow()
{
    if (!m_dock) return;
    m_dock->toggleViewAction()->trigger();
    if (m_dock->isVisible()) m_dock->raise();
}

void MenuWindowController::collectCategoriesFromMenuBar()
{
    if (!m_menuBar) return;

    QList<MenuWindow::CategoryInfo> categories;

    // メニューバーの各メニューを走査
    const auto menuBarActions = m_menuBar->actions();
    for (QAction* menuAction : std::as_const(menuBarActions)) {
        QMenu* menu = menuAction->menu();
        if (!menu) continue;

        // メニューウィンドウ自身を含むメニューはスキップ（表示メニュー内にactionMenuがある場合）
        QString menuTitle = menu->title();
        menuTitle.remove(QChar('&'));  // アクセラレータを除去

        MenuWindow::CategoryInfo category;
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

    const auto menuActions = menu->actions();
    for (QAction* action : std::as_const(menuActions)) {
        if (action->isSeparator()) continue;

        // サブメニューがある場合は再帰的に処理
        QMenu* subMenu = action->menu();
        if (subMenu) {
            collectActionsFromMenuHelper(subMenu, actions);
        } else {
            // 通常のアクションを追加
            // objectNameが空でないものだけを追加
            // actionToolBar と、保存済みレイアウトが無いときの仮項目（押せない）はメニューウィンドウには表示しない
            const QString name = action->objectName();
            if (!name.isEmpty() && name != QStringLiteral("actionToolBar")
                && name != QStringLiteral("actionSavedLayoutsPlaceholder")) {
                actions.append(action);
            }
        }
    }
}

void MenuWindowController::collectActionsFromMenu(QMenu* menu, QList<QAction*>& actions)
{
    collectActionsFromMenuHelper(menu, actions);
}
