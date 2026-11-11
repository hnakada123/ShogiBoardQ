#ifndef P2PTRANSPORT_H
#define P2PTRANSPORT_H

/// @file p2ptransport.h
/// @brief P2P対局の司令塔がフレームを送る先（通信プロセスの窓口。試験では2つの司令塔を直接つなぐ）

#include <QByteArray>
#include <QString>

/**
 * @brief 対局ストリームの送り先
 *
 * 司令塔（P2pGameCoordinator）は通信プロセスを直接知らず、この窓口だけを使う。実装は募集・招待・接続を
 * 受け持つ P2pLobbyService。接続の番号は通信プロセスが決めた値（0 は無い接続）。
 */
class P2pTransport
{
public:
    virtual ~P2pTransport() = default;

    /// 自分の PeerId（バイナリ）
    virtual QByteArray localPeerId() const = 0;
    /// 参加しているネットワークの networkId
    virtual QByteArray networkId() const = 0;
    /// フレームを送る（送れなければ false）
    virtual bool sendFrame(quint64 connection, const QByteArray& frame) = 0;
    virtual void closeConnection(quint64 connection) = 0;
    /// 対局の相手（この PeerId からの対局ストリームだけを受け付ける。空で解除）
    virtual void setGamePeer(const QString& peerId) = 0;
    /// 対局の相手へ接続し直す（結果は接続が開いたときに知らせる）
    virtual void reconnectGamePeer(const QString& peerId) = 0;
    /// 募集を取り下げる（申込側の開始署名を確かめた時点。実装仕様 3.3節）
    virtual void withdrawOffer() = 0;
};

#endif // P2PTRANSPORT_H
