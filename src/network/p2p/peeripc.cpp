/// @file peeripc.cpp
/// @brief 通信プロセスとの IPC のメッセージの形と検査の実装

#include "peeripc.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

#include <functional>
#include <initializer_list>
#include <utility>

#include "p2pframes.h"
#include "p2pprotocol.h"

namespace PeerIpc {

namespace {

using Check = std::function<bool(const QJsonValue&)>;
using Fields = std::initializer_list<std::pair<const char*, Check>>;

/// obj のフィールドがちょうど fields で、それぞれの検査に通るか
bool exactFields(const QJsonObject& obj, Fields fields)
{
    if (obj.size() != static_cast<qsizetype>(fields.size())) return false;
    for (const auto& [name, check] : fields) {
        const auto it = obj.constFind(QLatin1String(name));
        if (it == obj.constEnd() || !check(it.value())) return false;
    }
    return true;
}

bool isPrintableAscii(const QString& text)
{
    for (const QChar c : text) {
        if (c.unicode() < 0x20 || c.unicode() > 0x7E) return false;
    }
    return true;
}

Check str(int maxLength)
{
    return [maxLength](const QJsonValue& v) {
        return v.isString() && v.toString().size() <= maxLength && isPrintableAscii(v.toString());
    };
}

Check oneOf(std::initializer_list<const char*> values)
{
    QStringList allowed;
    for (const char* v : values) allowed << QLatin1String(v);
    return [allowed](const QJsonValue& v) { return v.isString() && allowed.contains(v.toString()); };
}

Check hex32()
{
    return [](const QJsonValue& v) {
        if (!v.isString()) return false;
        const QString s = v.toString();
        if (s.size() != 64) return false;
        for (const QChar c : s) {
            if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9')) || (c >= QLatin1Char('a') && c <= QLatin1Char('f')))) {
                return false;
            }
        }
        return true;
    };
}

Check base64(int maxLength)
{
    return [maxLength](const QJsonValue& v) {
        if (!v.isString() || v.toString().size() > maxLength) return false;
        const auto decoded = QByteArray::fromBase64Encoding(v.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        return static_cast<bool>(decoded);
    };
}

Check u64()
{
    return [](const QJsonValue& v) { return parseU64(v).has_value(); };
}

Check integer(int minimum, int maximum)
{
    return [minimum, maximum](const QJsonValue& v) {
        if (!v.isDouble()) return false;
        const double d = v.toDouble();
        return d == static_cast<double>(static_cast<int>(d)) && d >= minimum && d <= maximum;
    };
}

Check boolean()
{
    return [](const QJsonValue& v) { return v.isBool(); };
}

Check stringArray(int maxCount, int maxLength)
{
    return [maxCount, maxLength](const QJsonValue& v) {
        if (!v.isArray() || v.toArray().size() > maxCount) return false;
        const QJsonArray array = v.toArray();
        for (const QJsonValue& item : array) {
            if (!item.isString() || item.toString().size() > maxLength || !isPrintableAscii(item.toString())) return false;
        }
        return true;
    };
}

Check objectWith(std::function<bool(const QJsonObject&)> check)
{
    return [check = std::move(check)](const QJsonValue& v) { return v.isObject() && check(v.toObject()); };
}

/// PeerId の文字列表記（base58btc の文字だけ）
Check peerIdText()
{
    return [](const QJsonValue& v) {
        static const QString alphabet = QStringLiteral("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz");
        if (!v.isString()) return false;
        const QString s = v.toString();
        if (s.isEmpty() || s.size() > 128) return false;
        for (const QChar c : s) {
            if (!alphabet.contains(c)) return false;
        }
        return true;
    };
}

/// フレームの上限（P2p::kMaxFrameBytes）の base64 の長さ
constexpr int kMaxFrameBase64 = static_cast<int>(((P2p::kMaxFrameBytes + 2) / 3) * 4);

// メソッドの名前（protocol/ipc.md 3章。hello と shutdown は PeerProcess が送る）
constexpr char kStart[] = "start";
constexpr char kCreateInvite[] = "createInvite";
constexpr char kCancelInvites[] = "cancelInvites";
constexpr char kInspectInvite[] = "inspectInvite";
constexpr char kConnectInvite[] = "connectInvite";
constexpr char kSetGamePeer[] = "setGamePeer";
constexpr char kConnectPeer[] = "connectPeer";
constexpr char kSendFrame[] = "sendFrame";
constexpr char kCloseConnection[] = "closeConnection";

// 通知の名前（protocol/ipc.md 4章）
constexpr char kNetworkStatus[] = "networkStatus";
constexpr char kConnectionOpened[] = "connectionOpened";
constexpr char kConnectionRoute[] = "connectionRoute";
constexpr char kFrameReceived[] = "frameReceived";
constexpr char kConnectionClosed[] = "connectionClosed";

/// 接続が閉じた理由の語
constexpr std::pair<CloseReason, const char*> kCloseReasons[] = {
    {CloseReason::Closed, "closed"},           {CloseReason::Reset, "reset"},
    {CloseReason::Timeout, "timeout"},         {CloseReason::Unreachable, "unreachable"},
    {CloseReason::NoRelayRoute, "no_relay_route"}, {CloseReason::InvalidFrame, "invalid_frame"},
    {CloseReason::RateLimited, "rate_limited"}, {CloseReason::Limit, "limit"},
};

Check closeReason()
{
    return [](const QJsonValue& v) {
        if (!v.isString()) return false;
        for (const auto& [reason, name] : kCloseReasons) {
            if (v.toString() == QLatin1String(name)) return true;
        }
        return false;
    };
}

/// 経路の語（direct・relay）
Check route()
{
    return oneOf({"direct", "relay"});
}

bool isRelay(const QJsonValue& v)
{
    return v.toString() == QLatin1String("relay");
}

quint64 connectionOf(const QJsonObject& obj)
{
    return parseU64(obj.value(QStringLiteral("connection"))).value_or(0);
}

QByteArray base64Of(const QJsonValue& v)
{
    return QByteArray::fromBase64(v.toString().toLatin1());
}

Request request(const char* method, QJsonObject params = QJsonObject())
{
    return Request{QLatin1String(method), std::move(params)};
}

} // namespace

QByteArray frame(const QJsonObject& message)
{
    const QByteArray json = QJsonDocument(message).toJson(QJsonDocument::Compact);
    QByteArray out;
    out.reserve(json.size() + 4);
    const auto length = static_cast<quint32>(json.size());
    out.append(static_cast<char>((length >> 24) & 0xFFu));
    out.append(static_cast<char>((length >> 16) & 0xFFu));
    out.append(static_cast<char>((length >> 8) & 0xFFu));
    out.append(static_cast<char>(length & 0xFFu));
    out.append(json);
    return out;
}

std::optional<quint64> parseU64(const QJsonValue& value)
{
    if (!value.isString()) return std::nullopt;
    const QString s = value.toString();
    if (s.isEmpty() || s.size() > 20 || (s.size() > 1 && s.startsWith(QLatin1Char('0')))) return std::nullopt;
    for (const QChar c : s) {
        if (c < QLatin1Char('0') || c > QLatin1Char('9')) return std::nullopt;
    }
    bool ok = false;
    const quint64 n = s.toULongLong(&ok);
    if (!ok) return std::nullopt;
    return n;
}

std::optional<Incoming> parse(const QByteArray& json, QString* error)
{
    const auto fail = [error](const QString& reason) {
        if (error) *error = reason;
        return std::nullopt;
    };
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) return fail(QStringLiteral("not a JSON object"));
    const QJsonObject obj = doc.object();
    if (obj.value(QStringLiteral("protocolVersion")) != QJsonValue(kProtocolVersion)) {
        return fail(QStringLiteral("unsupported protocolVersion"));
    }
    const std::optional<quint64> generation = parseU64(obj.value(QStringLiteral("processGeneration")));
    if (!generation) return fail(QStringLiteral("invalid processGeneration"));

    Incoming in;
    in.generation = *generation;
    if (obj.contains(QStringLiteral("notification"))) {
        if (obj.size() != 4 || !obj.value(QStringLiteral("notification")).isString()
            || !obj.value(QStringLiteral("params")).isObject()) {
            return fail(QStringLiteral("malformed notification"));
        }
        in.kind = Incoming::Kind::Notification;
        in.name = obj.value(QStringLiteral("notification")).toString();
        in.body = obj.value(QStringLiteral("params")).toObject();
        return in;
    }
    const std::optional<quint64> requestId = parseU64(obj.value(QStringLiteral("requestId")));
    if (!requestId || obj.size() != 4) return fail(QStringLiteral("malformed response"));
    in.requestId = *requestId;
    if (obj.value(QStringLiteral("result")).isObject()) {
        in.kind = Incoming::Kind::Response;
        in.body = obj.value(QStringLiteral("result")).toObject();
        return in;
    }
    const QJsonObject err = obj.value(QStringLiteral("error")).toObject();
    if (!exactFields(err, {{"code", str(32)}, {"message", str(512)}})) return fail(QStringLiteral("malformed error"));
    in.kind = Incoming::Kind::Error;
    in.body = err;
    return in;
}

bool checkResult(const QString& method, const QJsonObject& result)
{
    if (method == QLatin1String("hello")) {
        return exactFields(result, {{"peerVersion", str(64)},
                                    {"ipcVersion", integer(kProtocolVersion, kProtocolVersion)},
                                    {"sandbox", objectWith([](const QJsonObject& s) {
                                         return exactFields(s, {{"applied", boolean()},
                                                                {"os", str(16)},
                                                                {"mechanisms", stringArray(16, 32)},
                                                                {"landlockAbi", integer(0, 1000)}});
                                     })}});
    }
    if (method == QLatin1String(kStart)) {
        return exactFields(result, {{"peerId", peerIdText()},
                                    {"peerIdBytes", base64(128)},
                                    {"networkId", hex32()},
                                    {"profile", oneOf({"local", "internet"})}});
    }
    if (method == QLatin1String(kCreateInvite)) {
        return exactFields(result, {{"invite", str(1024)}, {"expiresAt", u64()}, {"addressCount", integer(0, 8)}});
    }
    if (method == QLatin1String(kInspectInvite)) {
        return exactFields(result, {{"peerId", peerIdText()},
                                    {"expiresAt", u64()},
                                    {"addressCount", integer(0, 8)},
                                    {"usableAddressCount", integer(0, 8)},
                                    {"privateAddresses", stringArray(8, 256)}});
    }
    if (method == QLatin1String(kConnectInvite) || method == QLatin1String(kConnectPeer)) {
        return exactFields(result, {{"connection", u64()}});
    }
    if (method == QLatin1String(kCancelInvites) || method == QLatin1String(kSetGamePeer)
        || method == QLatin1String(kSendFrame) || method == QLatin1String(kCloseConnection)
        || method == QLatin1String("shutdown")) {
        return result.isEmpty();
    }
    return false;
}

bool checkNotification(const QString& name, const QJsonObject& params)
{
    if (name == QLatin1String(kNetworkStatus)) {
        return exactFields(params, {{"state", oneOf({"connecting", "online", "offline"})},
                                    {"connectedPeers", integer(0, 100000)},
                                    {"reachability", oneOf({"unknown", "public", "private"})},
                                    {"relayReservations", integer(0, 1000)}});
    }
    if (name == QLatin1String(kConnectionOpened)) {
        return exactFields(params, {{"connection", u64()},
                                    {"peerId", peerIdText()},
                                    {"peerIdBytes", base64(128)},
                                    {"route", route()},
                                    {"direction", oneOf({"inbound", "outbound"})},
                                    {"origin", oneOf({"invite", "game"})}});
    }
    if (name == QLatin1String(kConnectionRoute)) {
        return exactFields(params, {{"connection", u64()}, {"route", route()}});
    }
    if (name == QLatin1String(kFrameReceived)) {
        return exactFields(params, {{"connection", u64()}, {"frame", base64(kMaxFrameBase64)}});
    }
    if (name == QLatin1String(kConnectionClosed)) {
        return exactFields(params, {{"connection", u64()}, {"reason", closeReason()}});
    }
    return false;
}

// ---------------------------------------------------------------- 要求

namespace Requests {

Request start(const QString& networkName, const QStringList& bootstrap, bool relayOnly)
{
    QJsonObject params;
    params[QStringLiteral("networkName")] = networkName;
    params[QStringLiteral("bootstrap")] = QJsonArray::fromStringList(bootstrap);
    params[QStringLiteral("relayOnly")] = relayOnly;
    return request(kStart, params);
}

Request createInvite()
{
    return request(kCreateInvite);
}

Request cancelInvites()
{
    return request(kCancelInvites);
}

Request inspectInvite(const QString& invite)
{
    QJsonObject params;
    params[QStringLiteral("invite")] = invite;
    return request(kInspectInvite, params);
}

Request connectInvite(const QString& invite, bool allowPrivate)
{
    QJsonObject params;
    params[QStringLiteral("invite")] = invite;
    params[QStringLiteral("allowPrivate")] = allowPrivate;
    return request(kConnectInvite, params);
}

Request setGamePeer(const QString& peerId)
{
    QJsonObject params;
    params[QStringLiteral("peerId")] = peerId;
    return request(kSetGamePeer, params);
}

Request connectPeer(const QString& peerId)
{
    QJsonObject params;
    params[QStringLiteral("peerId")] = peerId;
    return request(kConnectPeer, params);
}

Request sendFrame(quint64 connection, const QByteArray& frame)
{
    QJsonObject params;
    params[QStringLiteral("connection")] = QString::number(connection);
    params[QStringLiteral("frame")] = QString::fromLatin1(frame.toBase64());
    return request(kSendFrame, params);
}

Request closeConnection(quint64 connection)
{
    QJsonObject params;
    params[QStringLiteral("connection")] = QString::number(connection);
    return request(kCloseConnection, params);
}

} // namespace Requests

// ---------------------------------------------------------------- 応答と通知の読み取り

std::optional<Reply> readReply(const QString& method, const QJsonObject& result)
{
    if (method == QLatin1String(kStart)) {
        return StartResult{base64Of(result.value(QStringLiteral("peerIdBytes"))),
                           QByteArray::fromHex(result.value(QStringLiteral("networkId")).toString().toLatin1())};
    }
    if (method == QLatin1String(kCreateInvite)) {
        return CreatedInvite{result.value(QStringLiteral("invite")).toString(),
                             parseU64(result.value(QStringLiteral("expiresAt"))).value_or(0),
                             result.value(QStringLiteral("addressCount")).toInt()};
    }
    if (method == QLatin1String(kInspectInvite)) {
        InspectedInvite info;
        info.peerId = result.value(QStringLiteral("peerId")).toString();
        info.expiresAt = parseU64(result.value(QStringLiteral("expiresAt"))).value_or(0);
        info.addressCount = result.value(QStringLiteral("addressCount")).toInt();
        info.usableAddressCount = result.value(QStringLiteral("usableAddressCount")).toInt();
        const QJsonArray addresses = result.value(QStringLiteral("privateAddresses")).toArray();
        for (const QJsonValue& address : addresses) info.privateAddresses << address.toString();
        return info;
    }
    if (method == QLatin1String(kConnectInvite) || method == QLatin1String(kConnectPeer)) {
        return ConnectionStarted{connectionOf(result)};
    }
    if (method == QLatin1String(kCancelInvites) || method == QLatin1String(kSetGamePeer)
        || method == QLatin1String(kSendFrame) || method == QLatin1String(kCloseConnection)) {
        return Done{};
    }
    return std::nullopt;
}

std::optional<InviteRejection> inviteRejection(const QString& method, const QString& code)
{
    if (method != QLatin1String(kInspectInvite) && method != QLatin1String(kConnectInvite)) return std::nullopt;
    if (code == QLatin1String("invite_expired")) return InviteRejection::Expired;
    if (code == QLatin1String("invite_wrong_network")) return InviteRejection::WrongNetwork;
    if (code == QLatin1String("invite_own")) return InviteRejection::Own;
    return InviteRejection::Unreadable;
}

std::optional<Notification> readNotification(const QString& name, const QJsonObject& params)
{
    if (name == QLatin1String(kNetworkStatus)) {
        return NetworkStatus{params.value(QStringLiteral("connectedPeers")).toInt(),
                             params.value(QStringLiteral("reachability")).toString(),
                             params.value(QStringLiteral("relayReservations")).toInt()};
    }
    if (name == QLatin1String(kConnectionOpened)) {
        ConnectionOpened opened;
        opened.connection = connectionOf(params);
        opened.peerId = params.value(QStringLiteral("peerId")).toString();
        opened.peerIdBytes = base64Of(params.value(QStringLiteral("peerIdBytes")));
        opened.viaRelay = isRelay(params.value(QStringLiteral("route")));
        opened.outbound = params.value(QStringLiteral("direction")).toString() == QLatin1String("outbound");
        opened.gameReconnect = params.value(QStringLiteral("origin")).toString() == QLatin1String("game");
        return opened;
    }
    if (name == QLatin1String(kConnectionRoute)) {
        return ConnectionRoute{connectionOf(params), isRelay(params.value(QStringLiteral("route")))};
    }
    if (name == QLatin1String(kFrameReceived)) {
        return FrameReceived{connectionOf(params), base64Of(params.value(QStringLiteral("frame")))};
    }
    if (name == QLatin1String(kConnectionClosed)) {
        ConnectionClosed closed{connectionOf(params), CloseReason::Closed};
        const QString reason = params.value(QStringLiteral("reason")).toString();
        for (const auto& [value, text] : kCloseReasons) {
            if (reason == QLatin1String(text)) closed.reason = value;
        }
        return closed;
    }
    return std::nullopt;
}

} // namespace PeerIpc
