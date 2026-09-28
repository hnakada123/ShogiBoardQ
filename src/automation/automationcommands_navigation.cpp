/// @file automationcommands_navigation.cpp
/// @brief メニュー項目、お気に入り、分岐ツリーの自動化操作

#include "automationcommands.h"
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
#include <QMouseEvent>
#include <QPointer>
#include <QPushButton>
#include <QToolButton>

namespace {

QMenu* menuFor(QWidget* widget)
{
    QMenu* menu = qobject_cast<QMenu*>(widget);
    if (auto* button = qobject_cast<QPushButton*>(widget)) menu = button->menu();
    if (auto* button = qobject_cast<QToolButton*>(widget)) menu = button->menu();
    if (!menu) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Widget must be a menu or a menu button"));
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
    for (auto* action : menu->actions()) {
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
        throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Another popup is active"));
    AutomationWidgets::requireInteractive(window, true);
    if (!qobject_cast<QMenu*>(source)) AutomationWidgets::requireInteractive(source, true);
}

QGraphicsItem* branchNode(QGraphicsView* view, int id)
{
    if (view->scene()) {
        for (auto* item : view->scene()->items()) {
            const QVariant value = item->data(BranchTreeItemRoles::NodeId);
            if (value.isValid() && value.toInt() == id && item->isVisible()) return item;
        }
    }
    throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("Branch node no longer exists; read widget.text again"));
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
        QWidget* window = AutomationWidgets::requireWindow(context, AutomationParams::optionalString(params, QStringLiteral("target")));
        requireMenuInteractive(window, source, menu);
        const QJsonValue pathValue = params.value(QStringLiteral("path"));
        if (!pathValue.isArray() || pathValue.toArray().isEmpty() || pathValue.toArray().size() > 10)
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("path must contain 1-10 menu indices"));
        const QJsonArray path = pathValue.toArray();
        QAction* action = nullptr;
        QMenu* current = menu;
        QList<QPointer<QAction>> parents;
        for (qsizetype i = 0; i < path.size(); ++i) {
            const QJsonObject indexParam{{QStringLiteral("index"), path[i]}};
            const int index = AutomationParams::requireInt(indexParam, QStringLiteral("index"), 0, static_cast<int>(current->actions().size()) - 1);
            action = current->actions().at(index);
            if (!action->isVisible() || !action->isEnabled() || action->isSeparator())
                throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Menu item is hidden or disabled"));
            parents.append(action);
            if (i + 1 < path.size()) {
                if (!action->menu()) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Path does not name a submenu"));
                current = action->menu();
            }
        }
        if (action->menu() || !isAllowed(current, action))
            throw AutomationError(AutomationErrorCode::NotAllowed, QStringLiteral("Menu action is not allowed"));
        const QPointer<QAction> guarded(action);
        const QPointer<QMenu> parentMenu(current);
        const QPointer<QWidget> guardedWindow(window);
        const QPointer<QMenu> guardedMenu(menu);
        AutomationDeferredCall::schedule([guardedWindow, source, guardedMenu, guarded, parentMenu, parents]() {
            try {
                if (!guardedWindow || !guardedMenu) return;
                requireMenuInteractive(guardedWindow, source, guardedMenu);
                for (const auto& parent : parents)
                    if (!parent || !parent->isEnabled() || !parent->isVisible()) return;
                if (!guarded || !parentMenu || !isAllowed(parentMenu, guarded)) return;
                for (auto* submenu : guardedMenu->findChildren<QMenu*>()) submenu->hide();
                guardedMenu->hide();
                guarded->trigger();
            } catch (const AutomationError&) { }
        }, source);
        return QJsonObject{{QStringLiteral("queued"), true}, {QStringLiteral("text"), action->text()}};
    });

    dispatcher.registerMethod(QStringLiteral("menu.favorites"), [context](const QJsonObject& params) {
        if (!context.menuFavorites || !context.availableMenuActions || !context.setMenuFavorites)
            throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("Menu window is not available"));
        const QStringList available = context.availableMenuActions();
        if (available.isEmpty())
            throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("Menu window is not available"));
        if (params.contains(QStringLiteral("actions"))) {
            AutomationWidgets::requireInteractive(context.mainWindow);
            const auto value = params.value(QStringLiteral("actions"));
            if (!value.isArray()) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("actions must be an array"));
            QStringList names;
            for (const auto& name : value.toArray()) {
                if (!name.isString() || !available.contains(name.toString()) || names.contains(name.toString()))
                    throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Unknown or duplicate favorite action"));
                names.append(name.toString());
            }
            context.setMenuFavorites(names);
        }
        return QJsonObject{{QStringLiteral("actions"), QJsonArray::fromStringList(context.menuFavorites())},
                           {QStringLiteral("available_actions"), QJsonArray::fromStringList(available)}};
    });

    dispatcher.registerMethod(QStringLiteral("branch.click"), [context](const QJsonObject& params) {
        auto* view = qobject_cast<QGraphicsView*>(AutomationWidgets::requireWidget(context, params));
        if (!view) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Widget must be a branch graphics view"));
        const int id = AutomationParams::requireInt(params, QStringLiteral("id"), 1, 1000000000);
        branchNode(view, id);
        AutomationDeferredCall::schedule([view, id]() {
            try {
                AutomationWidgets::requireInteractive(view);
                auto* item = branchNode(view, id);
                view->ensureVisible(item);
                const QPointF pos(view->mapFromScene(item->sceneBoundingRect().center()));
                auto* viewport = view->viewport();
                const QPointF global(viewport->mapToGlobal(pos.toPoint()));
                QMouseEvent press(QEvent::MouseButtonPress, pos, global, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, pos, global, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(viewport, &press);
                QApplication::sendEvent(viewport, &release);
            } catch (const AutomationError&) { }
        }, view);
        return QJsonObject{{QStringLiteral("queued"), true}, {QStringLiteral("id"), id}};
    });
}
