#ifndef MOCK_ENGINE_PATH_H
#define MOCK_ENGINE_PATH_H

/// @file mock_engine_path.h
/// @brief テストの実行ファイルと同じ場所にある試験用エンジンの場所

#include <QCoreApplication>
#include <QString>

namespace MockEnginePath {

/// 試験用の実行ファイル name の場所。Windows では拡張子 .exe を付ける
/// （エンジンの起動は QFileInfo::isExecutable で確かめ、Windows では拡張子の無いファイルを実行できると見なさない）
inline QString of(const QString& name)
{
    QString path = QCoreApplication::applicationDirPath() + QLatin1Char('/') + name;
#ifdef Q_OS_WIN
    path += QStringLiteral(".exe");
#endif
    return path;
}

/// USI の対局・解析に答える試験用エンジン mock_usi_match の場所
inline QString usiMatch()
{
    return of(QStringLiteral("mock_usi_match"));
}

} // namespace MockEnginePath

#endif // MOCK_ENGINE_PATH_H
