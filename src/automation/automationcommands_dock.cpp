/// @file automationcommands_dock.cpp
/// @brief ドックの配置・表示状態の検査と操作

#include "automationcommands.h"
#include "automationparams.h"
#include "automationwidgets.h"

#include <QAction>
#include <QApplication>
#include <QDockWidget>
#include <QJsonArray>
#include <QMainWindow>
#include <QMouseEvent>
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
    throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("area must be left, right, top or bottom"));
}

QDockWidget* findDock(QMainWindow* window, const QString& name)
{
    const auto matches = window->findChildren<QDockWidget*>(name, Qt::FindDirectChildrenOnly);
    if (matches.size() != 1) {
        throw AutomationError(AutomationErrorCode::NotFound, QStringLiteral("Expected one dock named \"%1\"").arg(name));
    }
    return matches.first();
}

void requireOperation(QMainWindow* window, QDockWidget* dock, const QString& operation,
                      Qt::DockWidgetArea area, QDockWidget* relative)
{
    AutomationWidgets::requireInteractive(window);
    if (!dock || !dock->isEnabled()) {
        throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("Dock is unavailable or disabled"));
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
    throw AutomationError(AutomationErrorCode::InvalidState,
                          QStringLiteral("Dock operation is locked, disallowed or has no visible docked relative"));
}

void dragTitleBar(QDockWidget* dock, const QPoint& destination)
{
    // ドッキング中の Qt タイトルバーへ通常のマウス入力を送る。
    // フローティング後のOSネイティブ装飾はこの操作の対象外。
    const QPoint start = dock->mapToGlobal(QPoint(dock->width() / 2, dock->widget()->geometry().top() / 2));
    const auto send = [dock](QEvent::Type type, const QPoint& global, Qt::MouseButton button, Qt::MouseButtons buttons) {
        QMouseEvent event(type, QPointF(dock->mapFromGlobal(global)), QPointF(global), button, buttons, Qt::NoModifier);
        QApplication::sendEvent(dock, &event);
    };
    send(QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    send(QEvent::MouseMove, start + QPoint(QApplication::startDragDistance() + 10, 0), Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseMove, destination, Qt::NoButton, Qt::LeftButton);
    send(QEvent::MouseButtonRelease, destination, Qt::LeftButton, Qt::NoButton);
}

} // namespace

void AutomationCommands::registerDockCommands(AutomationDispatcher& dispatcher, const AutomationContext& context)
{
    dispatcher.registerMethod(QStringLiteral("dock.list"), [context](const QJsonObject&) {
        QJsonArray docks;
        auto* window = context.mainWindow;
        if (!window) throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("No main window"));
        for (auto* dock : window->findChildren<QDockWidget*>(QString(), Qt::FindDirectChildrenOnly)) {
            QJsonArray tabs;
            for (auto* sibling : window->tabifiedDockWidgets(dock)) tabs.append(sibling->objectName());
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
        if (!window) throw AutomationError(AutomationErrorCode::InvalidState, QStringLiteral("No main window"));
        auto* dock = findDock(window, AutomationParams::requireString(params, QStringLiteral("widget")));
        const QString operation = AutomationParams::requireString(params, QStringLiteral("operation"));
        if (!QStringList{QStringLiteral("show"), QStringLiteral("hide"), QStringLiteral("float"),
                         QStringLiteral("dock"), QStringLiteral("tabify"), QStringLiteral("drag")}.contains(operation)) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Unknown dock operation"));
        }
        Qt::DockWidgetArea area = Qt::NoDockWidgetArea;
        QPointer<QDockWidget> relative;
        if (operation == QLatin1String("dock")) area = parseArea(AutomationParams::requireString(params, QStringLiteral("area")));
        if (operation == QLatin1String("tabify")) {
            relative = findDock(window, AutomationParams::requireString(params, QStringLiteral("relative_to")));
            if (relative == dock) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Cannot tabify a dock with itself"));
        }
        QRect geometry;
        QPoint destination;
        if (operation == QLatin1String("drag")) {
            if (!params.value(QStringLiteral("x")).isDouble() || !params.value(QStringLiteral("y")).isDouble()) {
                throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("drag requires integer x and y"));
            }
            destination = QPoint(AutomationParams::requireInt(params, QStringLiteral("x"), -100000, 100000),
                                 AutomationParams::requireInt(params, QStringLiteral("y"), -100000, 100000));
        }
        if (params.contains(QStringLiteral("geometry"))) {
            if (operation != QLatin1String("float") || !params.value(QStringLiteral("geometry")).isObject()) {
                throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("geometry is only valid for float"));
            }
            const auto obj = params.value(QStringLiteral("geometry")).toObject();
            for (const QString& key : {QStringLiteral("x"), QStringLiteral("y"), QStringLiteral("width"), QStringLiteral("height")}) {
                if (!obj.value(key).isDouble()) throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("geometry requires integer coordinates and dimensions"));
            }
            geometry = QRect(AutomationParams::requireInt(obj, QStringLiteral("x"), -100000, 100000),
                             AutomationParams::requireInt(obj, QStringLiteral("y"), -100000, 100000),
                             AutomationParams::requireInt(obj, QStringLiteral("width"), 1, 16384),
                             AutomationParams::requireInt(obj, QStringLiteral("height"), 1, 16384));
        }
        if ((params.contains(QStringLiteral("area")) && operation != QLatin1String("dock"))
            || (params.contains(QStringLiteral("relative_to")) && operation != QLatin1String("tabify"))
            || ((params.contains(QStringLiteral("x")) || params.contains(QStringLiteral("y"))) && operation != QLatin1String("drag"))) {
            throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("Unexpected parameter for dock operation"));
        }
        requireOperation(window, dock, operation, area, relative);
        AutomationDeferredCall::schedule([window, dock, operation, area, relative, geometry, destination]() {
            try {
                requireOperation(window, dock, operation, area, relative);
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
            } catch (const AutomationError&) { }
        }, dock);
        return QJsonObject{{QStringLiteral("queued"), true}, {QStringLiteral("object_name"), dock->objectName()}};
    });
}
