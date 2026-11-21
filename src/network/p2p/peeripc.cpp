/// @file peeripc.cpp
/// @brief 通信プロセスとの IPC のメッセージの形と読み取りの実装

#include "peeripc.h"

#include <QJsonArray>
#include <QJsonDocument>

#include <cstddef>
#include <initializer_list>
#include <utility>

#include "p2pframes.h"

namespace PeerIpc {

namespace {

/// IPC に書く語と、その値
template <typename T>
struct Word {
    const char* text;
    T value;
};

// メソッドの名前（protocol/ipc.md 3章）
constexpr Word<Method> kMethods[] = {
    {"hello", Method::Hello},
    {"start", Method::Start},
    {"createInvite", Method::CreateInvite},
    {"cancelInvites", Method::CancelInvites},
    {"inspectInvite", Method::InspectInvite},
    {"connectInvite", Method::ConnectInvite},
    {"setGamePeer", Method::SetGamePeer},
    {"connectPeer", Method::ConnectPeer},
    {"sendFrame", Method::SendFrame},
    {"closeConnection", Method::CloseConnection},
    {"createPresence", Method::CreatePresence},
    {"cancelPresence", Method::CancelPresence},
    {"connectPresence", Method::ConnectPresence},
    {"meetPut", Method::MeetPut},
    {"meetGet", Method::MeetGet},
    {"shutdown", Method::Shutdown},
};

// 通知の名前（protocol/ipc.md 4章）
constexpr char kNetworkStatus[] = "networkStatus";
constexpr char kConnectionOpened[] = "connectionOpened";
constexpr char kConnectionRoute[] = "connectionRoute";
constexpr char kFrameReceived[] = "frameReceived";
constexpr char kConnectionClosed[] = "connectionClosed";

// 値の語
constexpr Word<InviteKind> kInviteKinds[] = {
    {"invite", InviteKind::Invite},
    {"code", InviteKind::LanCode},
    {"meetCode", InviteKind::NodeCode},
};
constexpr Word<Reachability> kReachabilities[] = {
    {"unknown", Reachability::Unknown},
    {"public", Reachability::Public},
    {"private", Reachability::Private},
};
constexpr Word<Origin> kOrigins[] = {
    {"invite", Origin::Invite},
    {"friend", Origin::Friend},
    {"game", Origin::Game},
};
/// 経路（値は、中継を通るか）
constexpr Word<bool> kRoutes[] = {{"direct", false}, {"relay", true}};
/// 向き（値は、自分から開いたか）
constexpr Word<bool> kDirections[] = {{"inbound", false}, {"outbound", true}};
constexpr Word<CloseReason> kCloseReasons[] = {
    {"closed", CloseReason::Closed},
    {"reset", CloseReason::Reset},
    {"timeout", CloseReason::Timeout},
    {"unreachable", CloseReason::Unreachable},
    {"no_relay_route", CloseReason::NoRelayRoute},
    {"invalid_frame", CloseReason::InvalidFrame},
    {"rate_limited", CloseReason::RateLimited},
    {"limit", CloseReason::Limit},
    {"refused", CloseReason::Refused},
};
/// 招待の要求が断られた理由（表にない code は Unreadable）
constexpr Word<InviteRejection> kInviteRejections[] = {
    {"invite_expired", InviteRejection::Expired},
    {"invite_wrong_network", InviteRejection::WrongNetwork},
    {"invite_own", InviteRejection::Own},
    {"invite_not_found", InviteRejection::NotFound},
    {"invite_no_node", InviteRejection::NoNode},
};

/// フレームの上限（P2p::kMaxFrameBytes）の base64 の長さ
constexpr int kMaxFrameBase64 = static_cast<int>(((P2p::kMaxFrameBytes + 2) / 3) * 4);
/// 在席の知らせ・預けるもの（1024 バイトまで）の base64 の長さ
constexpr int kMaxBlobBase64 = ((1024 + 2) / 3) * 4;

bool isPrintableAscii(const QString& text)
{
    for (const QChar c : text) {
        if (c.unicode() < 0x20 || c.unicode() > 0x7E) return false;
    }
    return true;
}

/// PeerId の文字列表記（base58btc の文字だけ）
bool isPeerIdText(const QString& text)
{
    static const QString alphabet = QStringLiteral("123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz");
    if (text.isEmpty() || text.size() > 128) return false;
    for (const QChar c : text) {
        if (!alphabet.contains(c)) return false;
    }
    return true;
}

/**
 * @brief 受けたオブジェクトのフィールドを、形を確かめながら1つずつ読む
 *
 * 足りないフィールド・型や範囲の合わない値を読むと、失敗として覚える。読み終えたら ok() で、どのフィールドも形に合い、
 * 読んでいない（知らない）フィールドが残っていないことを確かめる。値を使わないフィールドも、形を確かめるために読む。
 */
class Fields
{
public:
    explicit Fields(QJsonObject object)
        : m_object(std::move(object))
    {
    }

    bool ok() const { return m_ok && m_taken == m_object.size(); }
    /// フィールドどうしの関係が合わない
    void reject() { m_ok = false; }

    /// 印字できる ASCII の文字列
    QString text(const char* name, int maxLength)
    {
        const QJsonValue v = take(name);
        if (!v.isString() || v.toString().size() > maxLength || !isPrintableAscii(v.toString())) return failed<QString>();
        return v.toString();
    }

    /// PeerId の文字列表記
    QString peerIdText(const char* name)
    {
        const QJsonValue v = take(name);
        if (!v.isString() || !isPeerIdText(v.toString())) return failed<QString>();
        return v.toString();
    }

    /// 表にある語
    template <typename T, std::size_t N>
    T word(const char* name, const Word<T> (&words)[N])
    {
        const QJsonValue v = take(name);
        if (v.isString()) {
            const QString text = v.toString();
            for (const Word<T>& word : words) {
                if (text == QLatin1String(word.text)) return word.value;
            }
        }
        return failed<T>();
    }

    /// 決まった語のどれか（値は使わない）
    void oneOf(const char* name, std::initializer_list<const char*> words)
    {
        const QJsonValue v = take(name);
        if (v.isString()) {
            const QString text = v.toString();
            for (const char* word : words) {
                if (text == QLatin1String(word)) return;
            }
        }
        m_ok = false;
    }

    /// 32バイトの16進（小文字）
    QByteArray hex32(const char* name)
    {
        const QJsonValue v = take(name);
        const QString s = v.toString();
        if (!v.isString() || s.size() != 64) return failed<QByteArray>();
        for (const QChar c : s) {
            if (!((c >= QLatin1Char('0') && c <= QLatin1Char('9')) || (c >= QLatin1Char('a') && c <= QLatin1Char('f')))) {
                return failed<QByteArray>();
            }
        }
        return QByteArray::fromHex(s.toLatin1());
    }

    /// base64（maxLength は符号化した文字の数の上限）
    QByteArray base64(const char* name, int maxLength)
    {
        const std::optional<QByteArray> bytes = decodeBase64(take(name), maxLength);
        return bytes ? *bytes : failed<QByteArray>();
    }

    /// base64 の配列
    QList<QByteArray> base64List(const char* name, int maxCount, int maxLength)
    {
        const QJsonValue v = take(name);
        if (!v.isArray() || v.toArray().size() > maxCount) return failed<QList<QByteArray>>();
        QList<QByteArray> list;
        const QJsonArray array = v.toArray();
        for (const auto& item : array) {
            const std::optional<QByteArray> bytes = decodeBase64(item, maxLength);
            if (!bytes) return failed<QList<QByteArray>>();
            list.append(*bytes);
        }
        return list;
    }

    /// 10進の文字列の符号なし64ビット整数
    quint64 u64(const char* name)
    {
        const std::optional<quint64> n = parseU64(take(name));
        return n ? *n : failed<quint64>();
    }

    int integer(const char* name, int minimum, int maximum)
    {
        const QJsonValue v = take(name);
        if (!v.isDouble()) return failed<int>();
        const double d = v.toDouble();
        // 範囲を先に確かめる（範囲外の値を int にすると未定義動作）。NaN は比べると偽になる
        if (!(d >= minimum && d <= maximum)) return failed<int>();
        const int n = static_cast<int>(d);
        return d == static_cast<double>(n) ? n : failed<int>();
    }

    bool boolean(const char* name)
    {
        const QJsonValue v = take(name);
        return v.isBool() ? v.toBool() : failed<bool>();
    }

    /// 印字できる ASCII の文字列の配列
    QStringList texts(const char* name, int maxCount, int maxLength)
    {
        const QJsonValue v = take(name);
        if (!v.isArray() || v.toArray().size() > maxCount) return failed<QStringList>();
        QStringList list;
        const QJsonArray array = v.toArray();
        for (const auto& item : array) {
            if (!item.isString() || item.toString().size() > maxLength || !isPrintableAscii(item.toString())) {
                return failed<QStringList>();
            }
            list.append(item.toString());
        }
        return list;
    }

    /// 入れ子のオブジェクト（中身は、別の Fields で読む）
    QJsonObject object(const char* name)
    {
        const QJsonValue v = take(name);
        return v.isObject() ? v.toObject() : failed<QJsonObject>();
    }

private:
    QJsonValue take(const char* name)
    {
        const auto it = m_object.constFind(QLatin1String(name));
        if (it == m_object.constEnd()) {
            m_ok = false;
            return QJsonValue(QJsonValue::Undefined);
        }
        ++m_taken;
        return it.value();
    }

    template <typename T>
    T failed()
    {
        m_ok = false;
        return T{};
    }

    static std::optional<QByteArray> decodeBase64(const QJsonValue& v, int maxLength)
    {
        if (!v.isString() || v.toString().size() > maxLength) return std::nullopt;
        const auto decoded = QByteArray::fromBase64Encoding(v.toString().toLatin1(), QByteArray::AbortOnBase64DecodingErrors);
        if (!decoded) return std::nullopt;
        return decoded.decoded;
    }

    QJsonObject m_object;
    qsizetype m_taken = 0;
    bool m_ok = true;
};

Request request(Method method, QJsonObject params = QJsonObject())
{
    return Request{method, std::move(params)};
}

// ---------------------------------------------------------------- 応答の中身（protocol/ipc.md 3章）

Hello readHello(Fields& f)
{
    Hello hello;
    f.text("peerVersion", 64);
    f.integer("ipcVersion", kProtocolVersion, kProtocolVersion);
    Fields sandbox(f.object("sandbox"));
    hello.sandbox.applied = sandbox.boolean("applied");
    sandbox.text("os", 16);
    hello.sandbox.mechanisms = sandbox.texts("mechanisms", 16, 32);
    sandbox.integer("landlockAbi", 0, 1000);
    if (!sandbox.ok()) f.reject();
    return hello;
}

StartResult readStartResult(Fields& f)
{
    StartResult result;
    f.peerIdText("peerId");
    result.peerIdBytes = f.base64("peerIdBytes", 128);
    result.networkId = f.hex32("networkId");
    f.oneOf("profile", {"local", "internet"});
    return result;
}

CreatedInvite readCreatedInvite(Fields& f)
{
    CreatedInvite invite;
    invite.invite = f.text("invite", 1024);
    invite.code = f.text("code", 64);
    invite.expiresAt = f.u64("expiresAt");
    invite.addressCount = f.integer("addressCount", 0, 8);
    return invite;
}

InspectedInvite readInspectedInvite(Fields& f)
{
    InspectedInvite info;
    info.kind = f.word("kind", kInviteKinds);
    info.peerId = f.text("peerId", 128);
    info.expiresAt = f.u64("expiresAt");
    info.addressCount = f.integer("addressCount", 0, 8);
    info.usableAddressCount = f.integer("usableAddressCount", 0, 8);
    info.privateAddresses = f.texts("privateAddresses", 8, 256);
    // 同じ LAN の合言葉には相手の PeerId も期限も入っていない。招待文字列と、協力ノードから引き取った招待は
    // PeerId を必ず持つ
    const bool shapeOk = info.kind == InviteKind::LanCode ? info.peerId.isEmpty() && info.expiresAt == 0
                                                          : isPeerIdText(info.peerId);
    if (!shapeOk) f.reject();
    return info;
}

CreatedPresence readCreatedPresence(Fields& f)
{
    CreatedPresence presence;
    presence.body = f.base64("presence", kMaxBlobBase64);
    presence.id = f.u64("id");
    presence.expiresAt = f.u64("expiresAt");
    presence.addressCount = f.integer("addressCount", 0, 8);
    if (presence.body.isEmpty()) f.reject();
    return presence;
}

// ---------------------------------------------------------------- 通知の中身（protocol/ipc.md 4章）

NetworkStatus readNetworkStatus(Fields& f)
{
    NetworkStatus status;
    f.oneOf("state", {"connecting", "online", "offline"});
    status.connectedPeers = f.integer("connectedPeers", 0, 100000);
    status.reachability = f.word("reachability", kReachabilities);
    status.relayReservations = f.integer("relayReservations", 0, 1000);
    status.meetNodes = f.integer("meetNodes", 0, 16);
    return status;
}

ConnectionOpened readConnectionOpened(Fields& f)
{
    ConnectionOpened opened;
    opened.connection = f.u64("connection");
    opened.peerId = f.peerIdText("peerId");
    opened.peerIdBytes = f.base64("peerIdBytes", 128);
    opened.viaRelay = f.word("route", kRoutes);
    opened.outbound = f.word("direction", kDirections);
    opened.origin = f.word("origin", kOrigins);
    // 番号を使うのは、友だちの申し込み（在席の知らせのトークンで開いた接続）だけ
    const quint64 presence = f.u64("presence");
    if (opened.origin == Origin::Friend) opened.presence = presence;
    return opened;
}

} // namespace

// ---------------------------------------------------------------- 要求

QLatin1String methodName(Method method)
{
    for (const Word<Method>& word : kMethods) {
        if (word.value == method) return QLatin1String(word.text);
    }
    return QLatin1String();
}

namespace Requests {

Request hello(const QString& uiVersion)
{
    QJsonObject params;
    params[QStringLiteral("uiVersion")] = uiVersion;
    return request(Method::Hello, params);
}

Request shutdown()
{
    return request(Method::Shutdown);
}

Request start(const QString& networkName, const QStringList& bootstrap, bool relayOnly)
{
    QJsonObject params;
    params[QStringLiteral("networkName")] = networkName;
    params[QStringLiteral("bootstrap")] = QJsonArray::fromStringList(bootstrap);
    params[QStringLiteral("relayOnly")] = relayOnly;
    return request(Method::Start, params);
}

Request createInvite()
{
    return request(Method::CreateInvite);
}

Request cancelInvites()
{
    return request(Method::CancelInvites);
}

Request inspectInvite(const QString& invite)
{
    QJsonObject params;
    params[QStringLiteral("invite")] = invite;
    return request(Method::InspectInvite, params);
}

Request connectInvite(const QString& invite, bool allowPrivate)
{
    QJsonObject params;
    params[QStringLiteral("invite")] = invite;
    params[QStringLiteral("allowPrivate")] = allowPrivate;
    return request(Method::ConnectInvite, params);
}

Request setGamePeer(const QString& peerId)
{
    QJsonObject params;
    params[QStringLiteral("peerId")] = peerId;
    return request(Method::SetGamePeer, params);
}

Request connectPeer(const QString& peerId)
{
    QJsonObject params;
    params[QStringLiteral("peerId")] = peerId;
    return request(Method::ConnectPeer, params);
}

Request sendFrame(quint64 connection, const QByteArray& frame)
{
    QJsonObject params;
    params[QStringLiteral("connection")] = QString::number(connection);
    params[QStringLiteral("frame")] = QString::fromLatin1(frame.toBase64());
    return request(Method::SendFrame, params);
}

Request closeConnection(quint64 connection)
{
    QJsonObject params;
    params[QStringLiteral("connection")] = QString::number(connection);
    return request(Method::CloseConnection, params);
}

Request createPresence()
{
    return request(Method::CreatePresence);
}

Request cancelPresence()
{
    return request(Method::CancelPresence);
}

Request connectPresence(const QByteArray& presence, bool allowPrivate)
{
    QJsonObject params;
    params[QStringLiteral("presence")] = QString::fromLatin1(presence.toBase64());
    params[QStringLiteral("allowPrivate")] = allowPrivate;
    return request(Method::ConnectPresence, params);
}

Request meetPut(const QByteArray& slot, const QByteArray& blob, int ttlSeconds)
{
    QJsonObject params;
    params[QStringLiteral("slot")] = QString::fromLatin1(slot.toHex());
    params[QStringLiteral("blob")] = QString::fromLatin1(blob.toBase64());
    params[QStringLiteral("ttlSeconds")] = ttlSeconds;
    return request(Method::MeetPut, params);
}

Request meetGet(const QByteArray& slot)
{
    QJsonObject params;
    params[QStringLiteral("slot")] = QString::fromLatin1(slot.toHex());
    return request(Method::MeetGet, params);
}

} // namespace Requests

QByteArray encodeRequest(quint64 generation, quint64 requestId, const Request& request)
{
    QJsonObject message;
    message[QStringLiteral("protocolVersion")] = kProtocolVersion;
    message[QStringLiteral("processGeneration")] = QString::number(generation);
    message[QStringLiteral("requestId")] = QString::number(requestId);
    message[QStringLiteral("method")] = methodName(request.method);
    message[QStringLiteral("params")] = request.params;
    const QByteArray json = QJsonDocument(message).toJson(QJsonDocument::Compact);
    if (json.size() > kMaxMessageBytes) return {};
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

// ---------------------------------------------------------------- 受けたメッセージの外枠

Framing takeMessage(QByteArray& buffer, QByteArray* json)
{
    if (buffer.size() < 4) return Framing::Incomplete;
    const quint32 length = (static_cast<quint32>(static_cast<quint8>(buffer.at(0))) << 24)
        | (static_cast<quint32>(static_cast<quint8>(buffer.at(1))) << 16)
        | (static_cast<quint32>(static_cast<quint8>(buffer.at(2))) << 8)
        | static_cast<quint32>(static_cast<quint8>(buffer.at(3)));
    if (length < 2 || length > static_cast<quint32>(kMaxMessageBytes)) return Framing::Violation;
    const auto size = static_cast<qsizetype>(length);
    if (buffer.size() < size + 4) return Framing::Incomplete;
    *json = buffer.mid(4, size);
    buffer.remove(0, size + 4);
    return Framing::Message;
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
        in.notification = obj.value(QStringLiteral("notification")).toString();
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
    Fields err(obj.value(QStringLiteral("error")).toObject());
    in.kind = Incoming::Kind::Error;
    in.errorCode = err.text("code", 32);
    err.text("message", 512);
    if (!err.ok()) return fail(QStringLiteral("malformed error"));
    return in;
}

// ---------------------------------------------------------------- 応答と通知の読み取り

std::optional<Reply> readReply(Method method, const QJsonObject& result)
{
    Fields f(result);
    Reply reply = Done{};
    switch (method) {
    case Method::Hello:
        reply = readHello(f);
        break;
    case Method::Start:
        reply = readStartResult(f);
        break;
    case Method::CreateInvite:
        reply = readCreatedInvite(f);
        break;
    case Method::InspectInvite:
        reply = readInspectedInvite(f);
        break;
    case Method::ConnectInvite:
    case Method::ConnectPeer:
    case Method::ConnectPresence:
        reply = ConnectionStarted{f.u64("connection")};
        break;
    case Method::CreatePresence:
        reply = readCreatedPresence(f);
        break;
    case Method::MeetPut:
        reply = MeetStored{f.integer("stored", 0, 16)};
        break;
    case Method::MeetGet:
        reply = MeetFetched{f.base64List("blobs", 16, kMaxBlobBase64)};
        break;
    case Method::CancelInvites:
    case Method::SetGamePeer:
    case Method::SendFrame:
    case Method::CloseConnection:
    case Method::CancelPresence:
    case Method::Shutdown:
        // 中身のない応答（フィールドがあれば、下の ok() が断る）
        break;
    }
    if (!f.ok()) return std::nullopt;
    return reply;
}

std::optional<InviteRejection> inviteRejection(Method method, const QString& errorCode)
{
    if (method != Method::InspectInvite && method != Method::ConnectInvite && method != Method::ConnectPresence) {
        return std::nullopt;
    }
    for (const Word<InviteRejection>& word : kInviteRejections) {
        if (errorCode == QLatin1String(word.text)) return word.value;
    }
    return InviteRejection::Unreadable;
}

std::optional<Notification> readNotification(const QString& name, const QJsonObject& params)
{
    Fields f(params);
    Notification notification;
    if (name == QLatin1String(kNetworkStatus)) {
        notification = readNetworkStatus(f);
    } else if (name == QLatin1String(kConnectionOpened)) {
        notification = readConnectionOpened(f);
    } else if (name == QLatin1String(kConnectionRoute)) {
        const quint64 connection = f.u64("connection");
        notification = ConnectionRoute{connection, f.word("route", kRoutes)};
    } else if (name == QLatin1String(kFrameReceived)) {
        const quint64 connection = f.u64("connection");
        notification = FrameReceived{connection, f.base64("frame", kMaxFrameBase64)};
    } else if (name == QLatin1String(kConnectionClosed)) {
        const quint64 connection = f.u64("connection");
        notification = ConnectionClosed{connection, f.word("reason", kCloseReasons)};
    } else {
        return std::nullopt;
    }
    if (!f.ok()) return std::nullopt;
    return notification;
}

} // namespace PeerIpc
