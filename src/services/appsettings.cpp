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

QStringList availablePieceStyles()
{
    return {QStringLiteral("standard"), QStringLiteral("clear"), QStringLiteral("wood"),
            QStringLiteral("ivory"), QStringLiteral("dark")};
}

QString pieceStyle()
{
    const QString style = SettingsCommon::openSettings()
        .value(SettingsKeys::kPieceStyle, QStringLiteral("standard")).toString();
    return availablePieceStyles().contains(style) ? style : QStringLiteral("standard");
}

void setPieceStyle(const QString& style)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kPieceStyle,
        availablePieceStyles().contains(style) ? style : QStringLiteral("standard"));
}

// --- 盤面の配色 ---

BoardColors boardColors()
{
    QSettings& s = SettingsCommon::openSettings();
    const BoardColors defaults;
    const BoardColors colors{
        QColor(s.value(SettingsKeys::kBoardBackgroundColor, defaults.background.name()).toString()),
        QColor(s.value(SettingsKeys::kBoardSurfaceColor, defaults.board.name()).toString()),
        QColor(s.value(SettingsKeys::kBoardStandColor, defaults.stand.name()).toString()),
        QColor(s.value(SettingsKeys::kBoardGridColor, defaults.grid.name()).toString())};
    return colors.normalized();
}

void setBoardColors(const BoardColors& colors)
{
    QSettings& s = SettingsCommon::openSettings();
    const BoardColors normalized = colors.normalized();
    s.setValue(SettingsKeys::kBoardBackgroundColor, normalized.background.name());
    s.setValue(SettingsKeys::kBoardSurfaceColor, normalized.board.name());
    s.setValue(SettingsKeys::kBoardStandColor, normalized.stand.name());
    s.setValue(SettingsKeys::kBoardGridColor, normalized.grid.name());
}

QSize boardColorDialogSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kBoardColorDialogSize, QSize(460, 300)).toSize();
}

void setBoardColorDialogSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kBoardColorDialogSize, size);
}

QSize boardColorPickerSize()
{
    return SettingsCommon::openSettings().value(SettingsKeys::kBoardColorPickerSize, QSize()).toSize();
}

void setBoardColorPickerSize(const QSize& size)
{
    SettingsCommon::openSettings().setValue(SettingsKeys::kBoardColorPickerSize, size);
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
