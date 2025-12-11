#include <QSettings>
#include <QApplication>
#include <QDir>
#include <QWidget>
#include "shogiview.h"

namespace SettingsService {

static const char* kIniName = "ShogiBoardQ.ini";

void loadWindowSize(QWidget* mainWindow)
{
    if (!mainWindow) return;
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    const QSize sz = s.value("SizeRelated/mainWindowSize", QSize(1100, 720)).toSize();
    if (sz.isValid() && sz.width() > 100 && sz.height() > 100)
        mainWindow->resize(sz);
}

void saveWindowAndBoard(QWidget* mainWindow, ShogiView* view)
{
    if (!mainWindow || !view) return;
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.beginGroup("SizeRelated");
    s.setValue("mainWindowSize", mainWindow->size());
    s.setValue("squareSize",     view->squareSize());
    s.endGroup();
}

QString lastKifuDirectory()
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    return s.value("General/lastKifuDirectory", QString()).toString();
}

void setLastKifuDirectory(const QString& dir)
{
    QDir::setCurrent(QApplication::applicationDirPath());
    QSettings s(kIniName, QSettings::IniFormat);
    s.setValue("General/lastKifuDirectory", dir);
}

} // namespace SettingsService
