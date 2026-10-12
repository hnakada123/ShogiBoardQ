/// @file networksettings.cpp
/// @brief CSAネットワーク設定の永続化実装

#include "networksettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace NetworkSettings {

int csaLogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeCsaLog, 10).toInt();
}

void setCsaLogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaLog, size);
}

int csaWaitingDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeCsaWaitingDialog, 10).toInt();
}

void setCsaWaitingDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaWaitingDialog, size);
}

int csaGameDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeCsaGameDialog, 10).toInt();
}

void setCsaGameDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeCsaGameDialog, size);
}

QSize csaLogWindowSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kCsaLogWindowSize, QSize(550, 450)).toSize();
}

void setCsaLogWindowSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kCsaLogWindowSize, size);
}

QSize csaGameDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kCsaGameDialogSize, QSize(450, 380)).toSize();
}

void setCsaGameDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kCsaGameDialogSize, size);
}

QSize csaWaitingDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kCsaWaitingDialogSize, QSize(400, 260)).toSize();
}

void setCsaWaitingDialogSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kCsaWaitingDialogSize, size);
}


namespace {
template <typename T>
T readValue(const char* key, const T& fallback)
{
    return SettingsCommon::openSettings().value(QLatin1String(key), fallback).template value<T>();
}
template <typename T>
void writeValue(const char* key, const T& value)
{
    SettingsCommon::openSettings().setValue(QLatin1String(key), value);
}
} // namespace

bool csaKeepAliveEnabled() { return readValue<bool>(SettingsKeys::kCsaKeepAliveEnabled, true); }
void setCsaKeepAliveEnabled(bool enabled) { writeValue(SettingsKeys::kCsaKeepAliveEnabled, enabled); }

int csaKeepAliveIntervalSec() { return readValue<int>(SettingsKeys::kCsaKeepAliveIntervalSec, 40); }
void setCsaKeepAliveIntervalSec(int seconds) { writeValue(SettingsKeys::kCsaKeepAliveIntervalSec, seconds); }

bool csaTcpKeepAliveEnabled() { return readValue<bool>(SettingsKeys::kCsaTcpKeepAliveEnabled, true); }
void setCsaTcpKeepAliveEnabled(bool enabled) { writeValue(SettingsKeys::kCsaTcpKeepAliveEnabled, enabled); }

int csaThinkingMarginSec() { return readValue<int>(SettingsKeys::kCsaThinkingMarginSec, 1); }
void setCsaThinkingMarginSec(int seconds) { writeValue(SettingsKeys::kCsaThinkingMarginSec, seconds); }

int csaGameCountMode() { return readValue<int>(SettingsKeys::kCsaGameCountMode, 0); }
void setCsaGameCountMode(int mode) { writeValue(SettingsKeys::kCsaGameCountMode, mode); }

int csaGameCount() { return readValue<int>(SettingsKeys::kCsaGameCount, 2); }
void setCsaGameCount(int count) { writeValue(SettingsKeys::kCsaGameCount, count); }

bool csaReconnectAfterGame() { return readValue<bool>(SettingsKeys::kCsaReconnectAfterGame, false); }
void setCsaReconnectAfterGame(bool enabled) { writeValue(SettingsKeys::kCsaReconnectAfterGame, enabled); }

bool csaAutoSaveEnabled() { return readValue<bool>(SettingsKeys::kCsaAutoSaveEnabled, false); }
void setCsaAutoSaveEnabled(bool enabled) { writeValue(SettingsKeys::kCsaAutoSaveEnabled, enabled); }

QString csaAutoSaveDir() { return readValue<QString>(SettingsKeys::kCsaAutoSaveDir, QString()); }
void setCsaAutoSaveDir(const QString& dir) { writeValue(SettingsKeys::kCsaAutoSaveDir, dir); }

QString csaAutoSaveFormat() { return readValue<QString>(SettingsKeys::kCsaAutoSaveFormat, QStringLiteral("kif")); }
void setCsaAutoSaveFormat(const QString& extension) { writeValue(SettingsKeys::kCsaAutoSaveFormat, extension); }

bool csaAutoFlipBoard() { return readValue<bool>(SettingsKeys::kCsaAutoFlipBoard, true); }
void setCsaAutoFlipBoard(bool enabled) { writeValue(SettingsKeys::kCsaAutoFlipBoard, enabled); }

QString csaGameName() { return readValue<QString>(SettingsKeys::kCsaGameName, QString()); }
void setCsaGameName(const QString& name) { writeValue(SettingsKeys::kCsaGameName, name); }

int csaTurnPreference() { return readValue<int>(SettingsKeys::kCsaTurnPreference, 0); }
void setCsaTurnPreference(int preference) { writeValue(SettingsKeys::kCsaTurnPreference, preference); }

} // namespace NetworkSettings
