#ifndef P2PNEGOTIATION_H
#define P2PNEGOTIATION_H

/// @file p2pnegotiation.h
/// @brief 開始までの交渉（HELLO・先後の決定・開始合意・開始署名。実装仕様 3.3節・5.4節・6.1節）

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

#include "p2pframes.h"
#include "p2psignature.h"

namespace P2p {

/// 交渉の条件（どちらの側か・自分の名乗り・招待した側の条件）
struct NegotiationConfig {
    quint8 role = kRoleApplicant;
    QByteArray networkId;
    QByteArray myPeerId;         ///< 自分の PeerId（バイナリ）
    QByteArray opponentPeerId;   ///< 通信プロセスが知らせた接続の相手（バイナリ）
    QString myDisplayName;
    SidePreference mySide = SidePreference::None;
    /// 招待した側の持ち時間（申込側は、招待した側の HELLO で初めて条件を知る）
    TimeControlTerms hostTerms;
    /// 今の時刻（UNIX秒）
    std::function<quint64()> nowSeconds;
    /// 招待した側: 申込みで名乗ったプレイヤーID がブロックした相手か
    std::function<bool(const QByteArray& playerKey)> isBlocked;
};

/// 交渉で分かった相手（HELLO の申告。開始署名を確かめるまで身元として扱わない）
struct OpponentHello {
    QString displayName;
    QByteArray playerKey;
    QByteArray sessionKey;
    QByteArray peerId;
    SidePreference sidePreference = SidePreference::None;
};

/**
 * @brief 1つの接続での開始までの交渉
 *
 * 受けたフレームを順に渡すと、送るフレーム（takeOutgoing）と段階が進む。ソケット・タイマー・UI に依存しない。
 * プレイヤー鍵で署名するのは confirm()（利用者が開始を押した）の後だけ。募集側は、申込側の開始署名を確かめるまで
 * 自分の開始署名を送らない。
 */
class Negotiation
{
public:
    enum class Stage {
        AwaitingHello,      ///< 相手の HELLO を待つ（申込側は送った後、募集側は受ける前）
        AwaitingAgreement,  ///< 募集側: START_AGREEMENT を待つ
        Confirming,         ///< 条件確認画面（開始を押すのを待つ）
        AwaitingSignature,  ///< 自分は押した。相手の開始署名を待つ
        Started,            ///< 両方の開始署名がそろった
        Failed,             ///< 取りやめ・拒否・検査の失敗
    };

    Negotiation(NegotiationConfig config, const SigningKey* playerKey);

    /// 申込側は HELLO を送る（募集側は何もしない）
    void begin();
    /// 受けたフレームを処理する
    void receive(const Frame& frame);
    /// 利用者が開始を押した
    void confirm();
    /// 取りやめる（開始前なら REJECT CANCELLED を送る）
    void cancel();

    std::vector<Frame> takeOutgoing();

    Stage stage() const { return m_stage; }
    /// 失敗の理由と、相手の REJECT によるものか
    std::optional<ErrorCode> failure() const { return m_failure; }
    bool failedByPeer() const { return m_failedByPeer; }
    /// 自分が開始を押したか
    bool confirmed() const { return m_confirmed; }

    const std::optional<OpponentHello>& opponent() const { return m_opponent; }
    /// 相手の開始署名を確かめたか（ここから相手のプレイヤーID を確かめた身元として扱う）
    bool opponentVerified() const { return !m_opponentStartSignature.isEmpty(); }
    /// 申込側の開始署名を確かめた（募集側はここで募集を取り下げる）

    const std::optional<StartAgreement>& agreement() const { return m_agreement; }
    TimeControlTerms terms() const { return m_terms; }
    bool iAmBlack() const { return m_iAmBlack; }
    /// 先後を双方の乱数で決めたか（希望で決めたなら false）
    bool sidesByCoin() const { return m_sidesByCoin; }
    QByteArray gameId() const { return m_agreement ? m_agreement->gameId() : QByteArray(); }
    QByteArray blackStartSignature() const { return m_iAmBlack ? m_myStartSignature : m_opponentStartSignature; }
    QByteArray whiteStartSignature() const { return m_iAmBlack ? m_opponentStartSignature : m_myStartSignature; }
    const SigningKey& sessionKey() const { return m_sessionKey; }
    /// 一局用鍵を対局へ渡す（Started の後に1回だけ）
    SigningKey takeSessionKey() { return std::move(m_sessionKey); }
    QByteArray myConnectionRandom() const { return m_myRandom; }
    QByteArray opponentConnectionRandom() const { return m_opponent ? m_opponentRandom : QByteArray(); }
    quint8 role() const { return m_config.role; }
    /// 次に送るフレームの requestId（対局へ引き継ぐ）
    quint64 nextRequestId() const { return m_requestId; }

    /// 先後の希望が両立するか（片方だけの希望、先手と後手の組）
    static bool sidesCompatible(SidePreference applicant, SidePreference host);

private:
    void send(FrameType type, const CborValue& payload, const QByteArray& gameId);
    void fail(ErrorCode code, bool sendReject);
    void handleHelloAsHost(const HelloPayload& hello);
    void handleHelloAsApplicant(const HelloPayload& hello);
    void handleAgreement(const StartAgreementPayload& payload);
    void handleStartSignature(const SideSignature& signature);
    void sendMyStartSignature();
    /// 申込側が先手か（希望、なければ双方の乱数）
    std::optional<bool> decideApplicantBlack(const QByteArray& nonceA, const QByteArray& nonceB) const;
    HelloPayload myHello() const;
    quint64 now() const { return m_config.nowSeconds ? m_config.nowSeconds() : 0; }

    NegotiationConfig m_config;
    const SigningKey* m_playerKey = nullptr;   ///< 非所有
    SigningKey m_sessionKey;
    Stage m_stage = Stage::AwaitingHello;
    std::optional<ErrorCode> m_failure;
    bool m_failedByPeer = false;
    bool m_confirmed = false;
    quint64 m_requestId = 1;
    std::vector<Frame> m_outgoing;

    QByteArray m_myRandom;
    QByteArray m_opponentRandom;
    QByteArray m_myNonce;        ///< 申込側 nonceA・募集側 nonceB
    QByteArray m_commitA;        ///< 募集側: 申込側の commit
    std::optional<OpponentHello> m_opponent;
    TimeControlTerms m_terms;
    std::optional<StartAgreement> m_agreement;
    bool m_iAmBlack = false;
    bool m_sidesByCoin = false;
    QByteArray m_myStartSignature;
    QByteArray m_opponentStartSignature;
};

} // namespace P2p

#endif // P2PNEGOTIATION_H
