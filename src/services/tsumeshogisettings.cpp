/// @file tsumeshogisettings.cpp
/// @brief 詰将棋設定の永続化実装

#include "tsumeshogisettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace TsumeshogiSettings {

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

} // namespace TsumeshogiSettings
