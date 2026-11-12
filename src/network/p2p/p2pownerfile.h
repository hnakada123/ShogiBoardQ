#ifndef P2POWNERFILE_H
#define P2POWNERFILE_H

/// @file p2pownerfile.h
/// @brief P2P対局の保存物の書き方（所有者だけが読み書きできる権限・原子的な書き込み。実装仕様 9章）

#include <QByteArray>
#include <QString>

/**
 * プレイヤー鍵・対局の記録・既知の相手は、所有者だけが読み書きできる権限で、途中で止まっても前の中身が残るように
 * 書く。この規則はここだけで決める。
 */
namespace P2pOwnerFile {

/// フォルダを作り（あれば何もしない）、所有者だけが使える権限にする
bool ensureDirectory(const QString& path);
/// 所有者だけが読み書きできる権限で、原子的に書く（書き終えてから置き換える）。置くフォルダも作る
bool writeAtomically(const QString& path, const QByteArray& bytes);

} // namespace P2pOwnerFile

#endif // P2POWNERFILE_H
