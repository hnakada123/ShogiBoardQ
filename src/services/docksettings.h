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

namespace DockSettings {

/// 評価値グラフドックの状態（QMainWindow::saveState()のバイト列）
QByteArray evalChartDockState();
void setEvalChartDockState(const QByteArray& state);

/// 評価値グラフドックのフローティング状態
bool evalChartDockFloating();
void setEvalChartDockFloating(bool floating);

/// 評価値グラフドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray evalChartDockGeometry();
void setEvalChartDockGeometry(const QByteArray& geometry);

/// 評価値グラフドックの表示状態
bool evalChartDockVisible();
void setEvalChartDockVisible(bool visible);

/// 棋譜欄ドックのフローティング状態
bool recordPaneDockFloating();
void setRecordPaneDockFloating(bool floating);

/// 棋譜欄ドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray recordPaneDockGeometry();
void setRecordPaneDockGeometry(const QByteArray& geometry);

/// 棋譜欄ドックの表示状態
bool recordPaneDockVisible();
void setRecordPaneDockVisible(bool visible);

/// 解析タブドックのフローティング状態
bool analysisTabDockFloating();
void setAnalysisTabDockFloating(bool floating);

/// 解析タブドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray analysisTabDockGeometry();
void setAnalysisTabDockGeometry(const QByteArray& geometry);

/// 解析タブドックの表示状態
bool analysisTabDockVisible();
void setAnalysisTabDockVisible(bool visible);

/// 将棋盤ドックのフローティング状態
bool boardDockFloating();
void setBoardDockFloating(bool floating);

/// 将棋盤ドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray boardDockGeometry();
void setBoardDockGeometry(const QByteArray& geometry);

/// 将棋盤ドックの表示状態
bool boardDockVisible();
void setBoardDockVisible(bool visible);

/// メニューウィンドウドックのフローティング状態
bool menuWindowDockFloating();
void setMenuWindowDockFloating(bool floating);

/// メニューウィンドウドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray menuWindowDockGeometry();
void setMenuWindowDockGeometry(const QByteArray& geometry);

/// メニューウィンドウドックの表示状態
bool menuWindowDockVisible();
void setMenuWindowDockVisible(bool visible);

/// 定跡ウィンドウドックのフローティング状態
bool josekiWindowDockFloating();
void setJosekiWindowDockFloating(bool floating);

/// 定跡ウィンドウドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray josekiWindowDockGeometry();
void setJosekiWindowDockGeometry(const QByteArray& geometry);

/// 定跡ウィンドウドックの表示状態
bool josekiWindowDockVisible();
void setJosekiWindowDockVisible(bool visible);

/// 棋譜解析結果ドックのフローティング状態
bool kifuAnalysisResultsDockFloating();
void setKifuAnalysisResultsDockFloating(bool floating);

/// 棋譜解析結果ドックのジオメトリ（フローティング時のウィンドウ位置・サイズ）
QByteArray kifuAnalysisResultsDockGeometry();
void setKifuAnalysisResultsDockGeometry(const QByteArray& geometry);

/// 棋譜解析結果ドックの表示状態
bool kifuAnalysisResultsDockVisible();
void setKifuAnalysisResultsDockVisible(bool visible);

/// 全ドックの固定設定
bool docksLocked();
void setDocksLocked(bool locked);

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
QString startupDockLayoutName();
void setStartupDockLayoutName(const QString& name);

} // namespace DockSettings

#endif // DOCKSETTINGS_H
