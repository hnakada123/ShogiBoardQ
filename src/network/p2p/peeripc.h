#ifndef PEERIPC_H
#define PEERIPC_H

/// @file peeripc.h
/// @brief 通信プロセスとの IPC のメッセージの形（4バイト長＋JSON）、要求の作成、受けたメッセージの検査と読み取り
/// （protocol/ipc.md）。メソッド・通知・フィールドの名前と値の語は、このファイルだけが知る

#include <QByteArray>
#include <QJsonObject>
#include <QString>
#include <QStringList>

#include <optional>
#include <variant>

namespace PeerIpc {

/// IPC 1件の上限（128 KiB）
inline constexpr qsizetype kMaxMessageBytes = 128 * 1024;
inline constexpr int kProtocolVersion = 4;

/// 長さの前置きを付けたメッセージ
QByteArray frame(const QJsonObject& message);

/// 受けたメッセージの種類
struct Incoming {
    enum class Kind { Response, Error, Notification };
    Kind kind = Kind::Notification;
    quint64 generation = 0;
    quint64 requestId = 0;     ///< 応答・エラー
    QString name;              ///< 通知の名前
    QJsonObject body;          ///< result・error・通知の params
};

/**
 * @brief 受けた JSON を読み、外枠（版・世代・要求番号・知らないフィールド）を確かめる
 *
 * 中身（result・params）は名前ごとの検査（checkResult・checkNotification）で確かめる。
 */
std::optional<Incoming> parse(const QByteArray& json, QString* error = nullptr);

/// 10進の文字列の符号なし64ビット整数（先頭の 0・符号・空白を許さない）
std::optional<quint64> parseU64(const QJsonValue& value);

/**
 * @brief 要求 method の応答の result が、その要求の形に合うか
 *
 * 知らないフィールド・足りないフィールド・型や範囲の合わない値を拒否する（protocol/ipc.md 3章）。
 */
bool checkResult(const QString& method, const QJsonObject& result);

/// 通知の params が、その通知の形に合うか（protocol/ipc.md 4章）
bool checkNotification(const QString& name, const QJsonObject& params);

// ---------------------------------------------------------------- 要求（protocol/ipc.md 3章）

/// 送る要求（hello と shutdown は PeerProcess が送る）
struct Request {
    QString method;
    QJsonObject params;
};

namespace Requests {
Request start(const QString& networkName, const QStringList& bootstrap, bool relayOnly);
Request createInvite();
Request cancelInvites();
Request inspectInvite(const QString& invite);
Request connectInvite(const QString& invite, bool allowPrivate);
Request setGamePeer(const QString& peerId);
/// 対局の相手へ接続し直す
Request connectPeer(const QString& peerId);
Request sendFrame(quint64 connection, const QByteArray& frame);
Request closeConnection(quint64 connection);
} // namespace Requests

// ---------------------------------------------------------------- 応答（checkResult を通った result を読む）

struct StartResult {
    QByteArray peerIdBytes;  ///< バイナリ（文字列表記の peerId は形を確かめるだけで持たない）
    QByteArray networkId;
};

struct CreatedInvite {
    QString invite;
    QString code;            ///< 合言葉（口頭でも伝えられる短い形。同じ LAN の合言葉か、協力ノードを使う合言葉。出せないときは空）
    quint64 expiresAt = 0;
    int addressCount = 0;
};

/// 読んだものの種類
enum class InviteKind {
    Invite,    ///< 招待文字列
    LanCode,   ///< 同じ LAN の合言葉（相手の PeerId と期限は入っていないので、peerId は空、expiresAt は 0）
    NodeCode,  ///< 協力ノードを使う合言葉（協力ノードから引き取った招待の中身が入る）
};

/// 招待文字列・合言葉を読んだ結果（通信プロセスの解釈。対局・UI側は招待文字列も合言葉も読まない）
struct InspectedInvite {
    InviteKind kind = InviteKind::Invite;
    QString peerId;
    quint64 expiresAt = 0;
    int addressCount = 0;
    int usableAddressCount = 0;
    QStringList privateAddresses;   ///< profile が internet のときの私設アドレスの到達候補（利用者に見せて許可を求める）
};

/// 接続を始めた（connectInvite・connectPeer）。番号は通信プロセスが決める
struct ConnectionStarted {
    quint64 connection = 0;
};

/// 中身のない応答（cancelInvites・setGamePeer・sendFrame・closeConnection）
struct Done {};

using Reply = std::variant<StartResult, CreatedInvite, InspectedInvite, ConnectionStarted, Done>;
/// 要求 method への応答を読む（知らないメソッドは nullopt）
std::optional<Reply> readReply(const QString& method, const QJsonObject& result);

/// 招待の要求（inspectInvite・connectInvite）が断られた理由（protocol/ipc.md のエラーの code）。NotFound は、
/// 協力ノードにその合言葉の招待が無い（合言葉が違う・期限が過ぎた・もう引き取られた）、NoNode は、合言葉の
/// 待ち合わせに対応した協力ノードにつながっていない
enum class InviteRejection { Unreadable, Expired, WrongNetwork, Own, NotFound, NoNode };
/// 招待の要求への失敗なら、その理由（ほかの要求なら nullopt）
std::optional<InviteRejection> inviteRejection(const QString& method, const QString& code);

// ---------------------------------------------------------------- 通知（checkNotification を通った params を読む）

struct NetworkStatus {
    int connectedPeers = 0;
    QString reachability;    ///< unknown・public・private
    int relayReservations = 0;
};

struct ConnectionOpened {
    quint64 connection = 0;
    QString peerId;
    QByteArray peerIdBytes;
    bool viaRelay = false;
    bool outbound = false;       ///< 自分から開いた
    bool gameReconnect = false;  ///< 対局の相手がつなぎ直した（origin が game。招待の接続でない）
};

struct ConnectionRoute {
    quint64 connection = 0;
    bool viaRelay = false;
};

struct FrameReceived {
    quint64 connection = 0;
    QByteArray frame;
};

/// 接続が閉じた理由（protocol/ipc.md 4章）
enum class CloseReason { Closed, Reset, Timeout, Unreachable, NoRelayRoute, InvalidFrame, RateLimited, Limit };

struct ConnectionClosed {
    quint64 connection = 0;
    CloseReason reason = CloseReason::Closed;
};

using Notification = std::variant<NetworkStatus, ConnectionOpened, ConnectionRoute, FrameReceived, ConnectionClosed>;
/// 通知を読む（知らない名前は nullopt）
std::optional<Notification> readNotification(const QString& name, const QJsonObject& params);

} // namespace PeerIpc

#endif // PEERIPC_H
