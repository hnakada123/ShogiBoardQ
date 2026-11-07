#include "dialogutils.h"
#include "buttonstyles.h"
#include <QAbstractButton>
#include <QDialog>
#include <QDialogButtonBox>
#include <QEvent>
#include <QLayout>
#include <QLabel>
#include <QPushButton>
#include <QCoreApplication>
#include <QComboBox>
#include <QMenu>
#include <QAbstractItemView>
#include <utility>

namespace DialogUtils {

void restoreDialogSize(QWidget* dialog, const QSize& savedSize)
{
    if (savedSize.isValid() && savedSize.width() > 100 && savedSize.height() > 100) {
        dialog->resize(savedSize);
    }
}

namespace {
/// 閉じたときに大きさを保存する（ダイアログの子。設定の宣言は実行中ずっとあるので参照で持つ）
class DialogSizeKeeper final : public QObject
{
public:
    DialogSizeKeeper(QDialog* dialog, const Setting<QSize>& setting) : QObject(dialog), m_setting(setting)
    {
        dialog->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (event->type() == QEvent::Hide) m_setting.set(static_cast<QWidget*>(watched)->size());
        return QObject::eventFilter(watched, event);
    }

private:
    const Setting<QSize>& m_setting;
};
} // namespace

void rememberDialogSize(QDialog* dialog, const Setting<QSize>& setting)
{
    restoreDialogSize(dialog, setting);
    new DialogSizeKeeper(dialog, setting);
}

void saveDialogSize(const QWidget* dialog, const std::function<void(const QSize&)>& setter)
{
    setter(dialog->size());
}

namespace {
bool isExcluded(const QWidget* child, const QList<QWidget*>& excluded)
{
    for (const QWidget* root : excluded) {
        if (root && (child == root || root->isAncestorOf(child))) return true;
    }
    return false;
}
} // namespace

void applyFontToAllChildren(QWidget* widget, const QFont& font, const QList<QWidget*>& excluded)
{
    widget->setFont(font);
    const auto children = widget->findChildren<QWidget*>();
    for (QWidget* child : std::as_const(children)) {
        // 別ウィンドウ（子ダイアログなど）には伝えない。ダイアログのボタンから開くメニューは同じ文字で出す
        const QWidget* owner = qobject_cast<QMenu*>(child) ? child->parentWidget() : child;
        if (!owner || owner->window() != widget->window()) continue;
        if (isExcluded(child, excluded)) continue;
        child->setFont(font);
        if (auto* combo = qobject_cast<QComboBox*>(child)) combo->view()->setFont(font);
    }
}

QFont applyFontSize(QWidget* widget, int size, const QList<QWidget*>& excluded)
{
    standardizeDialog(widget);
    QFont font = widget->font();
    font.setPointSize(size);
    applyFontToAllChildren(widget, font, excluded);
    return font;
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
        const auto boxButtons = box->buttons();
        for (auto* button : boxButtons) {
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

bool confirmAction(QWidget* parent, const QString& title, const QString& text,
                   const QString& actionText, QMessageBox::Icon icon)
{
    QMessageBox box(icon, title, text, QMessageBox::Yes | QMessageBox::Cancel, parent);
    box.button(QMessageBox::Yes)->setText(actionText);
    box.setDefaultButton(QMessageBox::Cancel);
    box.setEscapeButton(QMessageBox::Cancel);
    return box.exec() == QMessageBox::Yes;
}

} // namespace DialogUtils
