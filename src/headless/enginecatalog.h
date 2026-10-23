#ifndef ENGINECATALOG_H
#define ENGINECATALOG_H

/// @file enginecatalog.h
/// @brief 設定ファイルに登録された USI エンジンの名前解決（自動化・CLI 用）

#include <QJsonArray>
#include <QString>
#include <optional>

#include "enginelistsettings.h"

/**
 * @brief 登録済みエンジンの名前解決と一覧の JSON 化
 *
 * 自動化 API と CLI は任意の実行ファイルパスを受け付けず、
 * `EngineListSettings::loadEngines()` にある名前だけをエンジンとして使う。
 */
namespace EngineCatalog {

/// 名前が一致する登録エンジンを返す。見つからなければ nullopt を返し、error に候補名を含む説明を入れる
std::optional<EngineListSettings::EngineEntry> findByName(const QString& name, QString* error = nullptr);

/// 登録エンジン一覧を JSON 配列にする（name, path, author, exists）
QJsonArray toJson();

} // namespace EngineCatalog

#endif // ENGINECATALOG_H
