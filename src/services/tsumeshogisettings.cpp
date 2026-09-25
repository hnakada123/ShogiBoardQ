/// @file tsumeshogisettings.cpp
/// @brief 詰将棋設定の永続化実装

#include "tsumeshogisettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace TsumeshogiSettings {

int tsumePlayFontSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kFontSizeTsumePlay, 10).toInt();
}

void setTsumePlayFontSize(int size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kFontSizeTsumePlay, size);
}

int tsumeCollectionFontSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kFontSizeTsumeCollection, 10).toInt();
}

void setTsumeCollectionFontSize(int size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kFontSizeTsumeCollection, size);
}

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

QString tsumeshogiGeneratorLastSaveDirectory()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorLastSaveDirectory, QString()).toString();
}

void setTsumeshogiGeneratorLastSaveDirectory(const QString& dir)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorLastSaveDirectory, dir);
}

QSize tsumeshogiGeneratorDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorDialogSize, QSize(600, 550)).toSize();
}

void setTsumeshogiGeneratorDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorDialogSize, size);
}

int tsumeshogiGeneratorFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeTsumeshogiGenerator, 10).toInt();
}

void setTsumeshogiGeneratorFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeTsumeshogiGenerator, size);
}

int tsumeshogiGeneratorEngineIndex()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorEngineIndex, 0).toInt();
}

void setTsumeshogiGeneratorEngineIndex(int index)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorEngineIndex, index);
}

int tsumeshogiGeneratorTargetMoves()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorTargetMoves, 3).toInt();
}

void setTsumeshogiGeneratorTargetMoves(int moves)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorTargetMoves, moves);
}

int tsumeshogiGeneratorMaxAttackPieces()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorMaxAttackPieces, 4).toInt();
}

void setTsumeshogiGeneratorMaxAttackPieces(int count)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorMaxAttackPieces, count);
}

int tsumeshogiGeneratorMaxDefendPieces()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorMaxDefendPieces, 1).toInt();
}

void setTsumeshogiGeneratorMaxDefendPieces(int count)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorMaxDefendPieces, count);
}

int tsumeshogiGeneratorAttackRange()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorAttackRange, 3).toInt();
}

void setTsumeshogiGeneratorAttackRange(int range)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorAttackRange, range);
}

int tsumeshogiGeneratorTimeoutSec()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorTimeoutSec, 5).toInt();
}

void setTsumeshogiGeneratorTimeoutSec(int sec)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorTimeoutSec, sec);
}

int tsumeshogiGeneratorMaxPositions()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorMaxPositions, 10).toInt();
}

void setTsumeshogiGeneratorMaxPositions(int count)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorMaxPositions, count);
}

bool tsumeshogiGeneratorIncludePv()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorIncludePv, false).toBool();
}

void setTsumeshogiGeneratorIncludePv(bool include)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorIncludePv, include);
}

bool tsumeshogiGeneratorAllowFinalMoveAlternatives()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kTsumeshogiGeneratorAllowFinalMoveAlternatives, true).toBool();
}

void setTsumeshogiGeneratorAllowFinalMoveAlternatives(bool allow)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kTsumeshogiGeneratorAllowFinalMoveAlternatives, allow);
}

} // namespace TsumeshogiSettings
