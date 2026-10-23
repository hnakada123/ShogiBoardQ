/// @file apptooltipfilter.cpp
/// @brief アプリケーションツールチップフィルタクラスの実装

#include "apptooltipfilter.h"
#include "globaltooltip.h"

#include <QEvent>
#include <QHelpEvent>
#include <QWidget>

AppToolTipFilter::AppToolTipFilter(QWidget* parent)
    : QObject(parent),
    m_tip(new GlobalToolTip(parent))
{
}

void AppToolTipFilter::setPointSizeF(qreal pt) {
    if (m_tip) m_tip->setPointSizeF(pt);
}

bool AppToolTipFilter::eventFilter(QObject* obj, QEvent* ev) {
    // ツールチップ表示
    if (ev->type() == QEvent::ToolTip) {
        auto* w  = qobject_cast<QWidget*>(obj);
        auto* he = static_cast<QHelpEvent*>(ev);
        if (!w || !he) return QObject::eventFilter(obj, ev);

        const QString text = w->toolTip().trimmed();
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
