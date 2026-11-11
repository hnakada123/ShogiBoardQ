#ifndef P2PKNOWNPEERS_H
#define P2PKNOWNPEERS_H

/// @file p2pknownpeers.h
/// @brief 既知の相手（プレイヤーID・手元で付けた名前・確認済みか・ブロック・対局数。実装仕様 3.4節・9章）

#include <QByteArray>
#include <QList>
#include <QString>

/// 既知の相手1人
struct P2pKnownPeer {
    QByteArray playerKey;          ///< プレイヤーID（公開鍵32バイト）
    QString localName;             ///< 利用者が付けた名前（棋譜・相手には出さない）
    QString lastDisplayName;       ///< 相手が最後に名乗った表示名
    bool verified = false;         ///< 確認コードを別の経路で見比べた
    bool blocked = false;
    quint32 games = 0;             ///< 開始した対局の数
    quint32 unconfirmed = 0;       ///< 結果が確定しなかった数
};

/**
 * @brief 既知の相手の記録（`p2p/peers.cbor`）
 *
 * 確認済みかどうかは手元の記録だけで決め、相手の申告を使わない。相手のプレイヤーID は署名で確かめてから
 * 記録する（申告だけの ID を記録しない）。
 */
class P2pKnownPeers
{
public:
    static constexpr int kMaxPeers = 10000;

    explicit P2pKnownPeers(const QString& path);

    bool load();
    bool save() const;

    QList<P2pKnownPeer> all() const { return m_peers; }
    /// プレイヤーID の記録（無ければ nullptr）
    const P2pKnownPeer* find(const QByteArray& playerKey) const;
    /// プレイヤーID の記録（無ければ作る）
    P2pKnownPeer& upsert(const QByteArray& playerKey);
    void remove(const QByteArray& playerKey);

    /// 名前を付けた相手のうち、同じ表示名（比べる形で）で別のプレイヤーID の相手
    QList<P2pKnownPeer> sameNameDifferentId(const QString& displayName, const QByteArray& playerKey) const;

private:
    QString m_path;
    QList<P2pKnownPeer> m_peers;
};

#endif // P2PKNOWNPEERS_H
