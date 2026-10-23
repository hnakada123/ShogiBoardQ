/// @file automationcommands_dock.cpp
/// @brief ドックの配置・表示状態の検査と操作

#include "automationcommands.h"
#include "automationinput.h"
#include "automationinteractions.h"
#include "automationparams.h"
#include "automationwidgets.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QHash>
#include <QJsonArray>
#include <QMainWindow>
#include <QPointer>

namespace {

QString areaName(Qt::DockWidgetArea area)
{
    switch (area) {
    case Qt::LeftDockWidgetArea: return QStringLiteral("left");
    case Qt::RightDockWidgetArea: return QStringLiteral("right");
    case Qt::TopDockWidgetArea: return QStringLiteral("top");
    case Qt::BottomDockWidgetArea: return QStringLiteral("bottom");
    default: return QStringLiteral("none");
    }
}

Qt::DockWidgetArea parseArea(const QString& name)
{
    for (auto area : {Qt::LeftDockWidgetArea, Qt::RightDockWidgetArea,
                      Qt::TopDockWidgetArea, Qt::BottomDockWidgetArea}) {
        if (areaName(area) == name) return area;
    }
    throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("area must be left, right, top or bottom"));
}

QDockWidget* findDock(QMainWindow* window, const QString& name)
{
    const auto matches = window->findChildren<QDockWidget*>(name, Qt::FindDirectChildrenOnly);
    if (matches.size() != 1) {
        throw AutomationError(ErrorKind::NotFound, QStringLiteral("Expected one dock named \"%1\"").arg(name));
    }
    return matches.first();
}

/// ドックの操作（引数の operation。名前は表 AutomationMethodSpecs の選択肢で検査済み）
enum class DockOperation { Show, Hide, Float, Dock, Tabify, Drag };

DockOperation operationFrom(const QString& name)
{
    static const QHash<QString, DockOperation> operations{
        {QStringLiteral("show"), DockOperation::Show},   {QStringLiteral("hide"), DockOperation::Hide},
        {QStringLiteral("float"), DockOperation::Float}, {QStringLiteral("dock"), DockOperation::Dock},
        {QStringLiteral("tabify"), DockOperation::Tabify}, {QStringLiteral("drag"), DockOperation::Drag}};
    return operations.value(name, DockOperation::Show);
}

bool canOperate(QMainWindow* window, QDockWidget* dock, DockOperation operation, Qt::DockWidgetArea area,
                QDockWidget* relative)
{
    const QDockWidget::DockWidgetFeatures features = dock->features();
    const bool movable = features.testFlag(QDockWidget::DockWidgetMovable);
    switch (operation) {
    case DockOperation::Show:
        return true;
    case DockOperation::Hide:
        return features.testFlag(QDockWidget::DockWidgetClosable);
    case DockOperation::Drag:
        return movable && !dock->isFloating() && dock->widget() && !dock->visibleRegion().isEmpty()
            && features.testFlag(QDockWidget::DockWidgetFloatable);
    case DockOperation::Float:
        return movable && features.testFlag(QDockWidget::DockWidgetFloatable);
    case DockOperation::Dock:
        return movable && dock->isAreaAllowed(area);
    case DockOperation::Tabify:
        return movable && relative && relative->isEnabled() && !relative->isFloating() && !relative->isHidden()
            && relative->features().testFlag(QDockWidget::DockWidgetMovable)
            && dock->isAreaAllowed(window->dockWidgetArea(relative));
    }
    return false;
}

void requireOperation(QMainWindow* window, QDockWidget* dock, DockOperation operation, Qt::DockWidgetArea area,
                      QDockWidget* relative)
{
    AutomationWidgets::requireInteractive(window);
    if (!dock || !dock->isEnabled()) {
        throw AutomationError(ErrorKind::InvalidState, QStringLiteral("Dock is unavailable or disabled"));
    }
    if (!canOperate(window, dock, operation, area, relative))
        throw AutomationError(ErrorKind::InvalidState,
                              QStringLiteral("Dock operation is locked, disallowed or has no visible docked relative"));
}

void dragTitleBar(QDockWidget* dock, const QPoint& destination)
{
    // ドッキング中の Qt タイトルバーへ通常のマウス入力を送る。
    // フローティング後のOSネイティブ装飾はこの操作の対象外。
    const QPoint start = dock->mapToGlobal(QPoint(dock->width() / 2, dock->widget()->geometry().top() / 2));
    AutomationInput::sendDrag(dock, {start, start + QPoint(QApplication::startDragDistance() + 10, 0), destination});
}

} // namespace

void AutomationCommands::registerDockCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("dock.list"), [context](const QJsonObject&) {
        QJsonArray docks;
        auto* window = context.mainWindow;
        if (!window) throw AutomationError(ErrorKind::InvalidState, QStringLiteral("No main window"));
        const auto dockWidgets = window->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly);
        for (auto* dock : dockWidgets) {
            QJsonArray tabs;
            const auto tabified = window->tabifiedDockWidgets(dock);
            for (auto* sibling : tabified) tabs.append(sibling->objectName());
            QJsonArray areas;
            for (auto area : {Qt::LeftDockWidgetArea, Qt::RightDockWidgetArea,
                              Qt::TopDockWidgetArea, Qt::BottomDockWidgetArea}) {
                if (dock->isAreaAllowed(area)) areas.append(areaName(area));
            }
            const QPoint pos = dock->mapToGlobal(QPoint());
            const auto features = dock->features();
            docks.append(QJsonObject{
                {QStringLiteral("object_name"), dock->objectName()},
                {QStringLiteral("title"), dock->windowTitle()},
                {QStringLiteral("visible"), dock->isVisible()},
                {QStringLiteral("hidden"), dock->isHidden()},
                {QStringLiteral("exposed"), !dock->visibleRegion().isEmpty()},
                {QStringLiteral("floating"), dock->isFloating()},
                {QStringLiteral("area"), areaName(window->dockWidgetArea(dock))},
                {QStringLiteral("tabified_with"), tabs},
                {QStringLiteral("allowed_areas"), areas},
                {QStringLiteral("closable"), features.testFlag(QDockWidget::DockWidgetClosable)},
                {QStringLiteral("movable"), features.testFlag(QDockWidget::DockWidgetMovable)},
                {QStringLiteral("floatable"), features.testFlag(QDockWidget::DockWidgetFloatable)},
                {QStringLiteral("toggle_checked"), dock->toggleViewAction()->isChecked()},
                {QStringLiteral("geometry"), QJsonObject{{QStringLiteral("x"), pos.x()}, {QStringLiteral("y"), pos.y()},
                    {QStringLiteral("width"), dock->width()}, {QStringLiteral("height"), dock->height()}}}});
        }
        return QJsonObject{{QStringLiteral("docks"), docks}};
    });

    dispatcher.registerMethod(QStringLiteral("dock.configure"), [context](const QJsonObject& params) {
        auto* window = context.mainWindow;
        if (!window) throw AutomationError(ErrorKind::InvalidState, QStringLiteral("No main window"));
        // 型と範囲は AutomationMethodSpecs の表で検査済み。ここでは操作ごとの組み合わせだけを確かめる
        auto* dock = findDock(window, params.value(QStringLiteral("widget")).toString());
        const DockOperation operation = operationFrom(params.value(QStringLiteral("operation")).toString());
        Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
        QPointer<QDockWidget> relative;
        if (operation == DockOperation::Dock) area = parseArea(AutomationParams::requireString(params, QStringLiteral("area")));
        if (operation == DockOperation::Tabify) {
            relative = findDock(window, AutomationParams::requireString(params, QStringLiteral("relative_to")));
            if (relative == dock) throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Cannot tabify a dock with itself"));
        }
        QRect geometry;
        QPoint destination;
        if (operation == DockOperation::Drag) {
            if (!params.contains(QStringLiteral("x")) || !params.contains(QStringLiteral("y"))) {
                throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("drag requires integer x and y"));
            }
            destination = QPoint(params.value(QStringLiteral("x")).toInt(), params.value(QStringLiteral("y")).toInt());
        }
        if (params.contains(QStringLiteral("geometry"))) {
            if (operation != DockOperation::Float) {
                throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("geometry is only valid for float"));
            }
            const auto obj = params.value(QStringLiteral("geometry")).toObject();
            geometry = QRect(obj.value(QStringLiteral("x")).toInt(), obj.value(QStringLiteral("y")).toInt(),
                             obj.value(QStringLiteral("width")).toInt(), obj.value(QStringLiteral("height")).toInt());
        }
        if ((params.contains(QStringLiteral("area")) && operation != DockOperation::Dock)
            || (params.contains(QStringLiteral("relative_to")) && operation != DockOperation::Tabify)
            || ((params.contains(QStringLiteral("x")) || params.contains(QStringLiteral("y"))) && operation != DockOperation::Drag)) {
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Unexpected parameter for dock operation"));
        }
        return AutomationInteractions::queue(
            {dock, [window, dock, operation, area, relative]() { requireOperation(window, dock, operation, area, relative); },
             [window, dock, operation, area, relative, geometry, destination]() {
                switch (operation) {
                case DockOperation::Hide: dock->close(); return;
                case DockOperation::Drag: dragTitleBar(dock, destination); return;
                case DockOperation::Float:
                    dock->setFloating(true);
                    if (geometry.isValid()) dock->setGeometry(geometry);
                    break;
                case DockOperation::Dock:
                    dock->setFloating(false);
                    window->addDockWidget(area, dock);
                    break;
                case DockOperation::Tabify:
                    dock->setFloating(false);
                    window->tabifyDockWidget(relative, dock);
                    break;
                case DockOperation::Show:
                    break;
                }
                dock->show();
                dock->raise();
            }},
            QJsonObject{{QStringLiteral("object_name"), dock->objectName()}});
    });
}
