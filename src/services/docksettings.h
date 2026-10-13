#ifndef DOCKSETTINGS_H
#define DOCKSETTINGS_H

/// @file docksettings.h
/// @brief ドック配置設定の永続化
///
/// ドックウィジェットの配置・表示状態・レイアウト管理に関する設定を提供します。
/// 呼び出し元: dockcreationservice.cpp, docklayoutmanager.cpp, mainwindowuibootstrapper.cpp

#include <QString>
#include <QStringList>
#include <QByteArray>
#include "setting.h"

namespace DockSettings {

/// 評価値グラフドックのフローティング状態
inline const Setting<bool> evalChartDockFloating{"EvalChartDock/floating", false};

/// 評価値グラフドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
inline const Setting<QByteArray> evalChartDockGeometry{"EvalChartDock/geometry", QByteArray()};

/// 評価値グラフドックの表示状態
inline const Setting<bool> evalChartDockVisible{"EvalChartDock/visible", true};

/// 棋譜欄ドックのフローティング状態
inline const Setting<bool> recordPaneDockFloating{"RecordPaneDock/floating", false};

/// 棋譜欄ドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
inline const Setting<QByteArray> recordPaneDockGeometry{"RecordPaneDock/geometry", QByteArray()};

/// 棋譜欄ドックの表示状態
inline const Setting<bool> recordPaneDockVisible{"RecordPaneDock/visible", true};

/// メニューウィンドウドックのフローティング状態
inline const Setting<bool> menuWindowDockFloating{"MenuWindowDock/floating", false};

/// メニューウィンドウドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
inline const Setting<QByteArray> menuWindowDockGeometry{"MenuWindowDock/geometry", QByteArray()};

/// メニューウィンドウドックの表示状態
inline const Setting<bool> menuWindowDockVisible{"MenuWindowDock/visible", false};

/// 定跡ウィンドウドックのフローティング状態
inline const Setting<bool> josekiWindowDockFloating{"JosekiWindowDock/floating", false};

/// 定跡ウィンドウドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
inline const Setting<QByteArray> josekiWindowDockGeometry{"JosekiWindowDock/geometry", QByteArray()};

/// 定跡ウィンドウドックの表示状態
inline const Setting<bool> josekiWindowDockVisible{"JosekiWindowDock/visible", false};

/// 棋譜解析結果ドックのフローティング状態
inline const Setting<bool> kifuAnalysisResultsDockFloating{"KifuAnalysisResultsDock/floating", false};

/// 棋譜解析結果ドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
inline const Setting<QByteArray> kifuAnalysisResultsDockGeometry{"KifuAnalysisResultsDock/geometry", QByteArray()};

/// 棋譜解析結果ドックの表示状態
inline const Setting<bool> kifuAnalysisResultsDockVisible{"KifuAnalysisResultsDock/visible", false};

/// 全ドックの固定設定
inline const Setting<bool> docksLocked{"Dock/docksLocked", false};

/// カスタムドックレイアウトの保存・読み込み
/// 保存されているレイアウト名のリストを取得
QStringList savedDockLayoutNames();
/// 指定した名前でレイアウトを保存
void saveDockLayout(const QString& name, const QByteArray& state);
/// 指定した名前のレイアウトを取得（存在しない場合は空のQByteArray）
QByteArray loadDockLayout(const QString& name);
/// 指定した名前のレイアウトを削除
void deleteDockLayout(const QString& name);

/// 起動時に使用するレイアウト名
/// 空文字列の場合はデフォルトレイアウトを使用
inline const Setting<QString> startupDockLayoutName{"DockLayout/startupLayoutName", QString()};

} // namespace DockSettings

#endif // DOCKSETTINGS_H
