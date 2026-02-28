#include "dialogutils.h"

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
    for (QWidget* child : children) {
        child->setFont(font);
    }
}

} // namespace DialogUtils
