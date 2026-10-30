/// @file panelbutton.cpp
/// @brief ドックのパネルの小さなボタンの見た目と大きさの実装

#include "panelbutton.h"

#include "buttonstyles.h"

#include <QAbstractButton>
#include <QEvent>
#include <QFontMetrics>
#include <QToolButton>

namespace PanelButton {

namespace {

/// 書体が変わったボタン（検討タブは A-/A+ に表の文字の大きさを付ける）の大きさを測り直す
class Refitter : public QObject
{
public:
    using QObject::QObject;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::FontChange) {
            auto* button = static_cast<QAbstractButton*>(watched);
            button->setFixedSize(sizeFor(button));
        }
        return QObject::eventFilter(watched, event);
    }
};

/// 文字を出すボタンか（アイコンだけの QToolButton は、QAction から付いた文字を出さない）
bool showsText(const QAbstractButton* button)
{
    if (button->text().isEmpty()) return false;
    const auto* tool = qobject_cast<const QToolButton*>(button);
    return !tool || tool->icon().isNull() || tool->toolButtonStyle() != Qt::ToolButtonIconOnly;
}

} // namespace

QSize sizeFor(const QAbstractButton* button)
{
    if (!showsText(button)) return kSize;
    const QFontMetrics metrics(button->font());
    return {qMax(kSize.width(), metrics.horizontalAdvance(button->text()) + 16),
            qMax(kSize.height(), metrics.height() + 6)};
}

void setup(QAbstractButton* button, const QString& styleSheet)
{
    button->setStyleSheet(styleSheet.isEmpty() ? ButtonStyles::panelToolButton() : styleSheet);
    button->setIconSize(kIconSize);
    button->setFixedSize(sizeFor(button));
    // 2回整えても、測り直しは1つだけ
    static constexpr char kInstalled[] = "panelButtonRefitter";
    if (button->property(kInstalled).toBool()) return;
    button->setProperty(kInstalled, true);
    button->installEventFilter(new Refitter(button));
}

} // namespace PanelButton
