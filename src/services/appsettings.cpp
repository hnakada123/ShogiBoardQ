/// @file appsettings.cpp
/// @brief アプリケーション全般設定の永続化実装

#include "appsettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>
#include <QWidget>

namespace AppSettings {

// --- 言語 ---

QString language()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kLanguage, "system").toString();
}

void setLanguage(const QString& lang)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kLanguage, lang);
}

// --- UI状態 ---

int lastSelectedTabIndex()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kLastSelectedTabIndex, 0).toInt();
}

void setLastSelectedTabIndex(int index)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kLastSelectedTabIndex, index);
}

bool toolbarVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kToolbarVisible, true).toBool();
}

void setToolbarVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kToolbarVisible, visible);
}

// --- メニューウィンドウ ---

QStringList menuWindowFavorites()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowFavorites, QStringList()).toStringList();
}

void setMenuWindowFavorites(const QStringList& favorites)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowFavorites, favorites);
}

QSize menuWindowSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowSize, QSize(500, 400)).toSize();
}

void setMenuWindowSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowSize, size);
}

int menuWindowButtonSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowButtonSize, 72).toInt();
}

void setMenuWindowButtonSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowButtonSize, size);
}

int menuWindowFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowFontSize, 9).toInt();
}

void setMenuWindowFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowFontSize, size);
}

// --- メインウィンドウ ---

void loadWindowSize(QWidget* mainWindow)
{
    if (!mainWindow) return;
    QSettings& s = SettingsCommon::openSettings();
    const QSize sz = s.value(SettingsKeys::kMainWindowSize, QSize(1100, 720)).toSize();
    if (sz.isValid() && sz.width() > 100 && sz.height() > 100)
        mainWindow->resize(sz);
}

void saveWindowAndBoard(QWidget* mainWindow, int squareSize)
{
    if (!mainWindow) return;
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMainWindowSize, mainWindow->size());
    s.setValue(SettingsKeys::kSquareSize,     squareSize);
    if (s.status() != QSettings::NoError) {
        qWarning("AppSettings: Failed to save settings (status=%d)", static_cast<int>(s.status()));
    }
}

// --- マイグレーション ---

void migrateSettingsIfNeeded()
{
    QSettings& s = SettingsCommon::openSettings();
    const int version = s.value(SettingsKeys::kSettingsVersion, 0).toInt();
    if (version >= SettingsKeys::kCurrentSettingsVersion) {
        return;
    }

    // 将来のマイグレーション処理をここに追加:
    // if (version < 2) { ... }
    // if (version < 3) { ... }

    s.setValue(SettingsKeys::kSettingsVersion, SettingsKeys::kCurrentSettingsVersion);
}

} // namespace AppSettings
