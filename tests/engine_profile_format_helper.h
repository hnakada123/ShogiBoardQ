#ifndef ENGINE_PROFILE_FORMAT_HELPER_H
#define ENGINE_PROFILE_FORMAT_HELPER_H

/// @file engine_profile_format_helper.h
/// @brief 登録エンジンの設定の保存形式（EngineProfileStore が読み書きする形）を、テストで直接書き読みするための語

#include <QString>

#include "settingscommon.h"

/**
 * @brief 設定ファイルの登録エンジンの語
 *
 * アプリでこの形を知るのは EngineProfileStore だけ。テストはここに書いた語で設定ファイルを準備し、
 * 保存した形を確かめる（以前の版が書いた設定ファイルを読めることの確認にもなる）。
 */
namespace EngineProfileFormat {

inline constexpr char EnginesGroupName[] = "Engines";
inline constexpr char EngineNameKey[] = "name";
inline constexpr char EnginePathKey[] = "path";

inline constexpr char EngineOptionNameKey[] = "name";
inline constexpr char EngineOptionTypeKey[] = "type";
inline constexpr char EngineOptionDefaultKey[] = "default";
inline constexpr char EngineOptionMinKey[] = "min";
inline constexpr char EngineOptionMaxKey[] = "max";
inline constexpr char EngineOptionValueKey[] = "value";

/// エンジンごとのグループの中の GUI の設定（先読みを使うか）
inline constexpr char EngineGuiPonderEnabledKey[] = "gui/ponderEnabled";

/// エンジンごとの設定のグループ（"EngineOptions/<エスケープした名前>"）
inline QString engineOptionsGroup(const QString& engineName)
{
    return QStringLiteral("EngineOptions/") + SettingsCommon::escapeKey(engineName);
}

} // namespace EngineProfileFormat

#endif // ENGINE_PROFILE_FORMAT_HELPER_H
