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

} // namespace DialogUtils
