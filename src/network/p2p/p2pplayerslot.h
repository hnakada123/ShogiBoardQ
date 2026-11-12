#ifndef P2PPLAYERSLOT_H
#define P2PPLAYERSLOT_H

/// @file p2pplayerslot.h
/// @brief この PC の何人目のプレイヤーか（同じ PC で動かす ShogiBoardQ ごとの保存先）

#include <QDir>
#include <QString>

/**
 * 同じ PC で ShogiBoardQ をいくつも動かすと、「P2P対局」を開いた順に、空いている最初の番号（1人目・2人目…）の
 * プレイヤーになる（P2pPlayerProfile::acquire）。保存先（ロック・プレイヤー鍵・対局記録・既知の相手）はプレイヤーごとに
 * 分ける。同じ保存先を同時に使えるアプリは1つで、プレイヤー鍵も保存先ごとに1つなので、同じ PC の中で対局を試せる。
 * アプリを1つだけ動かしているときは、いつも1人目になる。
 */
namespace P2pPlayerSlot {

/// 同じ PC で同時に「P2P対局」を使えるアプリの数
inline constexpr int kMaxPlayers = 8;

/// player 人目（1〜）の保存先。1人目は dataDirectory/p2p、2人目からは p2p-2・p2p-3…
inline QString storageDirectory(const QString& dataDirectory, int player)
{
    const QString name = player <= 1 ? QStringLiteral("p2p") : QStringLiteral("p2p-%1").arg(player);
    return QDir(dataDirectory).filePath(name);
}

} // namespace P2pPlayerSlot

#endif // P2PPLAYERSLOT_H
