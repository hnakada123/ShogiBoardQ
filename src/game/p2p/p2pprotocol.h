#ifndef P2PPROTOCOL_H
#define P2PPROTOCOL_H

/// @file p2pprotocol.h
/// @brief P2P対局の条件・状態・開始合意・結果の値と、その決定的CBOR（protocol/game-frames.md 1章）

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <optional>
#include <vector>

#include "p2pcbor.h"

namespace P2p {

inline constexpr int kHashSize = 32;
inline constexpr int kNonceSize = 32;
inline constexpr int kPublicKeySize = 32;
inline constexpr int kSignatureSize = 64;
/// 1局の手数の上限（512手目を指したら終局）
inline constexpr quint32 kMaxPly = 512;
inline constexpr quint64 kMaxMainMs = 86'400'000;
inline constexpr quint64 kMaxByoyomiMs = 600'000;
inline constexpr int kMaxSfenBytes = 256;
/// 開始合意の期限（作成から）
inline constexpr quint64 kStartWindowSeconds = 180;

/// ゼロ32バイト（対局ID が決まる前の gameId・State0 の前状態ハッシュ）
QByteArray zeroHash();

/// 配列の値を、順序と型を確かめながら読む（1つでも外れれば ok() が false）
class CborReader
{
public:
    /// value が count 個の配列で、先頭が label の text、2番目が版 1 であることを確かめる（label が null なら見ない）
    CborReader(const CborValue& value, const char* label, std::size_t count);

    bool ok() const { return m_ok; }
    void fail() { m_ok = false; }

    quint64 uint(std::size_t index, quint64 maximum);
    QByteArray bytes(std::size_t index, int exactLength);
    QByteArray bytesUpTo(std::size_t index, int maxLength);
    /// ASCII の印字できる文字だけの text（maxLength バイトまで）
    QByteArray ascii(std::size_t index, int maxLength);
    /// UTF-8 の text（maxBytes バイトまで）
    QString utf8(std::size_t index, int maxBytes);
    /// 配列（maxCount 要素まで）
    const std::vector<CborValue>& array(std::size_t index, std::size_t maxCount);
    /// 要素そのもの（型を問わない）
    const CborValue& item(std::size_t index);
    /// index の要素が label の text か
    bool expectText(std::size_t index, const char* label);

private:
    const CborValue* at(std::size_t index);

    const CborValue& m_value;
    bool m_ok = true;
};

// ---------------------------------------------------------------- 条件

/// v1 のルール（平手・512手・千日手は引き分け・連続王手の千日手は負け・入玉宣言なし・人間対人間）
namespace Rules {
CborValue v1();
QByteArray v1Hash();
bool isV1(const CborValue& value);
/// 平手の初期局面（State0 の SFEN）
QByteArray initialSfen();
} // namespace Rules

/// 持ち時間（v1 は加算・遅延なし）
struct TimeControlTerms {
    quint64 mainMs = 600'000;
    quint64 byoyomiMs = 30'000;

    CborValue toCbor() const;
    QByteArray hash() const { return Cbor::hash(toCbor()); }
    bool isValid() const;
    static std::optional<TimeControlTerms> fromCbor(const CborValue& value);

    friend bool operator==(const TimeControlTerms& a, const TimeControlTerms& b)
    {
        return a.mainMs == b.mainMs && a.byoyomiMs == b.byoyomiMs;
    }
    friend bool operator!=(const TimeControlTerms& a, const TimeControlTerms& b) { return !(a == b); }
};

/// 通信の決まり（v1 は1つの値だけ）
struct NetworkPolicy {
    quint64 pingIntervalMs = 10'000;
    quint64 disconnectDetectMs = 30'000;
    quint64 reconnectWaitMs = 120'000;
    quint64 resendMs = 5'000;
    quint64 gameLimitMs = 86'400'000;
    quint64 toleranceMs = 10'000;
    quint64 disconnectTotalMs = 600'000;

    CborValue toCbor() const;
    QByteArray hash() const { return Cbor::hash(toCbor()); }
    static NetworkPolicy v1() { return NetworkPolicy{}; }
    /// v1 の値と同じときだけ読む
    static std::optional<NetworkPolicy> fromCbor(const CborValue& value);
};

// ---------------------------------------------------------------- 状態

/// 着手後の状態（State）
struct GameState {
    QByteArray gameId;
    quint32 ply = 0;
    QByteArray previousStateHash;
    QByteArray moveUsi;   ///< State0 は空
    QByteArray sfen;
    quint64 spentMs = 0;
    quint64 blackMainMs = 0;
    quint64 whiteMainMs = 0;

    CborValue toCbor() const;
    QByteArray hash() const { return Cbor::hash(toCbor()); }
    static std::optional<GameState> fromCbor(const CborValue& value);

    friend bool operator==(const GameState& a, const GameState& b) { return a.toCbor() == b.toCbor(); }
    friend bool operator!=(const GameState& a, const GameState& b) { return !(a == b); }
};

/// H(CBOR(Transcript))
QByteArray transcriptHash(const QByteArray& gameId, const std::vector<GameState>& states);

// ---------------------------------------------------------------- 開始合意

struct StartAgreement {
    QByteArray networkId;
    QByteArray blackNonce;
    QByteArray whiteNonce;
    QByteArray blackPlayerKey;
    QByteArray whitePlayerKey;
    QByteArray blackSessionKey;
    QByteArray whiteSessionKey;
    QByteArray blackPeerIdHash;
    QByteArray whitePeerIdHash;
    QByteArray rulesHash;
    QByteArray timeControlHash;
    QByteArray networkPolicyHash;
    quint64 startBefore = 0;

    CborValue toCbor() const;
    /// 対局ID（H(CBOR(StartAgreement))）
    QByteArray gameId() const { return Cbor::hash(toCbor()); }
    /// 4つの公開鍵・2つの PeerId のハッシュがそれぞれ異なるか
    bool hasDistinctKeys() const;
    static std::optional<StartAgreement> fromCbor(const CborValue& value);

    QByteArray playerKey(bool black) const { return black ? blackPlayerKey : whitePlayerKey; }
    QByteArray sessionKey(bool black) const { return black ? blackSessionKey : whiteSessionKey; }
    QByteArray peerIdHash(bool black) const { return black ? blackPeerIdHash : whitePeerIdHash; }
};

// ---------------------------------------------------------------- 署名の対象

/// StateApproval（一局用鍵で署名する）
CborValue stateApproval(const QByteArray& gameId, quint32 ply, const QByteArray& stateHash);

/// 制御メッセージの種類
enum class ControlKind : quint8 { Ready = 1, Resume = 2, ResultPropose = 3, Abort = 4, Error = 5, ResultDecline = 6 };

/// Control（一局用鍵で署名する）
struct Control {
    QByteArray gameId;
    QByteArray connectionId;
    quint64 sequence = 0;
    ControlKind kind = ControlKind::Ready;
    QByteArray bodyHash;

    CborValue toCbor() const;
    static std::optional<Control> fromCbor(const CborValue& value);
};

/// 終局の結果
enum class Outcome : quint8 { BlackWins = 1, WhiteWins = 2, Draw = 3, Aborted = 4 };
/// 終局の原因
enum class Cause : quint8 {
    Resign = 1, Checkmate = 2, Timeout = 3, Repetition = 4, PerpetualCheck = 5,
    AgreedDraw = 6, MaxPly = 7, AgreedAbort = 8
};
/// cause に許される outcome か（実装仕様 8章の表）
bool isAllowedOutcome(Cause cause, Outcome outcome);

/// FinalResult（プレイヤー鍵で署名する）
struct FinalResult {
    QByteArray gameId;
    QByteArray finalStateHash;
    QByteArray transcriptHash;
    quint32 ply = 0;
    Outcome outcome = Outcome::Draw;
    Cause cause = Cause::AgreedDraw;

    CborValue toCbor() const;
    /// 結果の識別子 resultDigest
    QByteArray digest() const { return Cbor::hash(toCbor()); }
    static std::optional<FinalResult> fromCbor(const CborValue& value);

    friend bool operator==(const FinalResult& a, const FinalResult& b) { return a.toCbor() == b.toCbor(); }
    friend bool operator!=(const FinalResult& a, const FinalResult& b) { return !(a == b); }
};

// ---------------------------------------------------------------- 識別子

/// このビルドが参加できるネットワーク（protocol/README.md 1.1節）
struct NetworkInfo {
    const char* name;
    bool local;        ///< ローカル検証用（loopback を使う。使用期限なし）
};
/// 参加できるネットワークの一覧
const std::vector<NetworkInfo>& allowedNetworks();
/// 一覧にある名前か
std::optional<NetworkInfo> findNetwork(const QString& name);

/// networkId = H(CBOR(["SBQ-NETWORK", 1, networkName]))
QByteArray networkId(const QByteArray& networkName);

/// 先後の決定の commit = H(CBOR(["SBQ-COMMIT", 1, nonceA]))
QByteArray coinCommit(const QByteArray& nonceA);
/// 申込側が先手か（H(CBOR(["SBQ-COIN", 1, nonceA, nonceB])) の最後のバイトの最下位ビットが0）
bool applicantIsBlack(const QByteArray& nonceA, const QByteArray& nonceB);
/// connectionId = H(CBOR(["SBQ-CONN", 1, gameId, blackRandom, whiteRandom]))
QByteArray connectionId(const QByteArray& gameId, const QByteArray& blackRandom, const QByteArray& whiteRandom);
/// 確認コード（5桁の数字6組）。2つの公開鍵の順によらない（実装仕様 3.4節）
QStringList verificationCode(const QByteArray& keyA, const QByteArray& keyB);

} // namespace P2p

#endif // P2PPROTOCOL_H
