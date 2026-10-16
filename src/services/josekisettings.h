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
#include "setting.h"

namespace JosekiSettings {

/// 定跡ウィンドウのフォントサイズ（デフォルト: 10）
inline const Setting<int> josekiWindowFontSize{"JosekiWindow/fontSize", 10};

/// 定跡ウィンドウの最後に開いた定跡ファイルパス
inline const Setting<QString> josekiWindowLastFilePath{"JosekiWindow/lastFilePath", QString()};

/// 定跡ウィンドウのサイズ（デフォルト: 800x500）
inline const Setting<QSize> josekiWindowSize{"JosekiWindow/size", QSize(800, 500)};

/// 定跡ファイル自動読込（デフォルト: true）
inline const Setting<bool> josekiWindowAutoLoadEnabled{"JosekiWindow/autoLoadEnabled", true};

/// 定跡ウィンドウの最近使ったファイルリスト（最大5件）
inline const Setting<QStringList> josekiWindowRecentFiles{"JosekiWindow/recentFiles", QStringList()};

/// 定跡ウィンドウの表示停止状態（デフォルト: true＝表示有効）
inline const Setting<bool> josekiWindowDisplayEnabled{"JosekiWindow/displayEnabled", true};

/// 定跡ウィンドウのSFEN詳細表示状態（デフォルト: false）
inline const Setting<bool> josekiWindowSfenDetailVisible{"JosekiWindow/sfenDetailVisible", false};

/// 定跡ウィンドウのテーブル列幅
inline const ListSetting<int> josekiWindowColumnWidths{QStringLiteral("JosekiWindow/columnWidths"), "width"};

/// 定跡手編集ダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> josekiMoveDialogFontSize{"JosekiWindow/moveDialogFontSize", 10};

/// 定跡手編集ダイアログのウィンドウサイズ（デフォルト: 500x600）
inline const Setting<QSize> josekiMoveDialogSize{"SizeRelated/josekiMoveDialogSize", QSize(500, 600)};

/// 定跡マージダイアログのフォントサイズ（デフォルト: 10）
inline const Setting<int> josekiMergeDialogFontSize{"JosekiWindow/mergeDialogFontSize", 10};

/// 定跡マージダイアログのウィンドウサイズ（デフォルト: 600x500）
inline const Setting<QSize> josekiMergeDialogSize{"JosekiWindow/mergeDialogSize", QSize(600, 500)};

} // namespace JosekiSettings

#endif // JOSEKISETTINGS_H
