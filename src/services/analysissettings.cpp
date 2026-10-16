/// @file analysissettings.cpp
/// @brief 解析・検討設定の永続化実装

#include "analysissettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace AnalysisSettings {

// --- 解析タブ ---

namespace {
QList<int> readWidths(const QString& key)
{
    QSettings& s = SettingsCommon::openSettings();
    QList<int> widths;
    const int size = s.beginReadArray(key);
    for (int i = 0; i < size; ++i) {
        s.setArrayIndex(i);
        widths.append(s.value(SettingsKeys::kArrayWidth, 0).toInt());
    }
    s.endArray();
    return widths;
}

void writeWidths(const QString& key, const QList<int>& widths)
{
    QSettings& s = SettingsCommon::openSettings();
    s.beginWriteArray(key);
    for (qsizetype i = 0; i < widths.size(); ++i) {
        s.setArrayIndex(static_cast<int>(i));
        s.setValue(SettingsKeys::kArrayWidth, widths.at(i));
    }
    s.endArray();
    s.sync();
}

QString tableKey(const char* format, ThinkingTable table)
{
    return QString::fromLatin1(format).arg(static_cast<int>(table));
}
} // namespace

QList<int> engineInfoColumnWidths(ThinkingTable table)
{
    return readWidths(tableKey(SettingsKeys::kEngineInfoColumnWidthsFmt, table));
}

void setEngineInfoColumnWidths(ThinkingTable table, const QList<int>& widths)
{
    writeWidths(tableKey(SettingsKeys::kEngineInfoColumnWidthsFmt, table), widths);
}

QList<int> thinkingViewColumnWidths(ThinkingTable table)
{
    return readWidths(tableKey(SettingsKeys::kThinkingViewColumnWidthsFmt, table));
}

void setThinkingViewColumnWidths(ThinkingTable table, const QList<int>& widths)
{
    writeWidths(tableKey(SettingsKeys::kThinkingViewColumnWidthsFmt, table), widths);
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
