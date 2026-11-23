/// @file executabledirectory.cpp
/// @brief 実行ファイルの実体があるフォルダ

#include "executabledirectory.h"

#include <QCoreApplication>
#include <QFileInfo>

QString ExecutableDirectory::path()
{
    const QFileInfo executable(QCoreApplication::applicationFilePath());
    // canonicalPath() はリンクをたどった先のフォルダを返す（たどれないときは空なので、リンクのあるフォルダにする）
    const QString canonical = executable.canonicalPath();
    return canonical.isEmpty() ? executable.absolutePath() : canonical;
}
