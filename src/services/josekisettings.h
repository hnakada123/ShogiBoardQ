#ifndef JOSEKISETTINGS_H
#define JOSEKISETTINGS_H

/// @file josekisettings.h
/// @brief 定跡設定の永続化
///
/// 定跡ウィンドウ・定跡手編集・定跡マージに関する設定を提供します。
/// 呼び出し元: josekiwindow.cpp, josekimovedialog.cpp, josekimergedialog.cpp

#include <QString>
#include <QStringList>
#include <QSize>
#include <QList>

namespace JosekiSettings {

/// 定跡ウィンドウのフォントサイズ（デフォルト: 10）
int josekiWindowFontSize();
void setJosekiWindowFontSize(int size);

/// 定跡ウィンドウのSFEN表示フォントサイズ（デフォルト: 9）
int josekiWindowSfenFontSize();
void setJosekiWindowSfenFontSize(int size);

/// 定跡ウィンドウの最後に開いた定跡ファイルパス
QString josekiWindowLastFilePath();
void setJosekiWindowLastFilePath(const QString& path);

/// 定跡ウィンドウのサイズ（デフォルト: 800x500）
QSize josekiWindowSize();
void setJosekiWindowSize(const QSize& size);

/// 定跡ファイル自動読込（デフォルト: true）
bool josekiWindowAutoLoadEnabled();
void setJosekiWindowAutoLoadEnabled(bool enabled);

/// 定跡ウィンドウの最近使ったファイルリスト（最大5件）
QStringList josekiWindowRecentFiles();
void setJosekiWindowRecentFiles(const QStringList& files);

/// 定跡ウィンドウの表示停止状態（デフォルト: true＝表示有効）
bool josekiWindowDisplayEnabled();
void setJosekiWindowDisplayEnabled(bool enabled);

/// 定跡ウィンドウのSFEN詳細表示状態（デフォルト: false）
bool josekiWindowSfenDetailVisible();
void setJosekiWindowSfenDetailVisible(bool visible);

/// 定跡ウィンドウのテーブル列幅
QList<int> josekiWindowColumnWidths();
void setJosekiWindowColumnWidths(const QList<int>& widths);

/// 定跡手編集ダイアログのフォントサイズ（デフォルト: 10）
int josekiMoveDialogFontSize();
void setJosekiMoveDialogFontSize(int size);

/// 定跡手編集ダイアログのウィンドウサイズ（デフォルト: 500x600）
QSize josekiMoveDialogSize();
void setJosekiMoveDialogSize(const QSize& size);

/// 定跡マージダイアログのフォントサイズ（デフォルト: 10）
int josekiMergeDialogFontSize();
void setJosekiMergeDialogFontSize(int size);

/// 定跡マージダイアログのウィンドウサイズ（デフォルト: 600x500）
QSize josekiMergeDialogSize();
void setJosekiMergeDialogSize(const QSize& size);

} // namespace JosekiSettings

#endif // JOSEKISETTINGS_H
