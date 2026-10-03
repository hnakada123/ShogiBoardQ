#include "dialogutils.h"
#include "buttonstyles.h"
#include <QAbstractButton>
#include <QDialogButtonBox>
#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QCoreApplication>
#include <QComboBox>
#include <QAbstractItemView>
#include <utility>

namespace DialogUtils {

void restoreDialogSize(QWidget* dialog, const QSize& savedSize)
{
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        dialog->resize(savedSize);
    }
}

void saveDialogSize(const QWidget* dialog, const std::function<void(const QSize&)>& setter)
{
    setter(dialog->size());
}

void applyFontToAllChildren(QWidget* widget, const QFont& font)
{
    widget->setFont(font);
    const auto children = widget->findChildren<QWidget*>();
    for (QWidget* child : std::as_const(children)) {
        if (child->window() != widget->window()) continue;
        child->setFont(font);
        if (auto* combo = qobject_cast<QComboBox*>(child)) combo->view()->setFont(font);
    }
}

void updateFontButtons(QWidget* widget, int size, int minimum, int maximum)
{
    const auto buttons = widget->findChildren<QAbstractButton*>();
    for (auto* button : buttons) {
        if (button->window() != widget->window()) continue;
        const QString text = button->text();
        const bool decrease = text == QLatin1String("A-") || text == QStringLiteral("A−");
        if (!decrease && text != QLatin1String("A+")) continue;
        button->setStyleSheet(ButtonStyles::fontButton());
        button->setToolTip(decrease
            ? QCoreApplication::translate("DialogUtils", "文字を小さくする")
            : QCoreApplication::translate("DialogUtils", "文字を大きくする"));
        button->setAccessibleName(button->toolTip());
        button->setFocusPolicy(Qt::StrongFocus);
        if (auto* push = qobject_cast<QPushButton*>(button)) push->setAutoDefault(false);
        const QFontMetrics metrics(button->font());
        button->setFixedWidth(qMax(40, metrics.horizontalAdvance(button->text()) + 20));
        button->setMinimumHeight(qMax(32, metrics.height() + 12));
        button->setEnabled(decrease ? size > minimum : size < maximum);
    }
}

void standardizeDialog(QWidget* dialog)
{
    if (dialog->layout()) {
        dialog->layout()->setContentsMargins(16, 16, 16, 16);
        dialog->layout()->setSpacing(10);
    }
    const auto buttons = dialog->findChildren<QPushButton*>();
    for (auto* button : buttons) {
        if (button->window() != dialog->window()) continue;
        if (button->maximumHeight() >= 32)
            button->setMinimumHeight(qMax(32, button->minimumHeight()));
        const QString style = button->styleSheet();
        if (style.isEmpty() || style == ButtonStyles::secondaryNeutral()
            || style == ButtonStyles::editOperation() || style == ButtonStyles::undoRedo()
            || style == ButtonStyles::fileOperation() || style == ButtonStyles::wideNavigationButton())
            button->setStyleSheet(ButtonStyles::dialogSecondaryAction());
    }
    const auto boxes = dialog->findChildren<QDialogButtonBox*>();
    for (auto* box : boxes) {
        if (box->window() != dialog->window()) continue;
        for (auto* button : box->buttons()) {
            if (box->buttonRole(button) == QDialogButtonBox::AcceptRole)
                button->setStyleSheet(ButtonStyles::primaryAction());
        }
    }
}

void fitWrappedLabels(QWidget* dialog)
{
    // 表示前の仮の幅で折り返しを計算すると、不要な最小高さが残る。
    if (!dialog->isVisible()) return;
    if (dialog->layout()) dialog->layout()->activate();
    const auto labels = dialog->findChildren<QLabel*>();
    for (auto* label : labels) {
        if (label->window() != dialog->window() || !label->wordWrap() || !label->isVisibleTo(dialog)) continue;
        label->setMinimumHeight(qMax(0, label->heightForWidth(label->width())));
    }
    if (dialog->layout()) dialog->layout()->activate();
    dialog->resize(dialog->size().expandedTo(dialog->minimumSizeHint()));
}

} // namespace DialogUtils
