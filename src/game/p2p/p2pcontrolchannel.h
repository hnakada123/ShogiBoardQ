#ifndef P2PCONTROLCHANNEL_H
#define P2PCONTROLCHANNEL_H

/// @file p2pcontrolchannel.h
/// @brief 対局中の制御メッセージの通し番号・署名・1局の件数（実装仕様 4章・6.3節）

#include <QByteArray>

#include <utility>

#include "p2pframes.h"
#include "p2psignature.h"

namespace P2p {

/**
 * @brief 1つの接続の制御メッセージの通し番号と、1局の件数（規則だけを持つ値。送受信はしない）
 *
 * 送る側は番号を1つずつ増やして一局用鍵で署名する。受ける側は、別の対局・別の接続のものと同じ番号の再送を扱わず、
 * 番号の飛び・署名・1局の件数の上限を確かめる。番号は接続ごとに1から数え（再接続で数え直す）、件数は1局で数える。
 * いつ送るか・ジャーナルに書くか・中身をどう扱うかは、対局の状態機械（Match）が決める。
 */
class ControlChannel
{
public:
    /// 1局の制御メッセージの上限（送ったものと受けたものの合計。実装仕様 4章）
    static constexpr quint64 kMaxMessages = 4096;

    /// 受けた制御メッセージの扱い
    enum class Receipt {
        Accepted,      ///< 次の番号で、署名が正しい（番号を進めた）
        Ignored,       ///< 別の対局・別の接続のもの、または同じ接続での再送（扱わない）
        OutOfOrder,    ///< 番号が飛んだ
        BadSignature,  ///< 一局用鍵の署名が合わない
        OverLimit,     ///< 番号は進めたが、1局の件数の上限を超えた
    };

    ControlChannel(QByteArray gameId, QByteArray connectionId)
        : m_gameId(std::move(gameId))
        , m_connectionId(std::move(connectionId))
    {
    }

    const QByteArray& connectionId() const { return m_connectionId; }
    /// 新しい接続を始めた（connectionId が決まるまで、どの制御メッセージも受けない。番号は 0 から数え直す）
    void restart();
    /// 新しい接続の connectionId が決まった（再接続の HELLO を交わした）
    void setConnectionId(QByteArray connectionId) { m_connectionId = std::move(connectionId); }

    /// 送る制御メッセージを、次の番号で作って署名する（件数に数える）
    ControlPayload make(ControlKind kind, const CborValue& body, const SigningKey& sessionKey);
    /// 受けた制御メッセージを確かめる（Accepted と OverLimit のときだけ番号を進める）
    Receipt receive(const ControlPayload& payload, const QByteArray& opponentSessionKey);
    /// 1局の件数の上限を超えた
    bool isOverLimit() const { return m_count > kMaxMessages; }

private:
    QByteArray m_gameId;
    QByteArray m_connectionId;
    quint64 m_sent = 0;       ///< この接続で最後に送った番号
    quint64 m_received = 0;   ///< この接続で最後に受けた番号
    quint64 m_count = 0;      ///< 1局で送った・受けた数
};

} // namespace P2p

#endif // P2PCONTROLCHANNEL_H
