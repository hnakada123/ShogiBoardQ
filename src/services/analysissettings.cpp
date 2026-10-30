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
ListSetting<int> engineInfoColumnWidths(ThinkingTable table)
{
    return {tableKey(SettingsKeys::kEngineInfoColumnWidthsFmt, table), SettingsKeys::kArrayWidth};
}

ListSetting<int> thinkingViewColumnWidths(ThinkingTable table)
{
    return {tableKey(SettingsKeys::kThinkingViewColumnWidthsFmt, table), SettingsKeys::kArrayWidth};
}

// --- 棋譜解析 ---

ListSetting<int> kifuAnalysisColumnWidths()
{
    const QString key = QString::fromLatin1(SettingsKeys::kKifuAnalysisColumnWidths);
    const ListSetting<int> widths{key, SettingsKeys::kArrayWidth};
    // 以前の版は同じキーに1つの値（QVariantList）で保存していた。配列に置き換える
    QSettings& s = SettingsCommon::openSettings();
    const QVariant legacy = s.value(key);
    if (legacy.isValid()) {
        QList<int> values;
        const QVariantList list = legacy.toList();
        for (const QVariant& value : list) values.append(qMax(0, value.toInt()));
        s.remove(key);
        widths.set(values);
    }
    return widths;
}

} // namespace AnalysisSettings
