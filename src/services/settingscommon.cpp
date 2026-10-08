/// @file settingscommon.cpp
/// @brief 設定サービスの共通インフラストラクチャ実装

#include "settingscommon.h"
#include <QSettings>
#include <QDir>
#include "apppaths.h"

namespace SettingsCommon {

static const char* kIniName = "ShogiBoardQ.ini";

QString settingsFilePath()
{
    static QString path;
    if (path.isEmpty()) {
        const QString configDir = AppPaths::configDirectory();
        QDir().mkpath(configDir);
        path = configDir + "/" + kIniName;
    }
    return path;
}

QSettings& openSettings()
{
    static QSettings s(settingsFilePath(), QSettings::IniFormat);
    return s;
}

bool resetAllSettings()
{
    QSettings& settings = openSettings();
    settings.clear();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

} // namespace SettingsCommon
