/// @file docksettings.cpp
/// @brief ドック配置設定の永続化実装

#include "docksettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace DockSettings {

// 保存されているカスタムドックレイアウト名のリストを取得
QStringList savedDockLayoutNames()
{
    QSettings& s = SettingsCommon::openSettings();
    s.beginGroup(SettingsKeys::kCustomDockLayoutsGroup);
    QStringList names = s.childKeys();
    s.endGroup();
    return names;
}

// 指定した名前でドックレイアウトを保存
void saveDockLayout(const QString& name, const QByteArray& state)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name, state);
}

// 指定した名前のドックレイアウトを取得
QByteArray loadDockLayout(const QString& name)
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name, QByteArray()).toByteArray();
}

// 指定した名前のドックレイアウトを削除
void deleteDockLayout(const QString& name)
{
    QSettings& s = SettingsCommon::openSettings();
    s.remove(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name);
}

} // namespace DockSettings
