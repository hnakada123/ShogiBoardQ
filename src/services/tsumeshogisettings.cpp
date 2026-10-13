/// @file tsumeshogisettings.cpp
/// @brief 詰将棋設定の永続化実装

#include "tsumeshogisettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace TsumeshogiSettings {

CollectionPreferences collectionPreferences()
{
    QSettings& s = SettingsCommon::openSettings();
    CollectionPreferences p;
    p.size = s.value(SettingsKeys::kTsumeCollectionSize, p.size).toSize();
    p.lastFile = s.value(SettingsKeys::kTsumeCollectionFile).toString();
    p.enginePath = s.value(SettingsKeys::kTsumeCollectionEngine).toString();
    p.pageSize = s.value(SettingsKeys::kTsumeCollectionPageSize, 10).toInt();
    p.page = s.value(SettingsKeys::kTsumeCollectionPage, 1).toInt();
    p.filter = s.value(SettingsKeys::kTsumeCollectionFilter, 0).toInt();
    p.timeoutSec = s.value(SettingsKeys::kTsumeCollectionTimeout, 5).toInt();
    return p;
}

void setCollectionPreferences(const CollectionPreferences& p)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeCollectionSize, p.size);
    s.setValue(SettingsKeys::kTsumeCollectionFile, p.lastFile);
    s.setValue(SettingsKeys::kTsumeCollectionEngine, p.enginePath);
    s.setValue(SettingsKeys::kTsumeCollectionPageSize, p.pageSize);
    s.setValue(SettingsKeys::kTsumeCollectionPage, p.page);
    s.setValue(SettingsKeys::kTsumeCollectionFilter, p.filter);
    s.setValue(SettingsKeys::kTsumeCollectionTimeout, p.timeoutSec);
}

PlayPreferences playPreferences()
{
    QSettings& s = SettingsCommon::openSettings();
    PlayPreferences p;
    p.size = s.value(SettingsKeys::kTsumePlaySize, p.size).toSize();
    p.lastFile = s.value(SettingsKeys::kTsumePlayFile).toString();
    p.problemIndex = s.value(SettingsKeys::kTsumePlayIndex, 0).toInt();
    p.timeoutSec = s.value(SettingsKeys::kTsumePlayTimeout, 5).toInt();
    p.squareSize = s.value(SettingsKeys::kTsumePlaySquareSize, 42).toInt();
    p.boardRotated = s.value(SettingsKeys::kTsumePlayBoardRotated, false).toBool();
    return p;
}

void setPlayPreferences(const PlayPreferences& p)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumePlaySize, p.size);
    s.setValue(SettingsKeys::kTsumePlayFile, p.lastFile);
    s.setValue(SettingsKeys::kTsumePlayIndex, p.problemIndex);
    s.setValue(SettingsKeys::kTsumePlayTimeout, p.timeoutSec);
    s.setValue(SettingsKeys::kTsumePlaySquareSize, p.squareSize);
    s.setValue(SettingsKeys::kTsumePlayBoardRotated, p.boardRotated);
}

} // namespace TsumeshogiSettings
