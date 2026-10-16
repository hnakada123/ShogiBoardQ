#ifndef DOCKSETTINGS_H
#define DOCKSETTINGS_H

/// @file docksettings.h
/// @brief ドック配置設定の永続化
///
/// ドックウィジェットの配置・表示状態・レイアウト管理に関する設定を提供します。
/// 呼び出し元: dockregistry.cpp, docklayoutmanager.cpp

#include <QString>
#include <QStringList>
#include <QByteArray>
#include "setting.h"

namespace DockSettings {

/// ドック1つの保存した状態（キーは「<グループ>/floating」「/visible」「/geometry」）
struct DockState {
    bool floating = false;
    bool visible = true;
    QByteArray geometry;  ///< フローティング時のウィンドウ位置・サイズ
};

/// group の保存した状態（保存していなければ表示状態は defaultVisible）
DockState dockState(const QString& group, bool defaultVisible);
void setDockState(const QString& group, const DockState& state);

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
