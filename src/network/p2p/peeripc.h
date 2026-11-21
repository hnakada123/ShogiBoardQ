#ifndef PEERIPC_H
#define PEERIPC_H

/// @file peeripc.h
/// @brief 通信プロセスとの IPC のメッセージの形（4バイト長＋JSON）、要求の作成、受けたメッセージの読み取り
/// （protocol/ipc.md）。メソッド・通知・フィールドの名前と値の語は、このファイルだけが知る

#include <QByteArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QStringList>

#include <optional>
#include <variant>

namespace PeerIpc {

/// IPC 1件の上限（128 KiB）
inline constexpr qsizetype kMaxMessageBytes = 128 * 1024;
inline constexpr int kProtocolVersion = 6;

// ---------------------------------------------------------------- 要求（protocol/ipc.md 3章）

/// 要求のメソッド
enum class Method {
    Hello,
    Start,
    CreateInvite,
    CancelInvites,
    InspectInvite,
    ConnectInvite,
    SetGamePeer,
    ConnectPeer,
    SendFrame,
    CloseConnection,
    CreatePresence,
    CancelPresence,
    ConnectPresence,
    MeetPut,
    MeetGet,
    Shutdown,
};

/// メソッドの名前（IPC に書く語。ログと診断にも使う）
QLatin1String methodName(Method method);

/// 送る要求
struct Request {
    Method method = Method::Hello;
    QJsonObject params;
};

namespace Requests {
/// 起動の直後に送る（応答で、権限の制限の結果を確かめる）
Request hello(const QString& uiVersion);
/// 招待・在席の知らせを無効にして、終わらせる
Request shutdown();
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
// 友だち一覧（在席の知らせ）
/// 在席の知らせを作る（自分の接続先と、友だちの申し込み用のトークン。中身は読まない）
Request createPresence();
/// 作った在席の知らせをすべて無効にする
Request cancelPresence();
/// 友だちの在席の知らせ（協力ノードから引き取って復号したもの）の相手へ、申し込みの接続を始める
Request connectPresence(const QByteArray& presence, bool allowPrivate);
/// 番号と預けるもの（暗号化済み）を、合言葉に対応した協力ノードに預ける
Request meetPut(const QByteArray& slot, const QByteArray& blob, int ttlSeconds);
/// 番号で預けられているものを、合言葉に対応した協力ノードから引き取る
Request meetGet(const QByteArray& slot);
} // namespace Requests

/// 要求1件を、外枠（版・世代・要求番号）に入れ、長さの前置きを付けたメッセージにする。上限を超えるなら空
QByteArray encodeRequest(quint64 generation, quint64 requestId, const Request& request);

// ---------------------------------------------------------------- 受けたメッセージの外枠

/// 受信バッファの先頭にあるもの
enum class Framing {
    Message,     ///< 1件を取り出した
    Incomplete,  ///< まだ届いていない
    Violation,   ///< 長さが範囲の外（通信プロセスを使うのをやめる）
};

/// buffer の先頭から、長さの前置きの付いたメッセージを1件取り出す（取り出した分は buffer から除く）。
/// 長さを確かめてから読む
Framing takeMessage(QByteArray& buffer, QByteArray* json);

/// 受けたメッセージの外枠を読んだもの
struct Incoming {
    enum class Kind { Response, Error, Notification };
    Kind kind = Kind::Notification;
    quint64 generation = 0;
    quint64 requestId = 0;     ///< 応答・エラー
    QString notification;      ///< 通知の名前
    QString errorCode;         ///< エラーの code
    QJsonObject body;          ///< 応答の result・通知の params（readReply・readNotification で読む）
};

/// 受けた JSON の外枠（版・世代・要求番号・知らないフィールド）を確かめて読む
std::optional<Incoming> parse(const QByteArray& json, QString* error = nullptr);

/// 10進の文字列の符号なし64ビット整数（先頭の 0・符号・空白を許さない）
std::optional<quint64> parseU64(const QJsonValue& value);

// ---------------------------------------------------------------- 応答

/// 権限の制限の結果（hello の応答の sandbox）
struct Sandbox {
    bool applied = false;
    QStringList mechanisms;  ///< 掛かっている仕組みの名前（PeerConfinement::requiredMechanisms と照らす）
};

struct Hello {
    Sandbox sandbox;
};

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

/// 接続を始めた（connectInvite・connectPeer・connectPresence）。番号は通信プロセスが決める
struct ConnectionStarted {
    quint64 connection = 0;
};

/// 中身のない応答（cancelInvites・setGamePeer・sendFrame・closeConnection・cancelPresence・shutdown）
struct Done {};

/// 作った在席の知らせ（createPresence）。body は通信プロセスが作ったバイト列で、対局・UI側は読まずに暗号化する
struct CreatedPresence {
    QByteArray body;
    quint64 id = 0;            ///< 知らせの番号（友だちの申し込みが届いたときに、どの知らせかを知らせる）
    quint64 expiresAt = 0;
    int addressCount = 0;      ///< 入っている接続先の数（0 なら、相手はつなげない）
};

/// 協力ノードに預けた（meetPut）。stored は預かった協力ノードの数
struct MeetStored {
    int stored = 0;
};

/// 協力ノードから引き取った（meetGet）。協力ノードが返したもの（暗号化されたまま）
struct MeetFetched {
    QList<QByteArray> blobs;
};

using Reply = std::variant<Hello, StartResult, CreatedInvite, InspectedInvite, ConnectionStarted, Done, CreatedPresence,
                           MeetStored, MeetFetched>;

/**
 * @brief 要求 method の応答の result を、形を確かめながら読む
 *
 * 知らないフィールド・足りないフィールド・型や範囲の合わない値があれば nullopt（protocol/ipc.md 3章）。
 * 検査と読み取りは同じ1か所で行う（確かめた値だけが、型になって外へ出る）。
 */
std::optional<Reply> readReply(Method method, const QJsonObject& result);

/// 招待の要求（inspectInvite・connectInvite・connectPresence）が断られた理由（protocol/ipc.md のエラーの code）。
/// NotFound は、協力ノードにその合言葉の招待が無い（合言葉が違う・期限が過ぎた・もう引き取られた）、NoNode は、
/// 合言葉の待ち合わせに対応した協力ノードにつながっていない
enum class InviteRejection { Unreadable, Expired, WrongNetwork, Own, NotFound, NoNode };
/// 招待の要求への失敗なら、その理由（ほかの要求なら nullopt）
std::optional<InviteRejection> inviteRejection(Method method, const QString& errorCode);

// ---------------------------------------------------------------- 通知（protocol/ipc.md 4章）

/// 外から届くか（AutoNAT の判定）
enum class Reachability { Unknown, Public, Private };

struct NetworkStatus {
    int connectedPeers = 0;
    Reachability reachability = Reachability::Unknown;
    int relayReservations = 0;
    int meetNodes = 0;       ///< つながっている、合言葉の待ち合わせに対応した協力ノードの数
};

/// 開いた接続の出どころ
enum class Origin {
    Invite,  ///< 招待（招待文字列・合言葉）
    Friend,  ///< 友だちの申し込み（在席の知らせのトークン）
    Game,    ///< 対局の相手のつなぎ直し
};

struct ConnectionOpened {
    quint64 connection = 0;
    QString peerId;
    QByteArray peerIdBytes;
    bool viaRelay = false;
    bool outbound = false;       ///< 自分から開いた
    Origin origin = Origin::Invite;
    quint64 presence = 0;        ///< 友だちの申し込みで使われた在席の知らせの番号（origin が Friend のときだけ。ほかは 0）
};

struct ConnectionRoute {
    quint64 connection = 0;
    bool viaRelay = false;
};

struct FrameReceived {
    quint64 connection = 0;
    QByteArray frame;
};

/// 接続が閉じた理由
/// Refused は、開く前に相手が受け付けなかった（合言葉の確認の値が返らない・合わない。protocol/ipc.md 5章）
enum class CloseReason { Closed, Reset, Timeout, Unreachable, NoRelayRoute, InvalidFrame, RateLimited, Limit, Refused };

struct ConnectionClosed {
    quint64 connection = 0;
    CloseReason reason = CloseReason::Closed;
};

using Notification = std::variant<NetworkStatus, ConnectionOpened, ConnectionRoute, FrameReceived, ConnectionClosed>;

/// 通知の params を、形を確かめながら読む（知らない名前・形の合わない中身は nullopt）
std::optional<Notification> readNotification(const QString& name, const QJsonObject& params);

} // namespace PeerIpc

#endif // PEERIPC_H
