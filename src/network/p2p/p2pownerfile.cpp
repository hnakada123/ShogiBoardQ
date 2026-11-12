/// @file p2pownerfile.cpp
/// @brief P2P対局の保存物の書き方の実装

#include "p2pownerfile.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

namespace P2pOwnerFile {

bool ensureDirectory(const QString& path)
{
    if (!QDir().mkpath(path)) return false;
    QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return true;
}

bool writeAtomically(const QString& path, const QByteArray& bytes)
{
    if (!ensureDirectory(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) return false;
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace P2pOwnerFile
