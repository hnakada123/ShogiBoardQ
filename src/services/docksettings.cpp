/// @file docksettings.cpp
/// @brief ドック配置設定の永続化実装

#include "docksettings.h"
#include "settingscommon.h"
#include "settingskeys.h"
#include <QSettings>

namespace DockSettings {

// 評価値グラフドックの状態を取得
QByteArray evalChartDockState()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEvalChartDockState, QByteArray()).toByteArray();
}

// 評価値グラフドックの状態を保存
void setEvalChartDockState(const QByteArray& state)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEvalChartDockState, state);
}

// 評価値グラフドックのフローティング状態を取得
bool evalChartDockFloating()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEvalChartDockFloating, false).toBool();
}

// 評価値グラフドックのフローティング状態を保存
void setEvalChartDockFloating(bool floating)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEvalChartDockFloating, floating);
}

// 評価値グラフドックのジオメトリを取得
QByteArray evalChartDockGeometry()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEvalChartDockGeometry, QByteArray()).toByteArray();
}

// 評価値グラフドックのジオメトリを保存
void setEvalChartDockGeometry(const QByteArray& geometry)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEvalChartDockGeometry, geometry);
}

// 評価値グラフドックの表示状態を取得
bool evalChartDockVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kEvalChartDockVisible, true).toBool();
}

// 評価値グラフドックの表示状態を保存
void setEvalChartDockVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kEvalChartDockVisible, visible);
}

// 棋譜欄ドックのフローティング状態を取得
bool recordPaneDockFloating()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kRecordPaneDockFloating, false).toBool();
}

// 棋譜欄ドックのフローティング状態を保存
void setRecordPaneDockFloating(bool floating)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kRecordPaneDockFloating, floating);
}

// 棋譜欄ドックのジオメトリを取得
QByteArray recordPaneDockGeometry()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kRecordPaneDockGeometry, QByteArray()).toByteArray();
}

// 棋譜欄ドックのジオメトリを保存
void setRecordPaneDockGeometry(const QByteArray& geometry)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kRecordPaneDockGeometry, geometry);
}

// 棋譜欄ドックの表示状態を取得
bool recordPaneDockVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kRecordPaneDockVisible, true).toBool();
}

// 棋譜欄ドックの表示状態を保存
void setRecordPaneDockVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kRecordPaneDockVisible, visible);
}

// 解析タブドックのフローティング状態を取得
bool analysisTabDockFloating()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kAnalysisTabDockFloating, false).toBool();
}

// 解析タブドックのフローティング状態を保存
void setAnalysisTabDockFloating(bool floating)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kAnalysisTabDockFloating, floating);
}

// 解析タブドックのジオメトリを取得
QByteArray analysisTabDockGeometry()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kAnalysisTabDockGeometry, QByteArray()).toByteArray();
}

// 解析タブドックのジオメトリを保存
void setAnalysisTabDockGeometry(const QByteArray& geometry)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kAnalysisTabDockGeometry, geometry);
}

// 解析タブドックの表示状態を取得
bool analysisTabDockVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kAnalysisTabDockVisible, true).toBool();
}

// 解析タブドックの表示状態を保存
void setAnalysisTabDockVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kAnalysisTabDockVisible, visible);
}

// 将棋盤ドックのフローティング状態を取得
bool boardDockFloating()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kBoardDockFloating, false).toBool();
}

// 将棋盤ドックのフローティング状態を保存
void setBoardDockFloating(bool floating)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kBoardDockFloating, floating);
}

// 将棋盤ドックのジオメトリを取得
QByteArray boardDockGeometry()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kBoardDockGeometry, QByteArray()).toByteArray();
}

// 将棋盤ドックのジオメトリを保存
void setBoardDockGeometry(const QByteArray& geometry)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kBoardDockGeometry, geometry);
}

// 将棋盤ドックの表示状態を取得
bool boardDockVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kBoardDockVisible, true).toBool();
}

// 将棋盤ドックの表示状態を保存
void setBoardDockVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kBoardDockVisible, visible);
}

// メニューウィンドウドックのフローティング状態を取得
bool menuWindowDockFloating()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowDockFloating, false).toBool();
}

// メニューウィンドウドックのフローティング状態を保存
void setMenuWindowDockFloating(bool floating)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowDockFloating, floating);
}

// メニューウィンドウドックのジオメトリを取得
QByteArray menuWindowDockGeometry()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowDockGeometry, QByteArray()).toByteArray();
}

// メニューウィンドウドックのジオメトリを保存
void setMenuWindowDockGeometry(const QByteArray& geometry)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowDockGeometry, geometry);
}

// メニューウィンドウドックの表示状態を取得
bool menuWindowDockVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kMenuWindowDockVisible, false).toBool();  // デフォルトは非表示
}

// メニューウィンドウドックの表示状態を保存
void setMenuWindowDockVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kMenuWindowDockVisible, visible);
}

// 定跡ウィンドウドックのフローティング状態を取得
bool josekiWindowDockFloating()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowDockFloating, false).toBool();
}

// 定跡ウィンドウドックのフローティング状態を保存
void setJosekiWindowDockFloating(bool floating)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowDockFloating, floating);
}

// 定跡ウィンドウドックのジオメトリを取得
QByteArray josekiWindowDockGeometry()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowDockGeometry, QByteArray()).toByteArray();
}

// 定跡ウィンドウドックのジオメトリを保存
void setJosekiWindowDockGeometry(const QByteArray& geometry)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowDockGeometry, geometry);
}

// 定跡ウィンドウドックの表示状態を取得
bool josekiWindowDockVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kJosekiWindowDockVisible, false).toBool();  // デフォルトは非表示
}

// 定跡ウィンドウドックの表示状態を保存
void setJosekiWindowDockVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kJosekiWindowDockVisible, visible);
}

// 保存されているカスタムドックレイアウト名のリストを取得
QStringList savedDockLayoutNames()
{
    QSettings& s = SettingsCommon::openSettings();
    s.beginGroup(SettingsKeys::kCustomDockLayoutsGroup);
    QStringList names = s.childKeys();
    s.endGroup();
    return names;
}

// 指定した名前でドックレイアウトを保存
void saveDockLayout(const QString& name, const QByteArray& state)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name, state);
}

// 指定した名前のドックレイアウトを取得
QByteArray loadDockLayout(const QString& name)
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name, QByteArray()).toByteArray();
}

// 指定した名前のドックレイアウトを削除
void deleteDockLayout(const QString& name)
{
    QSettings& s = SettingsCommon::openSettings();
    s.remove(QString(SettingsKeys::kCustomDockLayoutsGroup) + "/" + name);
}

// 起動時に使用するレイアウト名を取得
QString startupDockLayoutName()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kStartupDockLayoutName, QString()).toString();
}

// 起動時に使用するレイアウト名を保存
void setStartupDockLayoutName(const QString& name)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kStartupDockLayoutName, name);
}

// 棋譜解析結果ドックのフローティング状態を取得
bool kifuAnalysisResultsDockFloating()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisResultsDockFloating, false).toBool();
}

// 棋譜解析結果ドックのフローティング状態を保存
void setKifuAnalysisResultsDockFloating(bool floating)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisResultsDockFloating, floating);
}

// 棋譜解析結果ドックのジオメトリを取得
QByteArray kifuAnalysisResultsDockGeometry()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisResultsDockGeometry, QByteArray()).toByteArray();
}

// 棋譜解析結果ドックのジオメトリを保存
void setKifuAnalysisResultsDockGeometry(const QByteArray& geometry)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisResultsDockGeometry, geometry);
}

// 棋譜解析結果ドックの表示状態を取得
bool kifuAnalysisResultsDockVisible()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kKifuAnalysisResultsDockVisible, false).toBool();  // デフォルトは非表示
}

// 棋譜解析結果ドックの表示状態を保存
void setKifuAnalysisResultsDockVisible(bool visible)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kKifuAnalysisResultsDockVisible, visible);
}

// 全ドックの固定設定を取得
bool docksLocked()
{
    QSettings& s = SettingsCommon::openSettings();
    return s.value(SettingsKeys::kDocksLocked, false).toBool();  // デフォルトは非固定
}

// 全ドックの固定設定を保存
void setDocksLocked(bool locked)
{
    QSettings& s = SettingsCommon::openSettings();
    s.setValue(SettingsKeys::kDocksLocked, locked);
}

} // namespace DockSettings
