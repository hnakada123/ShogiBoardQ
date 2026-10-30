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

namespace {

// レイアウト名は設定のキーにする（SettingsCommon::escapeKey。以前の版はそのまま保存していた。
// 「%」を含む名前は、そのままのキーが残っていればそれを使う）
QString layoutKey(const QSettings& s, const QString& name)
{
    const QString group = QString::fromLatin1(SettingsKeys::kCustomDockLayoutsGroup) + QLatin1Char('/');
    const QString escaped = group + SettingsCommon::escapeKey(name);
    const QString legacy = group + name;
    return !s.contains(escaped) && name.contains(QLatin1Char('%')) && s.contains(legacy) ? legacy : escaped;
}

} // namespace

// 保存されているカスタムドックレイアウト名のリストを取得
QStringList savedDockLayoutNames()
{
    QSettings& s = SettingsCommon::openSettings();
    s.beginGroup(SettingsKeys::kCustomDockLayoutsGroup);
    const QStringList keys = s.childKeys();
    s.endGroup();
    QStringList names;
    for (const QString& key : keys) {
        const QString name = SettingsCommon::unescapeKey(key);
        if (!names.contains(name)) names.append(name);
    }
    return names;
}

// 指定した名前でドックレイアウトを保存
void saveDockLayout(const QString& name, const QByteArray& state)
{
    QSettings& s = SettingsCommon::openSettings();
    // 以前の版のキーは消して、今の形のキーに書く
    const QString key = layoutKey(s, name);
    const QString escaped = QString::fromLatin1(SettingsKeys::kCustomDockLayoutsGroup) + QLatin1Char('/') + SettingsCommon::escapeKey(name);
    if (key != escaped) s.remove(key);
    s.setValue(escaped, state);
}

// 指定した名前のドックレイアウトを取得
QByteArray loadDockLayout(const QString& name)
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(layoutKey(s, name), QByteArray()).toByteArray();
}

// 指定した名前のドックレイアウトを削除
void deleteDockLayout(const QString& name)
{
    QSettings& s = SettingsCommon::openSettings();
    s.remove(layoutKey(s, name));
}

} // namespace DockSettings
