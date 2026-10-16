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

void requireOperation(QMainWindow* window, QDockWidget* dock, const QString& operation,
                      Qt::DockWidgetArea area, QDockWidget* relative)
{
    AutomationWidgets::requireInteractive(window);
    if (!dock || !dock->isEnabled()) {
        throw AutomationError(ErrorKind::InvalidState, QStringLiteral("Dock is unavailable or disabled"));
    }
    if (operation == QLatin1String("show")) return;
    if (operation == QLatin1String("hide")) {
        if (dock->features().testFlag(QDockWidget::DockWidgetClosable)) return;
    } else if (dock->features().testFlag(QDockWidget::DockWidgetMovable)) {
        if (operation == QLatin1String("drag") && !dock->isFloating() && dock->widget()
            && !dock->visibleRegion().isEmpty() && dock->features().testFlag(QDockWidget::DockWidgetFloatable)) return;
        if (operation == QLatin1String("float") && dock->features().testFlag(QDockWidget::DockWidgetFloatable)) return;
        if (operation == QLatin1String("dock") && dock->isAreaAllowed(area)) return;
        if (operation == QLatin1String("tabify") && relative && relative->isEnabled()
            && !relative->isFloating() && !relative->isHidden()
            && relative->features().testFlag(QDockWidget::DockWidgetMovable)
            && dock->isAreaAllowed(window->dockWidgetArea(relative))) return;
    }
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
        const QString operation = params.value(QStringLiteral("operation")).toString();
        Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
        QPointer<QDockWidget> relative;
        if (operation == QLatin1String("dock")) area = parseArea(AutomationParams::requireString(params, QStringLiteral("area")));
        if (operation == QLatin1String("tabify")) {
            relative = findDock(window, AutomationParams::requireString(params, QStringLiteral("relative_to")));
            if (relative == dock) throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Cannot tabify a dock with itself"));
        }
        QRect geometry;
        QPoint destination;
        if (operation == QLatin1String("drag")) {
            if (!params.contains(QStringLiteral("x")) || !params.contains(QStringLiteral("y"))) {
                throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("drag requires integer x and y"));
            }
            destination = QPoint(params.value(QStringLiteral("x")).toInt(), params.value(QStringLiteral("y")).toInt());
        }
        if (params.contains(QStringLiteral("geometry"))) {
            if (operation != QLatin1String("float")) {
                throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("geometry is only valid for float"));
            }
            const auto obj = params.value(QStringLiteral("geometry")).toObject();
            geometry = QRect(obj.value(QStringLiteral("x")).toInt(), obj.value(QStringLiteral("y")).toInt(),
                             obj.value(QStringLiteral("width")).toInt(), obj.value(QStringLiteral("height")).toInt());
        }
        if ((params.contains(QStringLiteral("area")) && operation != QLatin1String("dock"))
            || (params.contains(QStringLiteral("relative_to")) && operation != QLatin1String("tabify"))
            || ((params.contains(QStringLiteral("x")) || params.contains(QStringLiteral("y"))) && operation != QLatin1String("drag"))) {
            throw AutomationError(ErrorKind::InvalidArgument, QStringLiteral("Unexpected parameter for dock operation"));
        }
        requireOperation(window, dock, operation, area, relative);
        return AutomationInteractions::queue(
            {dock, [window, dock, operation, area, relative]() { requireOperation(window, dock, operation, area, relative); },
             [window, dock, operation, area, relative, geometry, destination]() {
                if (operation == QLatin1String("hide")) { dock->close(); return; }
                if (operation == QLatin1String("drag")) { dragTitleBar(dock, destination); return; }
                if (operation == QLatin1String("float")) {
                    dock->setFloating(true);
                    if (geometry.isValid()) dock->setGeometry(geometry);
                } else if (operation == QLatin1String("dock")) {
                    dock->setFloating(false);
                    window->addDockWidget(area, dock);
                } else if (operation == QLatin1String("tabify")) {
                    dock->setFloating(false);
                    window->tabifyDockWidget(relative, dock);
                }
                dock->show();
                dock->raise();
            }},
            QJsonObject{{QStringLiteral("object_name"), dock->objectName()}});
    });
}
