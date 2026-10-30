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

QString escapeKey(const QString& name)
{
    QString key;
    key.reserve(name.size());
    for (const QChar ch : name) {
        if (ch == QLatin1Char('%')) key += QStringLiteral("%25");
        else if (ch == QLatin1Char('/')) key += QStringLiteral("%2F");
        else if (ch == QLatin1Char('\\')) key += QStringLiteral("%5C");
        else key += ch;
    }
    return key;
}

QString unescapeKey(const QString& key)
{
    QString name;
    name.reserve(key.size());
    for (qsizetype i = 0; i < key.size(); ++i) {
        const QStringView code = QStringView(key).mid(i, 3);
        if (code == u"%2F") { name += QLatin1Char('/'); i += 2; }
        else if (code == u"%5C") { name += QLatin1Char('\\'); i += 2; }
        else if (code == u"%25") { name += QLatin1Char('%'); i += 2; }
        else name += key.at(i);
    }
    return name;
}

bool resetAllSettings()
{
    QSettings& settings = openSettings();
    settings.clear();
    settings.sync();
    return settings.status() == QSettings::NoError;
}

} // namespace SettingsCommon
