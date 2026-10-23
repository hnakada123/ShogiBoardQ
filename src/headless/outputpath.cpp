/// @file outputpath.cpp
/// @brief 書き出し先のファイルの事前検査の実装

#include "outputpath.h"

#include <QDir>
#include <QFileInfo>

std::optional<Failure> OutputPath::check(const QString& path, bool overwrite, const QString& overwriteHint)
{
    const QFileInfo info(path);
    if (path.isEmpty() || !info.isAbsolute())
        return Failure{ErrorKind::InvalidArgument, QStringLiteral("Output path must be absolute: %1").arg(path)};
    // シンボリックリンクには書かない（リンク先がまだ無いと、上書きの許可なしにリンク先に作ってしまう。セキュリティ監査 S15）
    if (info.isSymLink())
        return Failure{ErrorKind::InvalidArgument, QStringLiteral("Output path is a symbolic link: %1").arg(path)};
    if (info.exists() && !info.isFile())
        return Failure{ErrorKind::InvalidArgument, QStringLiteral("Output path is not a regular file: %1").arg(path)};
    if (info.exists() && !overwrite) {
        return Failure{ErrorKind::FileExists, QStringLiteral("Output file already exists: %1").arg(path),
                       QStringLiteral("Pass %1 to replace it").arg(overwriteHint)};
    }
    if (!info.dir().exists()) {
        return Failure{ErrorKind::NotFound, QStringLiteral("Output directory does not exist: %1").arg(info.dir().path())};
    }
    return std::nullopt;
}
