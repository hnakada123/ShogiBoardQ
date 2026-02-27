/// @file josekisettings.cpp
/// @brief 定跡設定の永続化実装

#include "josekisettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace JosekiSettings {

int josekiWindowFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowFontSize, 10).toInt();
}

void setJosekiWindowFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowFontSize, size);
}

int josekiWindowSfenFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowSfenFontSize, 9).toInt();
}

void setJosekiWindowSfenFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowSfenFontSize, size);
}

QString josekiWindowLastFilePath()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowLastFilePath, QString()).toString();
}

void setJosekiWindowLastFilePath(const QString& path)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowLastFilePath, path);
}

QSize josekiWindowSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowSize, QSize(800, 500)).toSize();
}

void setJosekiWindowSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowSize, size);
}

bool josekiWindowAutoLoadEnabled()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowAutoLoadEnabled, true).toBool();
}

void setJosekiWindowAutoLoadEnabled(bool enabled)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowAutoLoadEnabled, enabled);
}

QStringList josekiWindowRecentFiles()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowRecentFiles, QStringList()).toStringList();
}

void setJosekiWindowRecentFiles(const QStringList& files)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowRecentFiles, files);
}

bool josekiWindowDisplayEnabled()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowDisplayEnabled, true).toBool();
}

void setJosekiWindowDisplayEnabled(bool enabled)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowDisplayEnabled, enabled);
}

bool josekiWindowSfenDetailVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowSfenDetailVisible, false).toBool();
}

void setJosekiWindowSfenDetailVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowSfenDetailVisible, visible);
}

QList<int> josekiWindowColumnWidths()
{
    QSettings& s = SettingsCommon::openSettings();
    QList<int> widths;

    int size = s.beginReadArray(SettingsKeys::kJosekiWindowColumnWidths);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value(SettingsKeys::kArrayWidth, 0).toInt());
    }
    s.endArray();

    return widths;
}

void setJosekiWindowColumnWidths(const QList<int>& widths)
{
    QSettings& s = SettingsCommon::openSettings();

    s.beginWriteArray(SettingsKeys::kJosekiWindowColumnWidths);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();

    s.sync();
}

int josekiMoveDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowMoveDialogFontSize, 10).toInt();
}

void setJosekiMoveDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowMoveDialogFontSize, size);
}

QSize josekiMoveDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiMoveDialogSize, QSize(500, 600)).toSize();
}

void setJosekiMoveDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiMoveDialogSize, size);
}

int josekiMergeDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowMergeDialogFontSize, 10).toInt();
}

void setJosekiMergeDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowMergeDialogFontSize, size);
}

QSize josekiMergeDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowMergeDialogSize, QSize(600, 500)).toSize();
}

void setJosekiMergeDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowMergeDialogSize, size);
}

} // namespace JosekiSettings
