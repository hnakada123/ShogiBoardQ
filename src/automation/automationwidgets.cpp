/// @file automationwidgets.cpp
/// @brief 自動化 API のウィンドウ列挙・ウィジェット内容取得ヘルパの実装

#include "automationwidgets.h"
#include "automationcontext.h"
#include "automationdispatcher.h"
#include "automationparams.h"
#include "automationactionpolicy.h"
#include "automationwidgetadapters.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QAbstractItemView>
#include <QCompleter>
#include <QDialog>
#include <QLineEdit>
#include <QMainWindow>
#include <QToolButton>

namespace {

QString widgetActionName(QWidget* widget)
{
    if (auto* button = qobject_cast<QToolButton*>(widget); button && button->defaultAction())
        return button->defaultAction()->objectName();
    return widget->property("automationAction").toString();
}

} // namespace

QList<QWidget*> AutomationWidgets::visibleWindows()
{
    QList<QWidget*> result;
    const QWidgetList windows = QApplication::topLevelWidgets();
    for (QWidget* w : windows) {
        if (!w->isVisible() || w->isHidden()) continue;
        if (qobject_cast<QMainWindow*>(w) || qobject_cast<QDialog*>(w) || w->isWindow()) {
            // ツールチップやメニューのポップアップは除外する
            if (w->windowType() == Qt::ToolTip || w->windowType() == Qt::Popup) continue;
            result.append(w);
        }
    }
    return result;
}

QWidget* AutomationWidgets::findWindow(const QString& nameOrTitle)
{
    const QList<QWidget*> windows = visibleWindows();
    for (QWidget* w : windows) {
        if (w->objectName() == nameOrTitle || selector(w) == nameOrTitle) return w;
    }
    for (QWidget* w : windows) {
        if (!nameOrTitle.isEmpty() && w->windowTitle().contains(nameOrTitle, Qt::CaseInsensitive)) return w;
    }
    return nullptr;
}

QJsonObject AutomationWidgets::describeWindow(QWidget* window, bool isMain)
{
    QJsonObject obj;
    obj[QStringLiteral("object_name")] = window->objectName();
    obj[QStringLiteral("selector")] = selector(window);
    obj[QStringLiteral("class")] = QString::fromLatin1(window->metaObject()->className());
    obj[QStringLiteral("title")] = window->windowTitle();
    obj[QStringLiteral("visible")] = window->isVisible();
    obj[QStringLiteral("modal")] = window->isModal();
    obj[QStringLiteral("active")] = QApplication::activeWindow() == window;
    obj[QStringLiteral("main")] = isMain;
    obj[QStringLiteral("width")] = window->width();
    obj[QStringLiteral("height")] = window->height();
    return obj;
}

QJsonArray AutomationWidgets::describeWidgets(QWidget* root, const QString& objectName, int maxRows, int maxWidgets,
                                             bool includeChildren)
{
    QJsonArray array;
    if (!root) return array;
    QList<QWidget*> targets;
    if (!objectName.isEmpty()) {
        if (auto* child = findWidget(root, objectName)) {
            targets.append(child);
            if (includeChildren) targets.append(child->findChildren<QWidget*>());
        }
    } else {
        targets = root->findChildren<QWidget*>();
    }
    for (QWidget* w : std::as_const(targets)) {
        if (array.size() >= maxWidgets) break;
        if (objectName.isEmpty() && !w->isVisible()) continue;
        QJsonObject obj;
        if (!AutomationWidgetAdapters::describe(w, maxRows, obj)) continue;
        obj[QStringLiteral("object_name")] = w->objectName();
        obj[QStringLiteral("selector")] = selector(w);
        obj[QStringLiteral("parent_selector")] = w->parent() ? selector(w->parent()) : QString();
        obj[QStringLiteral("tooltip")] = w->toolTip();
        obj[QStringLiteral("accessible_name")] = w->accessibleName();
        const QString actionName = widgetActionName(w);
        if (!actionName.isEmpty()) obj[QStringLiteral("action_name")] = actionName;
        obj[QStringLiteral("class")] = QString::fromLatin1(w->metaObject()->className());
        obj[QStringLiteral("enabled")] = w->isEnabled();
        obj[QStringLiteral("font_point_size")] = w->font().pointSize();
        const QPoint pos = w->mapTo(root, QPoint());
        obj[QStringLiteral("geometry")] = QJsonObject{{QStringLiteral("x"), pos.x()}, {QStringLiteral("y"), pos.y()},
                                                      {QStringLiteral("width"), w->width()}, {QStringLiteral("height"), w->height()}};
        if (!w->isVisible()) obj[QStringLiteral("visible")] = false;
        array.append(obj);
    }
    return array;
}

namespace AutomationWidgets {

QString selector(QObject* object)
{
    static quint64 nextId = 0; // 自動化APIはメインスレッドでのみ呼ばれる。
    constexpr auto key = "_automationSelector";
    QString id = object->property(key).toString();
    if (id.isEmpty()) {
        id = QStringLiteral("@widget-%1").arg(++nextId);
        object->setProperty(key, id);
    }
    return id;
}

QWidget* findWidget(QWidget* root, const QString& nameOrSelector)
{
    if (!root) return nullptr;
    QList<QWidget*> candidates = root->findChildren<QWidget*>();
    candidates.prepend(root);
    QWidget* found = nullptr;
    for (auto* widget : std::as_const(candidates)) {
        if (widget->objectName() != nameOrSelector && selector(widget) != nameOrSelector) continue;
        if (found) throw AutomationError(ErrorKind::InvalidArgument,
            QStringLiteral("Ambiguous widget name; use its selector from widget.text"));
        found = widget;
    }
    return found;
}

QWidget* requireWidget(const AutomationContext& context, const QJsonObject& params, bool interactive)
{
    QWidget* window = requireWindow(context, params.value(QStringLiteral("target")).toString());
    const QString name = AutomationParams::requireString(params, QStringLiteral("widget"));
    QWidget* widget = findWidget(window, name);
    if (!widget || (interactive && widget->window() != window))
        throw AutomationError(ErrorKind::NotFound, QStringLiteral("No widget in target window: %1").arg(name));
    if (interactive) requireInteractive(widget);
    return widget;
}

QWidget* requireWindow(const AutomationContext& context, const QString& target)
{
    if (target.isEmpty() || target == QLatin1String("main")) return context.mainWindow;
    QWidget* window = AutomationWidgets::findWindow(target);
    if (!window) {
        throw AutomationError(ErrorKind::NotFound, QStringLiteral("No open window matches \"%1\"").arg(target),
                              QStringLiteral("Use dialog.list to see the open windows"));
    }
    return window;
}

bool isSensitive(const QWidget* widget)
{
    if (!widget) return false;
    if (widget->property("automationSensitive").toBool()) return true;
    const auto* edit = qobject_cast<const QLineEdit*>(widget);
    return edit && edit->echoMode() != QLineEdit::Normal;
}

void requireInteractive(QWidget* widget, bool allowPopup, bool allowSensitiveWrite)
{
    if (isSensitive(widget) && !(allowSensitiveWrite && qobject_cast<QLineEdit*>(widget)))
        throw AutomationError(ErrorKind::NotAllowed, QStringLiteral("Sensitive controls are not accessible through automation"));
    if (widget) {
        const QString action = widgetActionName(widget);
        if (!action.isEmpty() && !AutomationActionPolicy::isAllowed(action))
            throw AutomationError(ErrorKind::NotAllowed, QStringLiteral("Menu button action is not allowed"));
    }
    QWidget* modal = QApplication::activeModalWidget();
    bool blocked = modal != nullptr;
    for (QWidget* parent = widget; parent; parent = parent->parentWidget()) {
        if (parent == modal) { blocked = false; break; }
    }
    if (!widget || !widget->isVisible() || !widget->isEnabled() || blocked) {
        throw AutomationError(ErrorKind::InvalidState,
                              QStringLiteral("The target is hidden, disabled or blocked by another dialog"));
    }
    // QFileDialogの補完はディレクトリ読込後にも開く。同じウィンドウ内の操作なら、
    // 通常のクリックと同じようにその入力欄の候補だけを閉じる。
    if (!allowPopup && QApplication::activePopupWidget()) {
        const auto edits = widget->window()->findChildren<QLineEdit*>();
        for (auto* edit : edits) {
            if (edit->window() == widget->window() && edit->completer()
                && edit->completer()->popup() == QApplication::activePopupWidget()) {
                edit->completer()->popup()->hide();
                break;
            }
        }
        if (QApplication::activePopupWidget())
            throw AutomationError(ErrorKind::InvalidState, QStringLiteral("Another popup is active"));
    }
}

} // namespace AutomationWidgets
