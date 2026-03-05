#ifndef ENGINELISTSETTINGS_H
#define ENGINELISTSETTINGS_H

/// @file enginelistsettings.h
/// @brief 設定ファイルから登録エンジン一覧を読み込むユーティリティ

#include <QList>
#include <QString>

namespace EngineListSettings {

struct EngineEntry {
    QString name;
    QString path;
    QString author;
};

QList<EngineEntry> loadEngines();

} // namespace EngineListSettings

#endif // ENGINELISTSETTINGS_H
