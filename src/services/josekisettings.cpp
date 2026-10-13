/// @file josekisettings.cpp
/// @brief 定跡設定の永続化実装

#include "josekisettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace JosekiSettings {

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

} // namespace JosekiSettings
