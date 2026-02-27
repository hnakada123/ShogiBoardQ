#ifndef SETTINGSCOMMON_H
#define SETTINGSCOMMON_H

/// @file settingscommon.h
/// @brief 設定サービスの共通インフラストラクチャ
///
/// 全ドメイン設定クラスが共有する QSettings アクセス基盤を提供します。

#include <QString>

class QSettings;

namespace SettingsCommon {

/// 設定ファイル（ShogiBoardQ.ini）のフルパスを返します。
/// QStandardPaths::AppConfigLocation を使用してプラットフォームごとの標準的な保存先に配置します。
/// - macOS: ~/Library/Preferences/ShogiBoardQ/ShogiBoardQ.ini
/// - Linux: ~/.config/ShogiBoardQ/ShogiBoardQ.ini
/// - Windows: C:/Users/<user>/AppData/Local/ShogiBoardQ/ShogiBoardQ.ini
QString settingsFilePath();

/// 共有 QSettings インスタンスへの参照を返します。
/// 各ドメイン設定の実装から使用されます。
QSettings& openSettings();

} // namespace SettingsCommon

#endif // SETTINGSCOMMON_H
