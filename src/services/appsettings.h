#ifndef APPSETTINGS_H
#define APPSETTINGS_H

/// @file appsettings.h
/// @brief アプリケーション全般設定の永続化
///
/// 言語・UI状態・メニューウィンドウ・メインウィンドウに関する設定を提供します。
/// 呼び出し元: main.cpp, mainwindowuibootstrapper.cpp, mainwindowlifecyclepipeline.cpp,
///             mainwindowappearancecontroller.cpp, languagecontroller.cpp, menuwindow.cpp,
///             playerinfowiring.cpp

#include <QString>
#include <QStringList>
#include <QSize>

class QWidget;

namespace AppSettings {

// --- 言語 ---

/// 言語コード（デフォルト: "system"）
QString language();
void setLanguage(const QString& lang);

// --- UI状態 ---

/// 最後に選択されたタブインデックス（デフォルト: 0＝対局情報）
int lastSelectedTabIndex();
void setLastSelectedTabIndex(int index);

/// ツールバーの表示状態（デフォルト: true）
bool toolbarVisible();
void setToolbarVisible(bool visible);

// --- メニューウィンドウ ---

/// メニューウィンドウのお気に入りアクションリスト
QStringList menuWindowFavorites();
void setMenuWindowFavorites(const QStringList& favorites);

/// メニューウィンドウのサイズ（デフォルト: 500x400）
QSize menuWindowSize();
void setMenuWindowSize(const QSize& size);

/// メニューウィンドウのボタンサイズ（デフォルト: 72）
int menuWindowButtonSize();
void setMenuWindowButtonSize(int size);

/// メニューウィンドウのフォントサイズ（デフォルト: 9）
int menuWindowFontSize();
void setMenuWindowFontSize(int size);

// --- メインウィンドウ ---

/// メインウィンドウのサイズを読み込んで適用
void loadWindowSize(QWidget* mainWindow);

/// メインウィンドウのサイズと盤マスサイズをINIに保存
void saveWindowAndBoard(QWidget* mainWindow, int squareSize);

// --- マイグレーション ---

/// 設定バージョンのマイグレーション
void migrateSettingsIfNeeded();

} // namespace AppSettings

#endif // APPSETTINGS_H
