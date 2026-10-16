/// @file analysissettings.cpp
/// @brief 解析・検討設定の永続化実装

#include "analysissettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace AnalysisSettings {

// --- 解析タブ ---

namespace {
QString tableKey(const char* format, ThinkingTable table)
{
    return QString::fromLatin1(format).arg(static_cast<int>(table));
}
} // namespace

// 列幅は表の組ごとの配列（キーは表の番号で決まる）
QList<int> engineInfoColumnWidths(ThinkingTable table)
{
    return ListSetting<int>(tableKey(SettingsKeys::kEngineInfoColumnWidthsFmt, table), SettingsKeys::kArrayWidth)();
}

void setEngineInfoColumnWidths(ThinkingTable table, const QList<int>& widths)
{
    ListSetting<int>(tableKey(SettingsKeys::kEngineInfoColumnWidthsFmt, table), SettingsKeys::kArrayWidth).set(widths);
}

QList<int> thinkingViewColumnWidths(ThinkingTable table)
{
    return ListSetting<int>(tableKey(SettingsKeys::kThinkingViewColumnWidthsFmt, table), SettingsKeys::kArrayWidth)();
}

void setThinkingViewColumnWidths(ThinkingTable table, const QList<int>& widths)
{
    ListSetting<int>(tableKey(SettingsKeys::kThinkingViewColumnWidthsFmt, table), SettingsKeys::kArrayWidth).set(widths);
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
