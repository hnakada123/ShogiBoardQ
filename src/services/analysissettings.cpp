/// @file analysissettings.cpp
/// @brief 解析・検討設定の永続化実装

#include "analysissettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace AnalysisSettings {

// --- 解析タブ ---

QList<int> engineInfoColumnWidths(int widgetIndex)
{
    QSettings& s = SettingsCommon::openSettings();
    QString key = QString(SettingsKeys::kEngineInfoColumnWidthsFmt).arg(widgetIndex);
    QList<int> widths;

    int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value(SettingsKeys::kArrayWidth, 0).toInt());
    }
    s.endArray();

    return widths;
}

void setEngineInfoColumnWidths(int widgetIndex, const QList<int>& widths)
{
    QSettings& s = SettingsCommon::openSettings();
    QString key = QString(SettingsKeys::kEngineInfoColumnWidthsFmt).arg(widgetIndex);

    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();

    s.sync();
}

QList<int> thinkingViewColumnWidths(int viewIndex)
{
    QSettings& s = SettingsCommon::openSettings();
    QString key = QString(SettingsKeys::kThinkingViewColumnWidthsFmt).arg(viewIndex);
    QList<int> widths;

    int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value(SettingsKeys::kArrayWidth, 0).toInt());
    }
    s.endArray();

    return widths;
}

void setThinkingViewColumnWidths(int viewIndex, const QList<int>& widths)
{
    QSettings& s = SettingsCommon::openSettings();
    QString key = QString(SettingsKeys::kThinkingViewColumnWidthsFmt).arg(viewIndex);

    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();

    s.sync();
}

int usiLogCommandTarget()
{
    return qBound(0, SettingsCommon::openSettings().value(SettingsKeys::kUsiLogCommandTarget, 0).toInt(), 2);
}

void setUsiLogCommandTarget(int target)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kUsiLogCommandTarget, qBound(0, target, 2));
}

// --- 棋譜解析 ---

QList<int> kifuAnalysisColumnWidths()
{
    const QVariantList values = SettingsCommon::openSettings()
        .value(SettingsKeys::kKifuAnalysisColumnWidths).toList();
    QList<int> widths;
    for (const QVariant& value : values) widths.append(qMax(0, value.toInt()));
    return widths;
}

void setKifuAnalysisColumnWidths(const QList<int>& widths)
{
    QVariantList values;
    for (int width : widths) values.append(width);
    SettingsCommon::openSettings().setValue(SettingsKeys::kKifuAnalysisColumnWidths, values);
}

} // namespace AnalysisSettings
