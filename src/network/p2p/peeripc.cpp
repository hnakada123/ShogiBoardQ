/// @file peeripc.cpp
/// @brief 通信プロセスとの IPC のメッセージの形と検査の実装

#include "peeripc.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QStringList>

#include <functional>
#include <initializer_list>
#include <utility>

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

/// 64 KiB のフレームの base64 の長さ
constexpr int kMaxFrameBase64 = ((64 * 1024 + 2) / 3) * 4;

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
    if (method == QLatin1String("start")) {
        return exactFields(result, {{"peerId", peerIdText()},
                                    {"peerIdBytes", base64(128)},
                                    {"networkId", hex32()},
                                    {"profile", oneOf({"local", "internet"})}});
    }
    if (method == QLatin1String("createInvite")) {
        return exactFields(result, {{"invite", str(1024)}, {"expiresAt", u64()}, {"addressCount", integer(0, 8)}});
    }
    if (method == QLatin1String("inspectInvite")) {
        return exactFields(result, {{"peerId", peerIdText()},
                                    {"expiresAt", u64()},
                                    {"addressCount", integer(0, 8)},
                                    {"usableAddressCount", integer(0, 8)},
                                    {"privateAddresses", stringArray(8, 256)}});
    }
    if (method == QLatin1String("connectInvite") || method == QLatin1String("connectPeer")) {
        return exactFields(result, {{"connection", u64()}});
    }
    if (method == QLatin1String("cancelInvites") || method == QLatin1String("setGamePeer")
        || method == QLatin1String("sendFrame") || method == QLatin1String("closeConnection")
        || method == QLatin1String("shutdown")) {
        return result.isEmpty();
    }
    return false;
}

bool checkNotification(const QString& name, const QJsonObject& params)
{
    if (name == QLatin1String("networkStatus")) {
        return exactFields(params, {{"state", oneOf({"connecting", "online", "offline"})},
                                    {"connectedPeers", integer(0, 100000)},
                                    {"reachability", oneOf({"unknown", "public", "private"})},
                                    {"relayReservations", integer(0, 1000)}});
    }
    if (name == QLatin1String("connectionOpened")) {
        return exactFields(params, {{"connection", u64()},
                                    {"peerId", peerIdText()},
                                    {"peerIdBytes", base64(128)},
                                    {"route", oneOf({"direct", "relay"})},
                                    {"direction", oneOf({"inbound", "outbound"})},
                                    {"origin", oneOf({"invite", "game"})}});
    }
    if (name == QLatin1String("connectionRoute")) {
        return exactFields(params, {{"connection", u64()}, {"route", oneOf({"direct", "relay"})}});
    }
    if (name == QLatin1String("frameReceived")) {
        return exactFields(params, {{"connection", u64()}, {"frame", base64(kMaxFrameBase64)}});
    }
    if (name == QLatin1String("connectionClosed")) {
        return exactFields(params, {{"connection", u64()},
                                    {"reason", oneOf({"closed", "reset", "timeout", "unreachable", "no_relay_route",
                                                      "invalid_frame", "rate_limited", "limit", "shutdown"})}});
    }
    return false;
}

} // namespace PeerIpc
