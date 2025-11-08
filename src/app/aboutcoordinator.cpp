#include <QDesktopServices>
#include <QUrl>
#include "versiondialog.h"

namespace AboutCoordinator {

void showVersionDialog(QWidget* parent)
{
    VersionDialog dlg(parent);
    dlg.exec();
}

void openProjectWebsite()
{
    QDesktopServices::openUrl(QUrl("https://github.com/hnakada123/ShogiBoardQ"));
}

} // namespace AboutCoordinator
