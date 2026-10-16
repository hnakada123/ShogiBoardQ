/// @file automationcommands_navigation.cpp
/// @brief メニュー項目、お気に入り、分岐ツリーの自動化操作

#include "automationcommands.h"
#include "automationinput.h"
#include "automationinteractions.h"
#include "automationactionpolicy.h"
#include "automationparams.h"
#include "automationwidgets.h"
#include "branchtreeitemroles.h"

#include <QAction>
#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QMainWindow>
#include <QMenu>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>

namespace {

QMenu* menuFor(QWidget* widget)
{
    QMenu* menu = qobject_cast<QMenu*>(widget);
    if (auto* button = qobject_cast<QPushButton*>(widget)) menu = button->menu();
    if (auto* button = qobject_cast<QToolButton*>(widget)) menu = button->menu();
    if (!menu) throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Widget must be a menu or a menu button"));
    return menu;
}

bool isAllowed(QMenu* menu, QAction* action)
{
    // 動的な定跡・履歴・レイアウトのメニューは作成元が明示的に公開する。
    for (QObject* parent = menu; parent; parent = parent->parent()) {
        if (parent->property("automationMenu").toBool()) return true;
    }
    return AutomationActionPolicy::isAllowed(action->objectName());
}

QJsonArray menuItems(QMenu* menu, int depth = 0)
{
    QJsonArray items;
    if (depth > 10) return items;
    int index = 0;
    const QList<QAction*> actions = menu->actions();
    for (auto* action : actions) {
        QJsonObject item{{QStringLiteral("index"), index++}, {QStringLiteral("text"), action->text()},
            {QStringLiteral("object_name"), action->objectName()}, {QStringLiteral("enabled"), action->isEnabled()},
            {QStringLiteral("visible"), action->isVisible()}, {QStringLiteral("checked"), action->isChecked()},
            {QStringLiteral("separator"), action->isSeparator()}, {QStringLiteral("allowed"), isAllowed(menu, action)}};
        if (action->menu()) item[QStringLiteral("items")] = menuItems(action->menu(), depth + 1);
        items.append(item);
    }
    return items;
}

void requireMenuInteractive(QWidget* window, QWidget* source, QMenu* menu)
{
    QWidget* popup = QApplication::activePopupWidget();
    if (popup && popup != menu && !menu->isAncestorOf(popup))
        throw AutomationError(ErrorKind::InvalidState, QStringLiteral("Another popup is active"));
    AutomationWidgets::requireInteractive(window, true);
    if (!qobject_cast<QMenu*>(source)) AutomationWidgets::requireInteractive(source, true);
}

QGraphicsItem* branchNode(QGraphicsView* view, int id)
{
    if (view->scene()) {
        const QList<QGraphicsItem*> sceneItems = view->scene()->items();
        for (auto* item : sceneItems) {
            const QVariant value = item->data(BranchTreeItemRoles::NodeId);
            if (value.isValid() && value.toInt() == id && item->isVisible()) return item;
        }
    }
    throw AutomationError(ErrorKind::NotFound, QStringLiteral("Branch node no longer exists; read widget.text again"));
}

} // namespace

void AutomationCommands::registerNavigationCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("menu.items"), [context](const QJsonObject& params) {
        auto* source = AutomationWidgets::requireWidget(context, params, false);
        auto* menu = menuFor(source);
        return QJsonObject{{QStringLiteral("items"), menuItems(menu)}};
    });

    dispatcher.registerMethod(QStringLiteral("menu.select"), [context](const QJsonObject& params) {
        auto* source = AutomationWidgets::requireWidget(context, params, false);
        QMenu* menu = menuFor(source);
        QWidget* window = AutomationWidgets::requireWindow(context, params.value(QStringLiteral("target")).toString());
        requireMenuInteractive(window, source, menu);
        const QJsonArray path = params.value(QStringLiteral("path")).toArray();
        QAction* action = nullptr;
        QMenu* current = menu;
        QList<QPointer<QAction>> parents;
        for (qsizetype i = 0; i < path.size(); ++i) {
            const QJsonObject indexParam{{QStringLiteral("index"), path[i]}};
            const int index = AutomationParams::requireInt(indexParam, QStringLiteral("index"), 0, static_cast<int>(current->actions().size()) - 1);
            action = current->actions().at(index);
            if (!action->isVisible() || !action->isEnabled() || action->isSeparator())
                throw AutomationError(ErrorKind::InvalidState, QStringLiteral("Menu item is hidden or disabled"));
            parents.append(action);
            if (i + 1 < path.size()) {
                if (!action->menu()) throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Path does not name a submenu"));
                current = action->menu();
            }
        }
        if (action->menu() || !isAllowed(current, action))
            throw AutomationError(ErrorKind::NotAllowed, QStringLiteral("Menu action is not allowed"));
        const QPointer<QAction> guarded(action);
        const QPointer<QMenu> parentMenu(current);
        const QPointer<QWidget> guardedWindow(window);
        const QPointer<QMenu> guardedMenu(menu);
        const auto check = [guardedWindow, source, guardedMenu, guarded, parentMenu, parents]() {
            if (!guardedWindow || !guardedMenu)
                throw AutomationError(ErrorKind::InvalidState, QStringLiteral("The menu was closed"));
            requireMenuInteractive(guardedWindow, source, guardedMenu);
            for (const auto& parent : parents)
                if (!parent || !parent->isEnabled() || !parent->isVisible())
                    throw AutomationError(ErrorKind::InvalidState, QStringLiteral("Menu item became hidden or disabled"));
            if (!guarded || !parentMenu || !isAllowed(parentMenu, guarded))
                throw AutomationError(ErrorKind::NotAllowed, QStringLiteral("Menu action is not allowed"));
        };
        const auto perform = [guardedMenu, guarded]() {
            const QList<QMenu*> submenus = guardedMenu->findChildren<QMenu*>();
            for (auto* submenu : submenus) submenu->hide();
            guardedMenu->hide();
            guarded->trigger();
        };
        return AutomationInteractions::queue({source, check, perform},
                                             QJsonObject{{QStringLiteral("text"), action->text()}});
    });

    dispatcher.registerMethod(QStringLiteral("menu.favorites"), [context](const QJsonObject& params) {
        if (!context.menuFavorites || !context.availableMenuActions || !context.setMenuFavorites)
            throw AutomationError(ErrorKind::NotFound, QStringLiteral("Menu window is not available"));
        const QStringList available = context.availableMenuActions();
        if (available.isEmpty())
            throw AutomationError(ErrorKind::NotFound, QStringLiteral("Menu window is not available"));
        if (params.contains(QStringLiteral("actions"))) {
            AutomationWidgets::requireInteractive(context.mainWindow);
            QStringList names;
            const QJsonArray requested = params.value(QStringLiteral("actions")).toArray();
            for (const auto& name : requested) {
                if (!name.isString() || !available.contains(name.toString()) || names.contains(name.toString()))
                    throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Unknown or duplicate favorite action"));
                names.append(name.toString());
            }
            context.setMenuFavorites(names);
        }
        return QJsonObject{{QStringLiteral("actions"), QJsonArray::fromStringList(context.menuFavorites())},
                           {QStringLiteral("available_actions"), QJsonArray::fromStringList(available)}};
    });

    dispatcher.registerMethod(QStringLiteral("branch.click"), [context](const QJsonObject& params) {
        auto* view = qobject_cast<QGraphicsView*>(AutomationWidgets::requireWidget(context, params));
        if (!view) throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Widget must be a branch graphics view"));
        const int id = params.value(QStringLiteral("id")).toInt();
        branchNode(view, id);
        return AutomationInteractions::queue(
            {view, [view]() { AutomationWidgets::requireInteractive(view); },
             [view, id]() {
                 auto* item = branchNode(view, id);
                 view->ensureVisible(item);
                 AutomationInput::sendClick(view->viewport(), view->mapFromScene(item->sceneBoundingRect().center()));
             }},
            QJsonObject{{QStringLiteral("id"), id}});
    });
}
