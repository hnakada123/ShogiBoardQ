/// @file automationcommands_branch.cpp
/// @brief 自動化 API の branch.* メソッド（分岐ツリーの手のクリック）の実装

#include "automationcommands.h"
#include "automationdispatcher.h"
#include "automationinput.h"
#include "automationinteractions.h"
#include "automationwidgets.h"
#include "branchtreeitemroles.h"

#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QGraphicsView>

namespace {

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

void AutomationCommands::registerBranchCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
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
