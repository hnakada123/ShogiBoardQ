/// @file fontsizecontrol.cpp
/// @brief A-/A+ のボタンの組の実装

#include "fontsizecontrol.h"

#include <QFontMetrics>
#include <QPushButton>
#include <QToolButton>

#include "buttonstyles.h"
#include "fontsizehelper.h"
#include "panelbutton.h"

FontSizeControl::FontSizeControl(QAbstractButton* decrease, QAbstractButton* increase, FontSizeHelper* helper,
                                 Sizing sizing)
    : QObject(helper)
    , m_decrease(decrease)
    , m_increase(increase)
    , m_helper(helper)
    , m_sizing(sizing)
{
    Q_ASSERT(decrease && increase && helper);
    const struct {
        QAbstractButton* button;
        const char* text;
        const char* role;
        QString toolTip;
    } buttons[] = {
        {decrease, "A-", kDecreaseRole, tr("文字を小さくする")},
        {increase, "A+", kIncreaseRole, tr("文字を大きくする")},
    };
    for (const auto& b : buttons) {
        b.button->setText(QString::fromLatin1(b.text));
        b.button->setProperty(kRoleProperty, QString::fromLatin1(b.role));
        if (sizing == Sizing::Panel) PanelButton::setup(b.button, ButtonStyles::fontButton());
        else b.button->setStyleSheet(ButtonStyles::fontButton());
        b.button->setToolTip(b.toolTip);
        b.button->setAccessibleName(b.toolTip);
        if (auto* push = qobject_cast<QPushButton*>(b.button)) push->setAutoDefault(false);
        // ダイアログでは Tab で選べるようにする（ドックのツールバーは呼び出し側が決める）
        if (sizing == Sizing::Dialog) b.button->setFocusPolicy(Qt::StrongFocus);
    }
    connect(decrease, &QAbstractButton::clicked, helper, &FontSizeHelper::decrease);
    connect(increase, &QAbstractButton::clicked, helper, &FontSizeHelper::increase);
    connect(helper, &FontSizeHelper::fontSizeChanged, this, &FontSizeControl::refresh);
    refresh();
}

FontSizeControl* FontSizeControl::createToolButtons(QWidget* parent, FontSizeHelper* helper,
                                                    const QString& decreaseName, const QString& increaseName)
{
    auto* decrease = new QToolButton(parent);
    auto* increase = new QToolButton(parent);
    decrease->setObjectName(decreaseName);
    increase->setObjectName(increaseName);
    return new FontSizeControl(decrease, increase, helper);
}

void FontSizeControl::refresh()
{
    const int size = m_helper->fontSize();
    if (m_decrease) m_decrease->setEnabled(size > m_helper->minSize());
    if (m_increase) m_increase->setEnabled(size < m_helper->maxSize());
    if (m_sizing != Sizing::Dialog) return;
    for (QAbstractButton* button : {m_decrease.data(), m_increase.data()}) {
        if (!button) continue;
        const QFontMetrics metrics(button->font());
        button->setFixedWidth(qMax(40, metrics.horizontalAdvance(button->text()) + 20));
        button->setMinimumHeight(qMax(32, metrics.height() + 12));
    }
}
