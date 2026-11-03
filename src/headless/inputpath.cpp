/// @file inputpath.cpp
/// @brief 読み込むファイルの事前検査の実装

#include "inputpath.h"

#include <QFileInfo>

std::optional<Failure> InputPath::check(const QString& path)
{
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isAbsolute())
        return Failure{ErrorKind::InvalidArgument, QStringLiteral("Input path must be absolute: %1").arg(path)};
    // シンボリックリンクはたどって確かめる（読むのはリンク先）
    if (!info.exists())
        return Failure{ErrorKind::NotFound, QStringLiteral("Input file does not exist: %1").arg(path)};
    if (!info.isFile())
        return Failure{ErrorKind::InvalidArgument, QStringLiteral("Input path is not a regular file: %1").arg(path)};
    return std::nullopt;
}
