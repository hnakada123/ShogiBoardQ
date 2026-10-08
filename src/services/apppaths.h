#ifndef APPPATHS_H
#define APPPATHS_H

/// @file apppaths.h
/// @brief 設定・データ・キャッシュの置き場所（環境変数で別の場所へ切り替えられる）
///
/// 既定は Qt の標準の場所（QStandardPaths）。次の環境変数を指定すると、その下の
/// アプリ名のフォルダを使う（XDG の *_HOME と同じ意味。どの OS でも効く）。
///   SHOGIBOARDQ_CONFIG_HOME … 設定（ShogiBoardQ.ini・自動化のエンドポイント情報）
///   SHOGIBOARDQ_DATA_HOME   … データ（詰将棋の進捗など）
///   SHOGIBOARDQ_CACHE_HOME  … キャッシュ（駒音・詰将棋のキャッシュ）
/// macOS・Windows の Qt は XDG_CONFIG_HOME などを見ないため、テストや撮影で普段の設定を
/// 書き換えないよう、これらで隔離する。

#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>
#include <QString>

namespace AppPaths {

namespace detail {
inline QString location(const char* variable, QStandardPaths::StandardLocation standard)
{
    const QString base = qEnvironmentVariable(variable);
    if (base.isEmpty()) {
        return QStandardPaths::writableLocation(standard);
    }
    const QString name = QCoreApplication::applicationName().isEmpty()
        ? QStringLiteral("ShogiBoardQ") : QCoreApplication::applicationName();
    return QDir(base).filePath(name);
}
} // namespace detail

/// 設定ファイルのフォルダ
inline QString configDirectory()
{
    return detail::location("SHOGIBOARDQ_CONFIG_HOME", QStandardPaths::AppConfigLocation);
}

/// データのフォルダ
inline QString dataDirectory()
{
    return detail::location("SHOGIBOARDQ_DATA_HOME", QStandardPaths::AppDataLocation);
}

/// キャッシュのフォルダ
inline QString cacheDirectory()
{
    return detail::location("SHOGIBOARDQ_CACHE_HOME", QStandardPaths::CacheLocation);
}

} // namespace AppPaths

#endif // APPPATHS_H
