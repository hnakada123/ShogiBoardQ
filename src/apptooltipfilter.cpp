#include "apptooltipfilter.h"
#include "globaltooltip.h"

#include <QEvent>
#include <QHelpEvent>
#include <QWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QAction>

AppToolTipFilter::AppToolTipFilter(QWidget* parent)
    : QObject(parent),
    m_tip(new GlobalToolTip(parent))
{
}

void AppToolTipFilter::setPointSizeF(qreal pt) {
    if (m_tip) m_tip->setPointSizeF(pt);
}

void AppToolTipFilter::installOn(QWidget* root) {
    if (!root) return;

    // ルート自身
    root->installEventFilter(this);

    // 既存の子ウィジェットへ一括装着
    const auto menuBars = root->findChildren<QMenuBar*>();
    for (QMenuBar* mb : std::as_const(menuBars))
        installOnMenuBar(mb);

    const auto toolBars = root->findChildren<QToolBar*>();
    for (QToolBar* tb : std::as_const(toolBars))
        installOnToolBar(tb);

    const auto menus = root->findChildren<QMenu*>();
    for (QMenu* menu : std::as_const(menus))
        installOnMenu(menu);

    const auto buttons = root->findChildren<QToolButton*>();
    for (QToolButton* btn : std::as_const(buttons))
        btn->installEventFilter(this);
}

void AppToolTipFilter::installOnMenuBar(QMenuBar* mb) {
    if (!mb) return;
    mb->installEventFilter(this);
}

void AppToolTipFilter::installOnToolBar(QToolBar* tb) {
    if (!tb) return;

    tb->installEventFilter(this);

    // 既存のツールボタンにも（detach回避：const 反復）
    const auto buttons = tb->findChildren<QToolButton*>();
    for (QToolButton* btn : std::as_const(buttons))
        btn->installEventFilter(this);
}

void AppToolTipFilter::installOnMenu(QMenu* menu) {
    if (!menu) return;
    menu->installEventFilter(this);
}

QString AppToolTipFilter::pickTextForWidget(QWidget* w, const QPoint& localPos) const {
    if (!w) return {};

    // AppToolTipFilter.cpp の QMenuBar 分岐
    if (auto* mb = qobject_cast<QMenuBar*>(w)) {
        if (QAction* act = mb->actionAt(localPos)) {
            // アイコンが無い項目はツールチップを出さない
            if (act->icon().isNull())
                return QString();  // 空を返すと表示しない

            const QString t = act->toolTip().trimmed();
            return t.isEmpty() ? act->text().trimmed() : t;
        }
    }

    return w->toolTip().trimmed();
}

bool AppToolTipFilter::eventFilter(QObject* obj, QEvent* ev) {
    // ツールチップ表示
    if (ev->type() == QEvent::ToolTip) {
        auto* w  = qobject_cast<QWidget*>(obj);
        auto* he = static_cast<QHelpEvent*>(ev);
        if (!w || !he) return QObject::eventFilter(obj, ev);

        const QString text = pickTextForWidget(w, he->pos());
        if (!text.isEmpty()) {
            m_tip->showText(he->globalPos(), text);
            return true;                 // 既定の QToolTip を抑止
        } else {
            m_tip->hideTip();
        }
    }
    // 非表示トリガ
    else if (ev->type() == QEvent::Leave      ||
             ev->type() == QEvent::Hide       ||
             ev->type() == QEvent::Close      ||
             ev->type() == QEvent::FocusOut   ||
             ev->type() == QEvent::WindowDeactivate ||
             ev->type() == QEvent::MouseButtonPress)
    {
        if (m_tip) m_tip->hideTip();
    }

    return QObject::eventFilter(obj, ev);
}

void AppToolTipFilter::setCompact(bool on) {
    if (m_tip) m_tip->setCompact(on);
}
