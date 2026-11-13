/// @file p2pownerfile.cpp
/// @brief P2P対局の保存物の書き方の実装

#include "p2pownerfile.h"

#include <QDir>
#include <QFileInfo>
#include <QSaveFile>

#ifdef Q_OS_UNIX
#include <unistd.h>
#endif

namespace P2pOwnerFile {

bool ensureDirectory(const QString& path)
{
    if (!QDir().mkpath(path)) return false;
    if (!QFile::setPermissions(path, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner)) {
        return false;
    }
#ifdef Q_OS_UNIX
    // 自分のフォルダで、ほかの利用者が読み書きできないこと（権限を持てないファイルシステム・他人のフォルダを使わない）。
    // Windows は利用者のフォルダの ACL に任せる
    const QFileInfo info(path);
    if (info.isSymLink() || info.ownerId() != ::getuid()) return false;
    const QFileDevice::Permissions others = QFileDevice::ReadGroup | QFileDevice::WriteGroup | QFileDevice::ExeGroup
        | QFileDevice::ReadOther | QFileDevice::WriteOther | QFileDevice::ExeOther;
    if (info.permissions() & others) return false;
#endif
    return true;
}

bool writeAtomically(const QString& path, const QByteArray& bytes)
{
    if (!ensureDirectory(QFileInfo(path).absolutePath())) return false;
    QSaveFile file(path);
    // 書く中身（合言葉なしの鍵のファイルなら種そのもの）を、Qt の内部の書き込み用の領域に写さない
    if (!file.open(QIODevice::WriteOnly | QIODevice::Unbuffered)) return false;
    if (!file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner) || file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return false;
    }
    return file.commit();
}

} // namespace P2pOwnerFile
