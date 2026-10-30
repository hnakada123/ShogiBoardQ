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

/// 利用者が付けた名前（エンジン名・ドックのレイアウト名）を設定のキーの1つの区切りにする。
/// 「/」「\」はグループの区切りになるので、「%」とともに %2F・%5C・%25 にする（戻すのは unescapeKey）
QString escapeKey(const QString& name);
/// escapeKey の逆
QString unescapeKey(const QString& key);

/// 全設定を消去して同期し、各 getter が既定値を返す状態に戻します。
/// 終了時の設定保存がすべて完了した後に呼び出してください。
[[nodiscard]] bool resetAllSettings();

} // namespace SettingsCommon

#endif // SETTINGSCOMMON_H
