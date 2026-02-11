/// @file aboutcoordinator.cpp
/// @brief バージョン情報コーディネータクラスの実装

#include <QDesktopServices>
#include <QUrl>
#include "aboutcoordinator.h"
#include "versiondialog.h"

namespace AboutCoordinator {

void showVersionDialog(QWidget* parent)
{
    VersionDialog dlg(parent);
    dlg.exec();
}

void openProjectWebsite()
{
    QDesktopServices::openUrl(QUrl("https://hnakada123.github.io/ShogiBoardQ/"));
}

} // namespace AboutCoordinator
