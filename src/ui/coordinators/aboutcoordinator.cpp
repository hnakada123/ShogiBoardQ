/// @file aboutcoordinator.cpp
/// @brief バージョン情報コーディネータクラスの実装
/// @todo remove コメントスタイルガイド適用済み

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
    QDesktopServices::openUrl(QUrl("https://github.com/hnakada123/ShogiBoardQ"));
}

} // namespace AboutCoordinator
