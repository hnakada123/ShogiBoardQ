/// @file p2pplayerprofile.cpp
/// @brief この PC の1人のプレイヤーの保存先の実装

#include "p2pplayerprofile.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>

#include "p2pidentitystore.h"
#include "p2pknownpeers.h"
#include "p2pownerfile.h"
#include "p2pplayerslot.h"
#include "p2precordstore.h"

P2pPlayerProfile::P2pPlayerProfile(const QString& root, int playerNumber, QObject* parent)
    : QObject(parent)
    , m_root(root)
    , m_playerNumber(playerNumber)
    , m_records(std::make_unique<P2pRecordStore>(root))
    , m_knownPeers(std::make_unique<P2pKnownPeers>(QDir(root).filePath(QStringLiteral("peers.cbor"))))
    , m_identity(new P2pIdentityStore(identityDirectory(root), this))
{
}

P2pPlayerProfile::~P2pPlayerProfile()
{
    if (m_lock) m_lock->unlock();
}

P2pPlayerProfile* P2pPlayerProfile::acquire(const QString& dataDirectory, QObject* parent)
{
    for (int player = 1; player <= P2pPlayerSlot::kMaxPlayers; ++player) {
        auto profile = std::make_unique<P2pPlayerProfile>(P2pPlayerSlot::storageDirectory(dataDirectory, player), player);
        // ほかのアプリが使っている番号は飛ばす
        if (!profile->open()) continue;
        profile->setParent(parent);
        return profile.release();
    }
    return nullptr;
}

QString P2pPlayerProfile::identityDirectory(const QString& root)
{
    return QDir(root).filePath(QStringLiteral("identity"));
}

bool P2pPlayerProfile::open()
{
    if (isOpen()) return true;
    if (!P2pOwnerFile::ensureDirectory(m_root)) return false;
    // 同じ保存先を使えるアプリは同時に1つ（実装仕様 9章）。前のアプリが異常終了して残したロックは、そのプロセスが
    // もう無ければ取り直す
    m_lock = std::make_unique<QLockFile>(QDir(m_root).filePath(QStringLiteral("p2p.lock")));
    m_lock->setStaleLockTime(0);
    if (!m_lock->tryLock(0)) {
        m_lock.reset();
        return false;
    }
    // 以前の版が残した既知ピアのキャッシュ（ほかの参加者の IP アドレスが入る）は、もう使わないので消す。実験版
    // （アプリ名 ShogiBoardQ-P2P。同じ親フォルダの隣）が残したものも消す（鍵と記録は利用者のものなので触らない。
    // 監査 2026-11-13 I-l）
    QFile::remove(QDir(m_root).filePath(QStringLiteral("peers.cache")));
    const QFileInfo root(m_root);
    const QString legacyRoot = QDir(QFileInfo(root.absolutePath()).absolutePath())
                                   .filePath(QStringLiteral("ShogiBoardQ-P2P/") + root.fileName());
    QFile::remove(QDir(legacyRoot).filePath(QStringLiteral("peers.cache")));
    m_records->recoverJournals();
    m_knownPeers->load();
    return true;
}

void P2pPlayerProfile::close()
{
    m_identity->lock();
    if (m_lock) m_lock->unlock();
    m_lock.reset();
}

bool P2pPlayerProfile::isOpen() const
{
    return m_lock && m_lock->isLocked();
}
