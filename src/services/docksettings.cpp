/// @file docksettings.cpp
/// @brief ドック配置設定の永続化実装

#include "docksettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace DockSettings {

DockState dockState(const QString& group, bool defaultVisible)
{
    const QSettings& s = SettingsCommon::openSettings();
    DockState state;
    state.floating = s.value(group + QStringLiteral("/floating"), false).toBool();
    state.visible = s.value(group + QStringLiteral("/visible"), defaultVisible).toBool();
    state.geometry = s.value(group + QStringLiteral("/geometry")).toByteArray();
    return state;
}

void setDockState(const QString& group, const DockState& state)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(group + QStringLiteral("/floating"), state.floating);
    s.setValue(group + QStringLiteral("/visible"), state.visible);
    s.setValue(group + QStringLiteral("/geometry"), state.geometry);
}

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
