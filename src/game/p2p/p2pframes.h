#ifndef P2PFRAMES_H
#define P2PFRAMES_H

/// @file p2pframes.h
/// @brief 対局フレームの外枠と payload（protocol/README.md 2章・protocol/game-frames.md 2章・3章）

#include <QByteArray>
#include <QByteArrayView>
#include <QString>

#include <optional>
#include <vector>

#include "p2pevidence.h"
#include "p2pprotocol.h"

namespace P2p {

/// フレーム1件の上限（64 KiB）
inline constexpr qsizetype kMaxFrameBytes = 64 * 1024;
/// PeerId のバイナリの長さの上限
inline constexpr int kMaxPeerIdBytes = 64;
/// SYNC_CHUNK 1回の状態の数の上限
inline constexpr std::size_t kMaxSyncChunkStates = 64;

enum class FrameType : quint16 {
    Hello = 1, StartAgreement = 2, StartSignature = 3, StatePropose = 4, StateAccept = 5, Control = 6,
    SyncRequest = 7, SyncChunk = 8, ResultSignature = 9, Reject = 10, InviteToken = 11,
};

/// 開始前の拒否・対局中のエラー・中断の理由（protocol/game-frames.md 2章）
enum class ErrorCode : quint8 {
    UnsupportedVersion = 1, WrongNetwork = 2, Busy = 3, Expired = 4, ConditionMismatch = 5,
    InvalidSignature = 6, InvalidMove = 7, StateConflict = 8, ClockMismatch = 9, LimitExceeded = 10,
    StorageError = 11, Cancelled = 12, DisconnectLimit = 13, GameTimeLimit = 14, ClockDiscontinuity = 15,
};
inline constexpr quint8 kMaxErrorCode = 15;

/// 先後（フレームの side。1=先手、2=後手）
inline constexpr quint8 kSideBlack = 1;
inline constexpr quint8 kSideWhite = 2;
/// 申込側・募集側（HELLO の role）
inline constexpr quint8 kRoleApplicant = 1;
inline constexpr quint8 kRoleHost = 2;
/// 先後の希望（0=指定なし、1=先手、2=後手）
enum class SidePreference : quint8 { None = 0, Black = 1, White = 2 };

/// フレームの外枠
struct Frame {
    FrameType type = FrameType::Hello;
    quint64 requestId = 0;
    QByteArray gameId;
    CborValue payload;   ///< 配列

    QByteArray encode() const;
    /// 上限・決定的CBOR・外枠を確かめて読む。INVITE_TOKEN は対局・UI側に届かないはずなので拒否する
    static std::optional<Frame> decode(QByteArrayView bytes, QString* error = nullptr);
    /// 外枠の規則（protocol/README.md 2章。type は 1〜11）に合うか。通信プロセスと同じ判定で、decode も同じ規則で読む
    static bool isWellFormedEnvelope(QByteArrayView bytes);
};

/// HELLO（開始前）
struct HelloPayload {
    quint8 role = kRoleApplicant;
    QByteArray networkId;
    QByteArray peerId;
    QByteArray connectionRandom;
    QByteArray playerKey;
    QByteArray sessionKey;
    QString displayName;
    SidePreference sidePreference = SidePreference::None;
    QByteArray offerId;      ///< 招待ではゼロ32バイト
    QByteArray coin;         ///< 申込側は commitA、募集側は nonceB
    /// 募集側だけが入れる持ち時間（Rules と NetworkPolicy は v1 の値）
    std::optional<TimeControlTerms> timeControl;

    CborValue toCbor() const;
    /// 形・表示名の規則・v1 の条件を確かめて読む（role に合わない条件の有無も拒否する）
    static std::optional<HelloPayload> fromCbor(const CborValue& value);
};

/// HELLO（再接続）
struct ReconnectHello {
    quint8 side = kSideBlack;
    QByteArray connectionRandom;

    CborValue toCbor() const;
    static std::optional<ReconnectHello> fromCbor(const CborValue& value);
};

struct StartAgreementPayload {
    StartAgreement agreement;
    TimeControlTerms timeControl;

    CborValue toCbor() const;
    static std::optional<StartAgreementPayload> fromCbor(const CborValue& value);
};

/// START_SIGNATURE・RESULT_SIGNATURE の側と署名
struct SideSignature {
    quint8 side = kSideBlack;
    QByteArray signature;
};

CborValue startSignaturePayload(const SideSignature& s);
std::optional<SideSignature> parseStartSignature(const CborValue& value);

struct StateProposePayload {
    GameState state;
    QByteArray signature;

    CborValue toCbor() const;
    static std::optional<StateProposePayload> fromCbor(const CborValue& value);
};

struct StateAcceptPayload {
    quint32 ply = 0;
    QByteArray stateHash;
    QByteArray signature;

    CborValue toCbor() const;
    static std::optional<StateAcceptPayload> fromCbor(const CborValue& value);
};

struct ControlPayload {
    Control control;
    CborValue body;
    QByteArray signature;

    CborValue toCbor() const;
    /// 形と bodyHash の一致を確かめて読む（署名は呼ぶ側が確かめる）
    static std::optional<ControlPayload> fromCbor(const CborValue& value);
};

struct SyncRequestPayload {
    quint32 fromPly = 0;
    QByteArray previousStateHash;

    CborValue toCbor() const;
    static std::optional<SyncRequestPayload> fromCbor(const CborValue& value);
};

struct SyncChunkPayload {
    std::vector<SignedState> states;
    bool final = true;

    CborValue toCbor() const;
    static std::optional<SyncChunkPayload> fromCbor(const CborValue& value);
};

struct ResultSignaturePayload {
    FinalResult result;
    SideSignature signature;

    CborValue toCbor() const;
    static std::optional<ResultSignaturePayload> fromCbor(const CborValue& value);
};

CborValue rejectPayload(ErrorCode code);
std::optional<ErrorCode> parseReject(const CborValue& value);

/// CONTROL の body
namespace ControlBody {
CborValue ready(const QByteArray& state0Hash);
CborValue resume(quint32 latestPly, const QByteArray& latestStateHash);
CborValue abort(const QByteArray& latestStateHash, ErrorCode reason);
CborValue error(ErrorCode code, quint32 relatedPly);
CborValue decline(const QByteArray& proposalBodyHash);

std::optional<QByteArray> parseReady(const CborValue& body);
struct Resume { quint32 latestPly = 0; QByteArray latestStateHash; };
std::optional<Resume> parseResume(const CborValue& body);
struct Abort { QByteArray latestStateHash; ErrorCode reason = ErrorCode::Cancelled; };
std::optional<Abort> parseAbort(const CborValue& body);
struct Error { ErrorCode code = ErrorCode::Cancelled; quint32 relatedPly = 0; };
std::optional<Error> parseError(const CborValue& body);
std::optional<QByteArray> parseDecline(const CborValue& body);
} // namespace ControlBody

} // namespace P2p

#endif // P2PFRAMES_H
