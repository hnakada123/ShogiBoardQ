/// @file gamesettings.cpp
/// @brief 対局・棋譜設定の永続化実装

#include "gamesettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace GameSettings {

// --- 棋譜ファイルI/O ---

QString lastKifuDirectory()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kLastKifuDirectory, QString()).toString();
}

void setLastKifuDirectory(const QString& dir)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kLastKifuDirectory, dir);
}

QString lastKifuSaveDirectory()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kLastKifuSaveDirectory, QString()).toString();
}

void setLastKifuSaveDirectory(const QString& dir)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kLastKifuSaveDirectory, dir);
}

// --- 棋譜欄 ---

int kifuPaneFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeKifuPane, 10).toInt();
}

void setKifuPaneFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeKifuPane, size);
}

bool kifuTimeColumnVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kRecordPaneTimeColumnVisible, true).toBool();
}

void setKifuTimeColumnVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kRecordPaneTimeColumnVisible, visible);
}

bool kifuBookmarkColumnVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kRecordPaneBookmarkColumnVisible, true).toBool();
}

void setKifuBookmarkColumnVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kRecordPaneBookmarkColumnVisible, visible);
}

bool kifuCommentColumnVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kRecordPaneCommentColumnVisible, true).toBool();
}

void setKifuCommentColumnVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kRecordPaneCommentColumnVisible, visible);
}

// --- メインタブ（コメント・対局情報） ---

int commentFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeComment, 10).toInt();
}

void setCommentFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeComment, size);
}

int gameInfoFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeGameInfo, 10).toInt();
}

void setGameInfoFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeGameInfo, size);
}

// --- 対局開始 ---

QSize startGameDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kStartGameDialogSize, QSize(1000, 580)).toSize();
}

void setStartGameDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kStartGameDialogSize, size);
}

int startGameDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeStartGameDialog, 9).toInt();
}

void setStartGameDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeStartGameDialog, size);
}

// --- 棋譜貼り付け ---

QSize kifuPasteDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuPasteDialogSize, QSize(600, 500)).toSize();
}

void setKifuPasteDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuPasteDialogSize, size);
}

int kifuPasteDialogFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeKifuPasteDialog, 10).toInt();
}

void setKifuPasteDialogFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeKifuPasteDialog, size);
}

// --- 局面集 ---

QSize sfenCollectionDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kSfenCollectionDialogSize, QSize(620, 780)).toSize();
}

void setSfenCollectionDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kSfenCollectionDialogSize, size);
}

QStringList sfenCollectionRecentFiles()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kSfenCollectionRecentFiles, QStringList()).toStringList();
}

void setSfenCollectionRecentFiles(const QStringList& files)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kSfenCollectionRecentFiles, files);
}

int sfenCollectionSquareSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kSfenCollectionSquareSize, 50).toInt();
}

void setSfenCollectionSquareSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kSfenCollectionSquareSize, size);
}

QString sfenCollectionLastDirectory()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kSfenCollectionLastDirectory, QString()).toString();
}

void setSfenCollectionLastDirectory(const QString& dir)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kSfenCollectionLastDirectory, dir);
}

// --- 持将棋 ---

int jishogiScoreFontSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kFontSizeJishogiScore, 10).toInt();
}

void setJishogiScoreFontSize(int size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kFontSizeJishogiScore, size);
}

QSize jishogiScoreDialogSize()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJishogiScoreDialogSize, QSize(250, 280)).toSize();
}

void setJishogiScoreDialogSize(const QSize& size)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJishogiScoreDialogSize, size);
}

} // namespace GameSettings
