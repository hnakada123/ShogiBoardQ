/// @file tst_p2p_protocol.cpp
/// @brief P2P対局の形式と記録の試験（決定的CBOR・署名・表示名・規則・証跡の検証。実装仕様 13.1節）

#include <QtTest>

#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include "p2p_test_util.h"
#include "p2pcbor.h"
#include "p2pdisplayname.h"
#include "p2pevidenceverifier.h"
#include "p2pframes.h"
#include "p2pfriend.h"
#include "matchverificationservice.h"
#include "p2pbuildexpiry.h"
#include "peeripc.h"

using namespace P2p;

namespace {

QByteArray hex(const char* text) { return QByteArray::fromHex(text); }

/// 32バイトの小端の整数に群の位数 L を足す（正規の形でない S を作る）
QByteArray addGroupOrder(const QByteArray& s)
{
    const QByteArray order = hex("edd3f55c1a631258d69cf7a2def9de1400000000000000000000000000000010");
    QByteArray out(32, '\0');
    unsigned carry = 0;
    for (int i = 0; i < 32; ++i) {
        const unsigned sum = static_cast<quint8>(s.at(i)) + static_cast<quint8>(order.at(i)) + carry;
        out[i] = static_cast<char>(sum & 0xFFu);
        carry = sum >> 8;
    }
    return out;
}

} // namespace

class TestP2pProtocol : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QVERIFY(Crypto::initialize()); }

    // ------------------------------------------------------------ 決定的CBOR

    void cborRoundTrip()
    {
        const CborValue value = CborValue::array({CborValue::uint(0), CborValue::uint(23), CborValue::uint(24),
                                                  CborValue::uint(255), CborValue::uint(256), CborValue::uint(65535),
                                                  CborValue::uint(65536), CborValue::uint(0xFFFFFFFFULL),
                                                  CborValue::uint(0x100000000ULL), CborValue::bytes("ab"),
                                                  CborValue::text("SBQ"), CborValue::array({})});
        const QByteArray bytes = Cbor::encode(value);
        const std::optional<CborValue> decoded = Cbor::decode(bytes);
        QVERIFY(decoded);
        QVERIFY(*decoded == value);
        QCOMPARE(Cbor::encode(*decoded), bytes);
        QCOMPARE(Cbor::encode(CborValue::uint(24)), hex("1818"));
        QCOMPARE(Cbor::encode(CborValue::uint(0x100000000ULL)), hex("1b0000000100000000"));
    }

    void cborRejectsNonDeterministic_data()
    {
        QTest::addColumn<QByteArray>("bytes");
        QTest::newRow("non-shortest uint") << hex("1817");
        QTest::newRow("non-shortest 2-byte") << hex("1900ff");
        QTest::newRow("indefinite array") << hex("9f01ff");
        QTest::newRow("indefinite bytes") << hex("5f41aaff");
        QTest::newRow("map") << hex("a10102");
        QTest::newRow("negative") << hex("20");
        QTest::newRow("tag") << hex("c001");
        QTest::newRow("float") << hex("f93c00");
        QTest::newRow("true") << hex("f5");
        QTest::newRow("trailing") << hex("0101");
        QTest::newRow("truncated") << hex("8301");
        QTest::newRow("bad utf8") << hex("62c328");
        QTest::newRow("overlong utf8") << hex("62c0af");
        QTest::newRow("surrogate") << hex("63eda080");
        QTest::newRow("huge length") << hex("5bffffffffffffffff");
        QTest::newRow("empty") << QByteArray();
        QByteArray deep;
        for (int i = 0; i < 17; ++i) deep.append('\x81');
        deep.append('\x00');
        QTest::newRow("depth 17") << deep;
    }

    void cborRejectsNonDeterministic()
    {
        QFETCH(QByteArray, bytes);
        QVERIFY(!Cbor::decode(bytes));
    }

    void cborAcceptsDepth16()
    {
        QByteArray deep;
        for (int i = 0; i < 15; ++i) deep.append('\x81');
        deep.append('\x00');
        QVERIFY(Cbor::decode(deep));
    }

    /// Rust の実装と同じ入力に同じ結果を返す（protocol/testvectors/cbor.txt）
    /// 1回の読み取りで作る値の総数に上限がある。細工した証跡で、入力の何十倍ものメモリを使わせない（監査 2026-11-13 P16）
    void cborLimitsTotalValues()
    {
        const auto zeros = [](quint32 count) {
            QByteArray bytes;
            bytes.append(char(0x9a));
            for (int shift = 24; shift >= 0; shift -= 8) bytes.append(char((count >> shift) & 0xffu));
            bytes.append(QByteArray(static_cast<qsizetype>(count), '\0'));
            return bytes;
        };
        // 配列と要素で、ちょうど上限の数
        const auto limit = static_cast<quint32>(Cbor::kMaxValues);
        QVERIFY(Cbor::decode(zeros(limit - 1)));
        QString error;
        QVERIFY(!Cbor::decode(zeros(limit), &error));
        QCOMPARE(error, QStringLiteral("too many values"));
        // 入れ子の各段が、残りの長さを要素数に名乗る（読めずに終わる。確保は名乗った数に比例しない）
        constexpr int kLevels = 15;
        constexpr int kTail = 4096;
        QByteArray nested;
        for (int i = 0; i < kLevels; ++i) {
            const auto claimed = static_cast<quint32>(5 * (kLevels - i - 1) + kTail);
            nested.append(char(0x9a));
            for (int shift = 24; shift >= 0; shift -= 8) nested.append(char((claimed >> shift) & 0xffu));
        }
        nested.append(QByteArray(kTail, '\0'));
        QVERIFY(!Cbor::decode(nested));
    }

    void cborSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("cbor.txt"));
        QVERIFY2(!lines.isEmpty(), "protocol/testvectors/cbor.txt is missing");
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QVERIFY2(parts.size() >= 2, line.constData());
            const bool expected = parts.at(0) == "valid";
            const QByteArray bytes = QByteArray::fromHex(parts.at(1));
            QVERIFY2(Cbor::decode(bytes).has_value() == expected, line.constData());
        }
    }

    void frameSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("frames.txt"));
        QVERIFY2(!lines.isEmpty(), "protocol/testvectors/frames.txt is missing");
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QVERIFY2(parts.size() >= 2, line.constData());
            const bool expected = parts.at(0) == "valid";
            const QByteArray bytes = QByteArray::fromHex(parts.at(1));
            QVERIFY2(Frame::isWellFormedEnvelope(bytes) == expected, line.constData());
            // 本番の読み取りも同じ外枠の規則で読む。正しい外枠で断るのは、対局・UI側に届かない INVITE_TOKEN だけ
            // （監査 2026-11-12 5.4）
            const std::optional<Frame> frame = Frame::decode(bytes);
            if (!expected) {
                QVERIFY2(!frame, line.constData());
            } else if (!frame) {
                QCOMPARE(Cbor::decode(bytes)->items().at(1).toUInt(), quint64(FrameType::InviteToken));
            }
        }
    }

    /// IPC の要求の作成と、応答・通知の読み取りは、検査と同じ名前と語を使う（監査 2026-11-12 R5）
    void ipcRequestsAndRepliesAreTyped()
    {
        const auto object = [](const char* text) { return QJsonDocument::fromJson(QByteArray(text)).object(); };
        const PeerIpc::Request send = PeerIpc::Requests::sendFrame(5, QByteArray("ab"));
        QCOMPARE(send.method, QStringLiteral("sendFrame"));
        QCOMPARE(send.params.value(QStringLiteral("connection")).toString(), QStringLiteral("5"));
        QCOMPARE(send.params.value(QStringLiteral("frame")).toString(), QStringLiteral("YWI="));
        const PeerIpc::Request start = PeerIpc::Requests::start(QStringLiteral("sbq-local-1"), {QStringLiteral("/ip4/x")}, true);
        QCOMPARE(start.method, QStringLiteral("start"));
        QCOMPARE(start.params.value(QStringLiteral("relayOnly")).toBool(), true);
        QCOMPARE(start.params.value(QStringLiteral("bootstrap")).toArray().size(), 1);

        // 応答
        const QByteArray hex(64, 'b');
        const QJsonObject startResult = object((R"({"peerId":"12D3KooWabc","peerIdBytes":"AAEC","networkId":")" + hex
                                                + R"(","profile":"local"})").constData());
        QVERIFY(PeerIpc::checkResult(QStringLiteral("start"), startResult));
        const std::optional<PeerIpc::Reply> started = PeerIpc::readReply(QStringLiteral("start"), startResult);
        QVERIFY(started && std::holds_alternative<PeerIpc::StartResult>(*started));
        QCOMPARE(std::get<PeerIpc::StartResult>(*started).peerIdBytes, QByteArray::fromHex("000102"));
        QCOMPARE(std::get<PeerIpc::StartResult>(*started).networkId, QByteArray::fromHex(hex));
        const std::optional<PeerIpc::Reply> connecting =
            PeerIpc::readReply(QStringLiteral("connectPeer"), object(R"({"connection":"9"})"));
        QVERIFY(connecting && std::holds_alternative<PeerIpc::ConnectionStarted>(*connecting));
        QCOMPARE(std::get<PeerIpc::ConnectionStarted>(*connecting).connection, quint64(9));
        QVERIFY(!PeerIpc::readReply(QStringLiteral("unknownMethod"), QJsonObject()));

        // 招待の要求の失敗だけを理由にする
        QCOMPARE(PeerIpc::inviteRejection(QStringLiteral("inspectInvite"), QStringLiteral("invite_own")),
                 std::optional<PeerIpc::InviteRejection>(PeerIpc::InviteRejection::Own));
        QCOMPARE(PeerIpc::inviteRejection(QStringLiteral("connectInvite"), QStringLiteral("invite_invalid")),
                 std::optional<PeerIpc::InviteRejection>(PeerIpc::InviteRejection::Unreadable));
        // 協力ノードを使う合言葉: 招待が見つからない・問い合わせる協力ノードが無い
        QCOMPARE(PeerIpc::inviteRejection(QStringLiteral("inspectInvite"), QStringLiteral("invite_not_found")),
                 std::optional<PeerIpc::InviteRejection>(PeerIpc::InviteRejection::NotFound));
        QCOMPARE(PeerIpc::inviteRejection(QStringLiteral("connectInvite"), QStringLiteral("invite_no_node")),
                 std::optional<PeerIpc::InviteRejection>(PeerIpc::InviteRejection::NoNode));
        QVERIFY(!PeerIpc::inviteRejection(QStringLiteral("sendFrame"), QStringLiteral("unknown_connection")));

        // 通知
        const QJsonObject opened = object(R"({"connection":"5","peerId":"12D3KooWabc","peerIdBytes":"AAEC",)"
                                          R"("route":"relay","direction":"outbound","origin":"game","presence":"0"})");
        QVERIFY(PeerIpc::checkNotification(QStringLiteral("connectionOpened"), opened));
        const std::optional<PeerIpc::Notification> openedRead = PeerIpc::readNotification(QStringLiteral("connectionOpened"), opened);
        QVERIFY(openedRead && std::holds_alternative<PeerIpc::ConnectionOpened>(*openedRead));
        const auto& o = std::get<PeerIpc::ConnectionOpened>(*openedRead);
        QVERIFY(o.viaRelay && o.outbound && o.gameReconnect);
        QCOMPARE(o.connection, quint64(5));
        QCOMPARE(o.presence, quint64(0));
        // 友だちの申し込み（在席の知らせのトークンで開いた接続）は、どの知らせかが番号で分かる
        QJsonObject fromFriend = opened;
        fromFriend.insert(QStringLiteral("direction"), QStringLiteral("inbound"));
        fromFriend.insert(QStringLiteral("origin"), QStringLiteral("friend"));
        fromFriend.insert(QStringLiteral("presence"), QStringLiteral("12"));
        QVERIFY(PeerIpc::checkNotification(QStringLiteral("connectionOpened"), fromFriend));
        const std::optional<PeerIpc::Notification> friendRead =
            PeerIpc::readNotification(QStringLiteral("connectionOpened"), fromFriend);
        QVERIFY(friendRead && std::holds_alternative<PeerIpc::ConnectionOpened>(*friendRead));
        QCOMPARE(std::get<PeerIpc::ConnectionOpened>(*friendRead).presence, quint64(12));
        QVERIFY(!std::get<PeerIpc::ConnectionOpened>(*friendRead).gameReconnect);
        fromFriend.remove(QStringLiteral("presence"));
        QVERIFY(!PeerIpc::checkNotification(QStringLiteral("connectionOpened"), fromFriend));

        // 友だち一覧の要求と応答
        const PeerIpc::Request put = PeerIpc::Requests::meetPut(QByteArray(32, '\xab'), QByteArray("blob"), 180);
        QCOMPARE(put.method, QStringLiteral("meetPut"));
        QCOMPARE(put.params.value(QStringLiteral("slot")).toString(), QString(64, QLatin1Char('a')).replace(QStringLiteral("aa"), QStringLiteral("ab")));
        QCOMPARE(put.params.value(QStringLiteral("blob")).toString(), QStringLiteral("YmxvYg=="));
        QCOMPARE(put.params.value(QStringLiteral("ttlSeconds")).toInt(), 180);
        QCOMPARE(PeerIpc::Requests::connectPresence(QByteArray("ab"), false).params.value(QStringLiteral("presence")).toString(),
                 QStringLiteral("YWI="));
        const QJsonObject created = object(R"({"presence":"YWI=","id":"3","expiresAt":"100","addressCount":2})");
        QVERIFY(PeerIpc::checkResult(QStringLiteral("createPresence"), created));
        const std::optional<PeerIpc::Reply> createdRead = PeerIpc::readReply(QStringLiteral("createPresence"), created);
        QVERIFY(createdRead && std::holds_alternative<PeerIpc::CreatedPresence>(*createdRead));
        QCOMPARE(std::get<PeerIpc::CreatedPresence>(*createdRead).body, QByteArray("ab"));
        QCOMPARE(std::get<PeerIpc::CreatedPresence>(*createdRead).id, quint64(3));
        QCOMPARE(std::get<PeerIpc::CreatedPresence>(*createdRead).addressCount, 2);
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("createPresence"),
                                      object(R"({"presence":"","id":"3","expiresAt":"100","addressCount":2})")));
        QVERIFY(PeerIpc::checkResult(QStringLiteral("meetPut"), object(R"({"stored":1})")));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("meetPut"), object(R"({"stored":17})")));
        const QJsonObject fetched = object(R"({"blobs":["YWI=","Y2Q="]})");
        QVERIFY(PeerIpc::checkResult(QStringLiteral("meetGet"), fetched));
        const std::optional<PeerIpc::Reply> fetchedRead = PeerIpc::readReply(QStringLiteral("meetGet"), fetched);
        QVERIFY(fetchedRead && std::holds_alternative<PeerIpc::MeetFetched>(*fetchedRead));
        QCOMPARE(std::get<PeerIpc::MeetFetched>(*fetchedRead).blobs, (QList<QByteArray>{QByteArray("ab"), QByteArray("cd")}));
        QVERIFY(PeerIpc::checkResult(QStringLiteral("meetGet"), object(R"({"blobs":[]})")));
        QVERIFY(PeerIpc::checkResult(QStringLiteral("cancelPresence"), QJsonObject()));
        QVERIFY(PeerIpc::checkResult(QStringLiteral("connectPresence"), object(R"({"connection":"9"})")));
        QCOMPARE(PeerIpc::inviteRejection(QStringLiteral("connectPresence"), QStringLiteral("invite_expired")),
                 std::optional<PeerIpc::InviteRejection>(PeerIpc::InviteRejection::Expired));
        const QJsonObject closed = object(R"({"connection":"7","reason":"no_relay_route"})");
        QVERIFY(PeerIpc::checkNotification(QStringLiteral("connectionClosed"), closed));
        const std::optional<PeerIpc::Notification> closedRead = PeerIpc::readNotification(QStringLiteral("connectionClosed"), closed);
        QVERIFY(closedRead && std::holds_alternative<PeerIpc::ConnectionClosed>(*closedRead));
        QCOMPARE(std::get<PeerIpc::ConnectionClosed>(*closedRead).reason, PeerIpc::CloseReason::NoRelayRoute);
        const std::optional<PeerIpc::Notification> frame =
            PeerIpc::readNotification(QStringLiteral("frameReceived"), object(R"({"connection":"7","frame":"YWI="})"));
        QVERIFY(frame && std::holds_alternative<PeerIpc::FrameReceived>(*frame));
        QCOMPARE(std::get<PeerIpc::FrameReceived>(*frame).frame, QByteArray("ab"));
        QVERIFY(!PeerIpc::readNotification(QStringLiteral("unknownNotification"), QJsonObject()));
        // 範囲外の大きな数は、int にする前に断る（監査 2026-11-13。UBSan の float-cast-overflow）
        const QJsonObject huge = object(R"({"state":"online","connectedPeers":1e300,"reachability":"unknown",)"
                                        R"("relayReservations":0,"meetNodes":0})");
        QVERIFY(!PeerIpc::checkNotification(QStringLiteral("networkStatus"), huge));
        const QJsonObject status = object(R"({"state":"online","connectedPeers":3,"reachability":"unknown",)"
                                          R"("relayReservations":0,"meetNodes":1})");
        QVERIFY(PeerIpc::checkNotification(QStringLiteral("networkStatus"), status));
        const std::optional<PeerIpc::Notification> statusRead = PeerIpc::readNotification(QStringLiteral("networkStatus"), status);
        QVERIFY(statusRead && std::holds_alternative<PeerIpc::NetworkStatus>(*statusRead));
        QCOMPARE(std::get<PeerIpc::NetworkStatus>(*statusRead).meetNodes, 1);
    }

    /// ビルドの使用期限は、期限の時刻を過ぎてから（監査 2026-11-12 R5。通信プロセスと同じ起点と日数）
    void buildExpiryIsAfterTheLifetime()
    {
        const qint64 expiresAt = P2pBuildExpiry::expiresAt();
        QVERIFY(!P2pBuildExpiry::isExpiredAt(expiresAt));
        QVERIFY(P2pBuildExpiry::isExpiredAt(expiresAt + 1));
        // 期限は構成した時刻から180日後で、試験を動かしている今はまだ過ぎていない
        QVERIFY(expiresAt > QDateTime::currentSecsSinceEpoch());
        QVERIFY(!P2pBuildExpiry::isExpired());
    }

    /// 交渉の前に接続を断る REJECT は、外枠の規則どおりに読めて、理由を読み戻せる（監査 2026-11-12 R5）
    void rejectFrameRoundTrips()
    {
        const std::optional<Frame> frame = Frame::decode(rejectFrame(ErrorCode::Busy).encode());
        QVERIFY(frame);
        QCOMPARE(frame->type, FrameType::Reject);
        QCOMPARE(frame->requestId, quint64(1));
        QCOMPARE(frame->gameId, zeroHash());
        QCOMPARE(parseReject(frame->payload), std::optional<ErrorCode>(ErrorCode::Busy));
    }

    /// ABORT・ERROR の body は理由のコードを読み戻す。受けた側が使わない latestStateHash・relatedPly も形は確かめる
    void abortAndErrorBodiesRoundTrip()
    {
        const QByteArray hash(kHashSize, '\x02');
        QCOMPARE(ControlBody::parseAbort(ControlBody::abort(hash, ErrorCode::DisconnectLimit)),
                 std::optional<ErrorCode>(ErrorCode::DisconnectLimit));
        QCOMPARE(ControlBody::parseError(ControlBody::error(ErrorCode::ClockMismatch, 12)),
                 std::optional<ErrorCode>(ErrorCode::ClockMismatch));
        QVERIFY(!ControlBody::parseAbort(ControlBody::abort(QByteArray(3, '\x02'), ErrorCode::Cancelled)));
        QVERIFY(!ControlBody::parseError(ControlBody::error(ErrorCode::Cancelled, kMaxPly + 1)));
        QVERIFY(!ControlBody::parseAbort(CborValue::array({CborValue::bytes(hash), CborValue::uint(0xFE)})));
    }

    /// 通信プロセスから届く IPC のメッセージは、外枠と中身の形をすべて確かめる（監査 2026-11-12 5.4。protocol/ipc.md）
    void ipcMessagesAreCheckedStrictly()
    {
        const auto json = [](const char* text) { return QByteArray(text); };
        // 外枠
        QVERIFY(PeerIpc::parse(json(R"({"protocolVersion":5,"processGeneration":"3","requestId":"17","result":{}})")));
        QVERIFY(PeerIpc::parse(json(R"({"protocolVersion":5,"processGeneration":"3","notification":"x","params":{}})")));
        QVERIFY(PeerIpc::parse(json(R"({"protocolVersion":5,"processGeneration":"3","requestId":"17",)"
                                    R"("error":{"code":"unknown_connection","message":"m"}})")));
        QVERIFY(!PeerIpc::parse(json(R"({"protocolVersion":4,"processGeneration":"3","requestId":"17","result":{}})")));
        QVERIFY(!PeerIpc::parse(json(R"({"protocolVersion":5,"processGeneration":"03","requestId":"17","result":{}})")));
        QVERIFY(!PeerIpc::parse(json(R"({"protocolVersion":5,"processGeneration":3,"requestId":"17","result":{}})")));
        QVERIFY(!PeerIpc::parse(json(R"({"protocolVersion":5,"processGeneration":"3","requestId":"+1","result":{}})")));
        QVERIFY(!PeerIpc::parse(json(R"({"protocolVersion":5,"processGeneration":"3","requestId":"17","result":{},"x":1})")));
        QVERIFY(!PeerIpc::parse(json(R"({"protocolVersion":5,"processGeneration":"3","requestId":"17",)"
                                     R"("error":{"code":"c","message":"m","extra":"e"}})")));
        QVERIFY(!PeerIpc::parse(json(R"([1,2])")));
        QCOMPARE(PeerIpc::parseU64(QJsonValue(QStringLiteral("18446744073709551615"))), std::optional<quint64>(~0ULL));
        QVERIFY(!PeerIpc::parseU64(QJsonValue(QStringLiteral("18446744073709551616"))));

        // 応答の中身（知らないフィールド・足りないフィールド・形の合わない値を拒否する）
        const auto object = [](const char* text) { return QJsonDocument::fromJson(QByteArray(text)).object(); };
        const QByteArray hex(64, 'a');
        const QJsonObject start = object((R"({"peerId":"12D3KooWabc","peerIdBytes":"AAEC","networkId":")" + hex
                                          + R"(","profile":"local"})").constData());
        QVERIFY(PeerIpc::checkResult(QStringLiteral("start"), start));
        QJsonObject extra = start;
        extra.insert(QStringLiteral("address"), QStringLiteral("192.0.2.1"));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("start"), extra));
        QJsonObject missing = start;
        missing.remove(QStringLiteral("profile"));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("start"), missing));
        QJsonObject upper = start;
        upper.insert(QStringLiteral("networkId"), QString::fromLatin1(hex.toUpper()));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("start"), upper));
        QVERIFY(PeerIpc::checkResult(QStringLiteral("sendFrame"), QJsonObject()));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("sendFrame"), object(R"({"ok":true})")));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("unknownMethod"), QJsonObject()));

        // 招待の応答には合言葉の欄がある（出せないときは空）
        const QJsonObject created = object(R"({"invite":"sbqinv1.AAAA.00000000","code":"008Q-4HMA-SW95",)"
                                           R"("expiresAt":"100","addressCount":2})");
        QVERIFY(PeerIpc::checkResult(QStringLiteral("createInvite"), created));
        const std::optional<PeerIpc::Reply> createdRead = PeerIpc::readReply(QStringLiteral("createInvite"), created);
        QVERIFY(createdRead && std::holds_alternative<PeerIpc::CreatedInvite>(*createdRead));
        QCOMPARE(std::get<PeerIpc::CreatedInvite>(*createdRead).code, QStringLiteral("008Q-4HMA-SW95"));
        QCOMPARE(std::get<PeerIpc::CreatedInvite>(*createdRead).expiresAt, quint64(100));
        QJsonObject withoutCode = created;
        withoutCode.insert(QStringLiteral("code"), QString());
        QVERIFY(PeerIpc::checkResult(QStringLiteral("createInvite"), withoutCode));
        withoutCode.remove(QStringLiteral("code"));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("createInvite"), withoutCode));

        // 確かめた結果: 招待文字列と、協力ノードを使う合言葉（引き取った招待）は PeerId を持ち、同じ LAN の合言葉は
        // PeerId も期限も持たない
        const QJsonObject asInvite = object(R"({"kind":"invite","peerId":"12D3KooWabc","expiresAt":"100",)"
                                            R"("addressCount":2,"usableAddressCount":1,"privateAddresses":[]})");
        QVERIFY(PeerIpc::checkResult(QStringLiteral("inspectInvite"), asInvite));
        const std::optional<PeerIpc::Reply> inviteRead = PeerIpc::readReply(QStringLiteral("inspectInvite"), asInvite);
        QVERIFY(inviteRead && std::holds_alternative<PeerIpc::InspectedInvite>(*inviteRead));
        QCOMPARE(std::get<PeerIpc::InspectedInvite>(*inviteRead).kind, PeerIpc::InviteKind::Invite);
        QCOMPARE(std::get<PeerIpc::InspectedInvite>(*inviteRead).usableAddressCount, 1);
        const QJsonObject asCode = object(R"({"kind":"code","peerId":"","expiresAt":"0",)"
                                          R"("addressCount":2,"usableAddressCount":2,"privateAddresses":[]})");
        QVERIFY(PeerIpc::checkResult(QStringLiteral("inspectInvite"), asCode));
        const std::optional<PeerIpc::Reply> codeRead = PeerIpc::readReply(QStringLiteral("inspectInvite"), asCode);
        QVERIFY(codeRead && std::holds_alternative<PeerIpc::InspectedInvite>(*codeRead));
        QCOMPARE(std::get<PeerIpc::InspectedInvite>(*codeRead).kind, PeerIpc::InviteKind::LanCode);
        QVERIFY(std::get<PeerIpc::InspectedInvite>(*codeRead).peerId.isEmpty());
        QJsonObject asNodeCode = asInvite;
        asNodeCode.insert(QStringLiteral("kind"), QStringLiteral("meetCode"));
        QVERIFY(PeerIpc::checkResult(QStringLiteral("inspectInvite"), asNodeCode));
        const std::optional<PeerIpc::Reply> nodeCodeRead = PeerIpc::readReply(QStringLiteral("inspectInvite"), asNodeCode);
        QVERIFY(nodeCodeRead && std::holds_alternative<PeerIpc::InspectedInvite>(*nodeCodeRead));
        QCOMPARE(std::get<PeerIpc::InspectedInvite>(*nodeCodeRead).kind, PeerIpc::InviteKind::NodeCode);
        QCOMPARE(std::get<PeerIpc::InspectedInvite>(*nodeCodeRead).expiresAt, quint64(100));
        asNodeCode.insert(QStringLiteral("peerId"), QString());
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("inspectInvite"), asNodeCode));
        QJsonObject badKind = asInvite;
        badKind.insert(QStringLiteral("peerId"), QString());
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("inspectInvite"), badKind));
        badKind = asCode;
        badKind.insert(QStringLiteral("peerId"), QStringLiteral("12D3KooWabc"));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("inspectInvite"), badKind));
        badKind = asCode;
        badKind.insert(QStringLiteral("expiresAt"), QStringLiteral("100"));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("inspectInvite"), badKind));
        badKind = asInvite;
        badKind.insert(QStringLiteral("kind"), QStringLiteral("word"));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("inspectInvite"), badKind));
        badKind = asInvite;
        badKind.remove(QStringLiteral("kind"));
        QVERIFY(!PeerIpc::checkResult(QStringLiteral("inspectInvite"), badKind));

        // 通知の中身
        QVERIFY(PeerIpc::checkNotification(QStringLiteral("connectionClosed"),
                                           object(R"({"connection":"5","reason":"closed"})")));
        // 通信プロセスは shutdown の後に知らせずに終わるので、shutdown の理由は届かない（ipc.md 4章）
        QVERIFY(!PeerIpc::checkNotification(QStringLiteral("connectionClosed"),
                                            object(R"({"connection":"5","reason":"shutdown"})")));
        QVERIFY(!PeerIpc::checkNotification(QStringLiteral("connectionOpened"),
                                            object(R"({"connection":"5","peerId":"12D3KooWabc","peerIdBytes":"AAEC",)"
                                                   R"("route":"direct","direction":"inbound","origin":"public","presence":"0"})")));
        QVERIFY(!PeerIpc::checkNotification(QStringLiteral("frameReceived"),
                                            object(R"({"connection":"5","frame":"not base64!"})")));
        QVERIFY(!PeerIpc::checkNotification(QStringLiteral("unknownNotification"), QJsonObject()));
    }

    void networkSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("network.txt"));
        QVERIFY2(lines.size() >= 2, "protocol/testvectors/network.txt is missing");
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QCOMPARE(parts.size(), 3);
            const QByteArray id = networkId(parts.at(0));
            QCOMPARE(id.toHex(), parts.at(1));
            QCOMPARE(Rules::v1Hash().toHex(), parts.at(2));
            QVERIFY(findNetwork(QString::fromLatin1(parts.at(0))));
        }
    }

    /// 友だち一覧の在席の知らせ: 2人だけの秘密・協力ノードに預ける番号と鍵・暗号化が、独立に作った試験入力
    /// （libsodium と SHA-256 で計算したもの）と同じ値になる
    void friendPresenceVectors()
    {
        int counts[5] = {0, 0, 0, 0, 0};
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("friend.txt"));
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            const QByteArray kind = parts.at(0);
            const auto hex = [&parts](int index) { return QByteArray::fromHex(parts.at(index)); };
            if (kind == "pair") {
                QCOMPARE(parts.size(), 6);
                SecureBytes seed(32);
                std::memcpy(seed.data(), hex(1).constData(), 32);
                const SigningKey mine = SigningKey::fromSeed(seed);
                QCOMPARE(mine.publicKey().toHex(), parts.at(2));
                const SecureBytes shared = mine.sharedSecret(hex(3));
                QVERIFY(shared.isValid());
                QCOMPARE(QByteArray(reinterpret_cast<const char*>(shared.data()), 32).toHex(), parts.at(4));
                QCOMPARE(Friend::pairSecret(mine, hex(3)).toHex(), parts.at(5));
                // 2人の ID の順序によらない
                QCOMPARE(Friend::pairSecretFromShared(hex(4), hex(3), hex(2)).toHex(), parts.at(5));
                ++counts[0];
            } else if (kind == "badkey") {
                const SigningKey mine = SigningKey::generate();
                QVERIFY2(!mine.sharedSecret(hex(1)).isValid(), parts.at(1).constData());
                QVERIFY(Friend::pairSecret(mine, hex(1)).isEmpty());
                ++counts[1];
            } else if (kind == "slot") {
                QCOMPARE(parts.size(), 7);
                const QByteArray network = networkId(parts.at(1));
                const quint64 epoch = parts.at(4).toULongLong();
                QCOMPARE(Friend::slot(network, hex(2), hex(3), epoch).toHex(), parts.at(5));
                QCOMPARE(Friend::key(network, hex(2), hex(3), epoch).toHex(), parts.at(6));
                ++counts[2];
            } else if (kind == "blob") {
                QCOMPARE(parts.size(), 9);
                const QByteArray network = networkId(parts.at(1));
                const quint64 epoch = parts.at(4).toULongLong();
                const Friend::Presence presence{parts.at(6).toULongLong(), hex(7)};
                QCOMPARE(Friend::seal(network, hex(2), hex(3), epoch, presence, hex(5)).toHex(), parts.at(8));
                const std::optional<Friend::Presence> opened = Friend::open(network, hex(2), hex(3), epoch, hex(8));
                QVERIFY(opened);
                QCOMPARE(opened->expiresAt, presence.expiresAt);
                QCOMPARE(opened->body, presence.body);
                ++counts[3];
            } else if (kind == "badblob") {
                QCOMPARE(parts.size(), 6);
                QVERIFY2(!Friend::open(networkId(parts.at(1)), hex(2), hex(3), parts.at(4).toULongLong(), hex(5)),
                         line.left(80).constData());
                ++counts[4];
            } else {
                QFAIL(kind.constData());
            }
        }
        for (int count : counts) QVERIFY(count >= 2);

        // 自分自身とは秘密を作らない。nonce は預けるたびに変わり、上限を超える知らせは預けない
        const SigningKey mine = SigningKey::generate();
        const SigningKey theirs = SigningKey::generate();
        QVERIFY(Friend::pairSecret(mine, mine.publicKey()).isEmpty());
        const QByteArray secret = Friend::pairSecret(mine, theirs.publicKey());
        QCOMPARE(secret.size(), 32);
        QCOMPARE(Friend::pairSecret(theirs, mine.publicKey()), secret);
        const QByteArray network = networkId("sbq-limited-1");
        const Friend::Presence presence{1794000000, QByteArray(200, 'x')};
        const QByteArray first = Friend::seal(network, secret, mine.publicKey(), 7, presence);
        const QByteArray second = Friend::seal(network, secret, mine.publicKey(), 7, presence);
        QVERIFY(!first.isEmpty() && first != second);
        QVERIFY(Friend::open(network, secret, mine.publicKey(), 7, first));
        QVERIFY(Friend::seal(network, secret, mine.publicKey(), 7, Friend::Presence{1, QByteArray(1024, 'x')}).isEmpty());
        QVERIFY(Friend::seal(network, secret, mine.publicKey(), 7, Friend::Presence{1, QByteArray()}).isEmpty());
        QCOMPARE(Friend::epochOf(1'794'000'599), quint64(2'990'000));
        QCOMPARE(Friend::epochOf(1'794'000'600), quint64(2'990'001));
    }

    /// 対局者の種別（人間・エンジン）を入れた Rules の4つの値。独立に作った試験入力と同じバイト列・ハッシュになる
    void rulesSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("rules.txt"));
        QCOMPARE(lines.size(), 4);
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QCOMPARE(parts.size(), 4);
            const std::optional<PlayerKind> black = playerKindFromLabel(parts.at(0));
            const std::optional<PlayerKind> white = playerKindFromLabel(parts.at(1));
            QVERIFY(black && white);
            const RulesTerms rules{*black, *white};
            QCOMPARE(Cbor::encode(rules.toCbor()).toHex(), parts.at(3));
            QCOMPARE(rules.hash().toHex(), parts.at(2));
            // 読み戻すと同じ値。双方人間の値だけが版 1 の Rules
            const std::optional<CborValue> value = Cbor::decode(QByteArray::fromHex(parts.at(3)));
            QVERIFY(value);
            QCOMPARE(RulesTerms::fromCbor(*value), std::optional<RulesTerms>(rules));
            QCOMPARE(Rules::isV1(*value), rules.isHumanOnly());
            QCOMPARE(rules.hash() == Rules::v1Hash(), rules.isHumanOnly());
        }
        // 自分の側と種別から作る（先手・後手の順に入る）
        QCOMPARE(RulesTerms::forSides(true, PlayerKind::Engine, PlayerKind::Human),
                 (RulesTerms{PlayerKind::Engine, PlayerKind::Human}));
        QCOMPARE(RulesTerms::forSides(false, PlayerKind::Engine, PlayerKind::Human),
                 (RulesTerms{PlayerKind::Human, PlayerKind::Engine}));
        // 知らない種別・ほかの要素の違う値は読まない
        QVERIFY(!playerKindFromLabel("robot"));
        std::vector<CborValue> items = RulesTerms{}.toCbor().items();
        items[8] = CborValue::text("robot");
        QVERIFY(!RulesTerms::fromCbor(CborValue::array(items)));
        items = RulesTerms{PlayerKind::Engine, PlayerKind::Human}.toCbor().items();
        items[3] = CborValue::uint(256);
        QVERIFY(!RulesTerms::fromCbor(CborValue::array(items)));
    }

    // ------------------------------------------------------------ 表示名

    void displayNameSharedVectors()
    {
        const QList<QByteArray> lines = P2pTest::readVectorLines(QStringLiteral("displayname.txt"));
        QVERIFY2(!lines.isEmpty(), "protocol/testvectors/displayname.txt is missing");
        for (const QByteArray& line : lines) {
            const QList<QByteArray> parts = line.split(' ');
            QVERIFY2(parts.size() >= 2, line.constData());
            const bool expected = parts.at(0) == "accept";
            QVERIFY2(DisplayName::isValidUtf8(QByteArray::fromHex(parts.at(1))) == expected, line.constData());
        }
    }

    void displayNameRules()
    {
        QVERIFY(DisplayName::isValid(QStringLiteral("将棋太郎")));
        QVERIFY(DisplayName::isValid(QStringLiteral("Taro 2")));
        QVERIFY(!DisplayName::isValid(QString()));
        QVERIFY(!DisplayName::isValid(QStringLiteral(" Taro")));
        QVERIFY(!DisplayName::isValid(QStringLiteral("Taro ")));
        QVERIFY(!DisplayName::isValid((QStringLiteral("a") + QChar(0x200B) + QStringLiteral("b"))));     // ゼロ幅
        QVERIFY(!DisplayName::isValid((QStringLiteral("a") + QChar(0x202E) + QStringLiteral("b"))));     // 双方向の制御
        QVERIFY(!DisplayName::isValid((QStringLiteral("か") + QChar(0x3099))));     // 結合する濁点
        QVERIFY(!DisplayName::isValid(QStringLiteral("「名人」")));       // かぎ括弧
        QVERIFY(!DisplayName::isValid(QStringLiteral("a\nb")));
        QVERIFY(DisplayName::isValid(QString(32, QLatin1Char('a'))));
        QVERIFY(!DisplayName::isValid(QString(33, QLatin1Char('a'))));
        QCOMPARE(DisplayName::comparisonKey(QStringLiteral("ＡＢＣabc")), QStringLiteral("abcabc"));
    }

    // ------------------------------------------------------------ 署名

    void signatureRfc8032()
    {
        // RFC 8032 7.1 の TEST 1・TEST 2
        struct Vector { const char* seed; const char* pk; const char* msg; const char* sig; };
        const Vector vectors[] = {
            {"9d61b19deffd5a60ba844af492ec2cc44449c5697b326919703bac031cae7f60",
             "d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a", "",
             "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b"},
            {"4ccd089b28ff96da9db6c346ec114e0f5b8a319f35aba624da8cf6ed4fb8a6fb",
             "3d4017c3e843895a92b70aa74d1b7ebc9c982ccf2ec4968cc0cd55f12af4660c", "72",
             "92a009a9f0d4cab8720e820b5f642540a2b27b5416503f8fb3762223ebdb69da085ac1e43e15996e458f3613d0f11d8c387b2eaeb4302aeeb00d291612bb0c00"},
        };
        for (const Vector& v : vectors) {
            SecureBytes seed(32);
            const QByteArray seedBytes = hex(v.seed);
            std::copy(seedBytes.cbegin(), seedBytes.cend(), seed.data());
            const SigningKey key = SigningKey::fromSeed(seed);
            QVERIFY(key.isValid());
            QCOMPARE(key.publicKey(), hex(v.pk));
            const QByteArray message = hex(v.msg);
            QCOMPARE(key.sign(message), hex(v.sig));
            QVERIFY(Crypto::verify(hex(v.pk), message, hex(v.sig)));
        }
    }

    void signatureRejectsMalleableAndSmallOrder()
    {
        const QByteArray pk = hex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
        const QByteArray sig = hex("e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e065224901555fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
        // S + L（正規の形でない S）
        const QByteArray malleable = sig.left(32) + addGroupOrder(sig.mid(32));
        QVERIFY(!Crypto::verify(pk, QByteArray(), malleable));
        // 位数の小さい公開鍵（単位元・位数8の点）
        const QByteArray identity = hex("0100000000000000000000000000000000000000000000000000000000000000");
        const QByteArray order8 = hex("c7176a703d4dd84fba3c0b760d10670f2a2053fa2c39ccc64ec7fd7792ac037a");
        QVERIFY(!Crypto::isValidPublicKey(identity));
        QVERIFY(!Crypto::isValidPublicKey(order8));
        const QByteArray smallOrderSignature = identity + QByteArray(32, '\0');
        QVERIFY(!Crypto::verify(identity, QByteArray("x"), smallOrderSignature));
        QVERIFY(!Crypto::verify(order8, QByteArray("x"), smallOrderSignature));
        // 位数の小さい R
        QVERIFY(!Crypto::verify(pk, QByteArray(), identity + sig.mid(32)));
        // 改変1バイト
        QByteArray tampered = sig;
        tampered[10] = static_cast<char>(tampered.at(10) ^ 1);
        QVERIFY(!Crypto::verify(pk, QByteArray(), tampered));
    }

    void signatureLabelsAreNotInterchangeable()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        const StartAgreement a = P2pTest::makeAgreement(black, white, TimeControlTerms{});
        const QByteArray gameId = a.gameId();
        // StateApproval への署名を、同じ値の Control として通さない（ラベルが違う）
        const CborValue approval = stateApproval(gameId, 0, zeroHash());
        const QByteArray signature = black.sessionKey.sign(approval);
        Control c;
        c.gameId = gameId;
        c.connectionId = zeroHash();
        c.sequence = 1;
        c.bodyHash = zeroHash();
        QVERIFY(Crypto::verify(black.sessionKey.publicKey(), approval, signature));
        QVERIFY(!Crypto::verify(black.sessionKey.publicKey(), c.toCbor(), signature));
        // 署名者を入れ替えると通らない
        QVERIFY(!Crypto::verify(white.sessionKey.publicKey(), approval, signature));
        // 別の networkId の合意は別の gameId
        StartAgreement other = a;
        other.networkId = networkId("sbq-limited-1");
        QVERIFY(other.gameId() != gameId);
    }

    // ------------------------------------------------------------ 値

    void agreementRoundTripAndDistinctKeys()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        StartAgreement a = P2pTest::makeAgreement(black, white, TimeControlTerms{});
        const std::optional<StartAgreement> decoded = StartAgreement::fromCbor(a.toCbor());
        QVERIFY(decoded);
        QCOMPARE(decoded->gameId(), a.gameId());
        // 同じプレイヤー同士・nonce の衝突は拒否する
        StartAgreement same = a;
        same.whitePlayerKey = same.blackPlayerKey;
        QVERIFY(!StartAgreement::fromCbor(same.toCbor()));
        StartAgreement nonce = a;
        nonce.whiteNonce = nonce.blackNonce;
        QVERIFY(!StartAgreement::fromCbor(nonce.toCbor()));
    }

    void timeControlBounds()
    {
        QVERIFY(TimeControlTerms::fromCbor(TimeControlTerms{600000, 30000}.toCbor()));
        QVERIFY(!TimeControlTerms::fromCbor(TimeControlTerms{0, 0}.toCbor()));
        QVERIFY(TimeControlTerms::fromCbor(TimeControlTerms{kMaxMainMs, kMaxByoyomiMs}.toCbor()));
        QVERIFY(!TimeControlTerms::fromCbor(TimeControlTerms{kMaxMainMs + 1, 0}.toCbor()));
        // 加算・遅延のある条件は v1 では拒否する
        const CborValue increment = CborValue::array({CborValue::text("SBQ-TIME"), CborValue::uint(1),
                                                      CborValue::uint(60000), CborValue::uint(0),
                                                      CborValue::uint(1000), CborValue::uint(0)});
        QVERIFY(!TimeControlTerms::fromCbor(increment));
    }

    void verificationCodeIsSymmetric()
    {
        const QByteArray a = Crypto::randomBytes(32);
        const QByteArray b = Crypto::randomBytes(32);
        const QStringList code = verificationCode(a, b);
        QCOMPARE(code.size(), 6);
        for (const QString& group : code) QCOMPARE(group.size(), 5);
        QCOMPARE(verificationCode(b, a), code);
    }

    void coinUsesBothNonces()
    {
        int applicantBlack = 0;
        const QByteArray nonceA = Crypto::randomBytes(32);
        for (int i = 0; i < 64; ++i) applicantBlack += applicantIsBlack(nonceA, Crypto::randomBytes(32)) ? 1 : 0;
        QVERIFY(applicantBlack > 0 && applicantBlack < 64);
        QCOMPARE(coinCommit(nonceA).size(), 32);
    }

    // ------------------------------------------------------------ 規則と時計

    void nextStateAndClock()
    {
        const TimeControlTerms terms{10000, 5000};
        const GameState s0 = GameRules::initialState(zeroHash(), terms);
        QCOMPARE(s0.sfen, Rules::initialSfen());
        // E < R + B なら指せる。基本持ち時間は max(0, R - E)
        const std::optional<GameState> s1 = GameRules::nextState(s0, UsiMove::fromString(u"7g7f"), 12000, terms);
        QVERIFY(s1);
        QCOMPARE(s1->blackMainMs, quint64(0));
        QCOMPARE(s1->whiteMainMs, quint64(10000));
        QCOMPARE(s1->sfen, QByteArray("lnsgkgsnl/1r5b1/ppppppppp/9/9/2P6/PP1PPPPPP/1B5R1/LNSGKGSNL w - 2"));
        QCOMPARE(GameRules::checkNext(s0, *s1, terms), GameRules::StateCheck::Ok);
        // 境界と同値は時間切れ
        QVERIFY(!GameRules::nextState(s0, UsiMove::fromString(u"7g7f"), 15000, terms));
        QVERIFY(GameRules::nextState(s0, UsiMove::fromString(u"7g7f"), 14999, terms));
        // 秒読みの未使用分は持ち越さない: 基本持ち時間0の側は次も B だけ
        const std::optional<GameState> s2 = GameRules::nextState(*s1, UsiMove::fromString(u"3c3d"), 100, terms);
        QVERIFY(s2);
        QVERIFY(!GameRules::nextState(*s2, UsiMove::fromString(u"2g2f"), 5000, terms));
        // 非合法手・自分の時間の書き換え・SFEN の書き換えは検査で落とす
        QVERIFY(!GameRules::nextState(s0, UsiMove::fromString(u"7g7e"), 0, terms));
        GameState forgedClock = *s1;
        forgedClock.blackMainMs = 10000;
        QCOMPARE(GameRules::checkNext(s0, forgedClock, terms), GameRules::StateCheck::ClockMismatch);
        GameState forgedSfen = *s1;
        forgedSfen.sfen.replace("w - 2", "w - 3");
        QCOMPARE(GameRules::checkNext(s0, forgedSfen, terms), GameRules::StateCheck::SfenMismatch);
        GameState wrongPrevious = *s1;
        wrongPrevious.previousStateHash = zeroHash();
        QCOMPARE(GameRules::checkNext(s0, wrongPrevious, terms), GameRules::StateCheck::WrongPrevious);
    }

    void judgeTerminal()
    {
        GameState mated;
        mated.ply = 3;
        mated.sfen = "4k4/4G4/4S4/9/9/9/9/9/4K4 w - 4";
        const std::optional<GameRules::Terminal> mate = GameRules::judge({mated});
        QVERIFY(mate);
        QCOMPARE(mate->cause, Cause::Checkmate);
        QCOMPARE(mate->outcome, Outcome::BlackWins);

        const P2pTest::Player black;
        const P2pTest::Player white;
        const TimeControlTerms terms;
        const std::vector<SignedState> signedStates =
            P2pTest::playMoves(zeroHash(), terms, P2pTest::repetitionMoves(), black, white);
        QCOMPARE(signedStates.size(), std::size_t(13));
        std::vector<GameState> states;
        for (const SignedState& s : signedStates) states.push_back(s.state);
        const std::optional<GameRules::Terminal> repetition = GameRules::judge(states);
        QVERIFY(repetition);
        QCOMPARE(repetition->cause, Cause::Repetition);
        states.pop_back();
        QVERIFY(!GameRules::judge(states));

        GameState last;
        last.ply = kMaxPly;
        last.sfen = "lnsgkgsnl/1r5b1/ppppppppp/9/9/9/PPPPPPPPP/1B5R1/LNSGKGSNL b - 513";
        const std::optional<GameRules::Terminal> maxPly = GameRules::judge({last});
        QVERIFY(maxPly);
        QCOMPARE(maxPly->cause, Cause::MaxPly);
        QVERIFY(!GameRules::nextState(last, UsiMove::fromString(u"7g7f"), 0, terms));
    }

    // ------------------------------------------------------------ 証跡

    void evidenceMutuallySignedResign()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        Evidence e = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f"), QStringLiteral("3c3d")});
        e.result = P2pTest::resultFor(e, Outcome::WhiteWins, Cause::Resign);
        e.blackResultSignature = black.playerKey.sign(e.result->toCbor());
        e.whiteResultSignature = white.playerKey.sign(e.result->toCbor());

        const QByteArray bytes = e.encode();
        const EvidenceReport report = EvidenceVerifier::verify(bytes);
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::MutuallySigned);
        QCOMPARE(report.lastMutualPly, 2);
        QCOMPARE(report.resultDigest, e.result->digest());
        QCOMPARE(report.networkName, QStringLiteral("sbq-local-1"));
        QCOMPARE(report.gameId, e.gameId());

        // 読み直した証跡は同じバイト列になる
        const std::optional<Evidence> decoded = Evidence::decode(bytes);
        QVERIFY(decoded);
        QCOMPARE(decoded->encode(), bytes);
    }

    void evidenceWithoutResultSignature()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        // 投了は棋譜から決まらないので、相手の署名がなければ結果未確定
        Evidence e = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f")});
        e.result = P2pTest::resultFor(e, Outcome::BlackWins, Cause::Resign);
        e.whiteResultSignature = white.playerKey.sign(e.result->toCbor());
        EvidenceReport report = EvidenceVerifier::verify(e.encode());
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::Unconfirmed);

        // 千日手は棋譜から決まる（結果への署名がなくても）
        Evidence draw = P2pTest::makeEvidence(black, white, TimeControlTerms{}, P2pTest::repetitionMoves());
        report = EvidenceVerifier::verify(draw.encode());
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::DeterminedByRecord);
        QVERIFY(report.terminal);
        QCOMPARE(report.terminal->cause, Cause::Repetition);

        // 千日手の局面に付いた投了は食い違いとして示す
        draw.result = P2pTest::resultFor(draw, Outcome::BlackWins, Cause::Resign);
        draw.blackResultSignature = black.playerKey.sign(draw.result->toCbor());
        draw.whiteResultSignature = white.playerKey.sign(draw.result->toCbor());
        report = EvidenceVerifier::verify(draw.encode());
        QVERIFY(report.resultConflictsWithRecord);
        QVERIFY(!report.isValid());
        // 双方の署名がそろっていても「双方が確定した記録」とは区分しない（棋譜から決まる記録として示す）
        QCOMPARE(report.category, EvidenceReport::Category::DeterminedByRecord);
    }

    /// 知らないネットワークの証跡は、このビルドではその規則を確かめられないので有効と言わない（監査 2026-11-13 I-u）
    void evidenceOfUnknownNetworkIsNotValid()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        const TimeControlTerms terms{60'000, 10'000};
        Evidence e;
        e.agreement = P2pTest::makeAgreement(black, white, terms);
        e.agreement.networkId = networkId("sbq-unknown-9");
        e.timeControl = terms;
        e.blackStartSignature = black.playerKey.sign(e.agreement.toCbor());
        e.whiteStartSignature = white.playerKey.sign(e.agreement.toCbor());
        e.states = P2pTest::playMoves(e.gameId(), terms, {QStringLiteral("7g7f")}, black, white);
        EvidenceReport report = EvidenceVerifier::verify(e.encode());
        QVERIFY(!report.networkKnown);
        QVERIFY(!report.isValid());
        QCOMPARE(report.error, QStringLiteral("unknown network"));
        // 知っているネットワークなら、同じ手順は有効
        e.agreement.networkId = networkId("sbq-local-1");
        e.blackStartSignature = black.playerKey.sign(e.agreement.toCbor());
        e.whiteStartSignature = white.playerKey.sign(e.agreement.toCbor());
        e.states = P2pTest::playMoves(e.gameId(), terms, {QStringLiteral("7g7f")}, black, white);
        report = EvidenceVerifier::verify(e.encode());
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QVERIFY(report.networkKnown);
    }

    void evidenceRejectsTampering()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        const P2pTest::Player stranger;
        const Evidence base = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f"), QStringLiteral("3c3d")});

        // 承認の署名の改変
        Evidence e = base;
        e.states[1].whiteApproval[0] = static_cast<char>(e.states[1].whiteApproval.at(0) ^ 1);
        QVERIFY(!EvidenceVerifier::verify(e.encode()).signaturesValid);

        // 別の一局用鍵による承認
        e = base;
        e.states[2] = P2pTest::approveBoth(e.states[2].state, black, stranger);
        QVERIFY(!EvidenceVerifier::verify(e.encode()).signaturesValid);

        // 開始署名を別の鍵で
        e = base;
        e.whiteStartSignature = stranger.playerKey.sign(e.agreement.toCbor());
        QVERIFY(!EvidenceVerifier::verify(e.encode()).whiteStartSigned);

        // 他の対局の状態の流用
        const Evidence other = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f"), QStringLiteral("3c3d")});
        e = base;
        e.states[2] = other.states[2];
        QVERIFY(!EvidenceVerifier::verify(e.encode()).recordLegal);

        // 承認のない状態の後に状態を続けない
        e = base;
        e.states[1].whiteApproval.clear();
        QVERIFY(!EvidenceVerifier::verify(e.encode()).recordLegal);

        // 末尾の1バイトの改変・上限を超える大きさ
        QByteArray bytes = base.encode();
        bytes[bytes.size() - 1] = static_cast<char>(bytes.at(bytes.size() - 1) ^ 1);
        QVERIFY(!EvidenceVerifier::verify(bytes).isValid());
        QVERIFY(!EvidenceVerifier::verify(QByteArray(Evidence::kMaxBytes + 1, '\0')).formatValid);
    }

    /// shogiboardq-cli verify-match の結果（ファイルから）
    void verifyMatchServiceReadsFile()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        Evidence e = P2pTest::makeEvidence(black, white, TimeControlTerms{}, {QStringLiteral("7g7f")});
        e.result = P2pTest::resultFor(e, Outcome::BlackWins, Cause::Resign);
        e.blackResultSignature = black.playerKey.sign(e.result->toCbor());
        e.whiteResultSignature = white.playerKey.sign(e.result->toCbor());
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("game.sbqmatch"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(e.encode());
        file.close();
        const auto outcome = MatchVerificationService::verifyFile(path);
        QVERIFY(std::holds_alternative<QJsonObject>(outcome));
        const QJsonObject obj = std::get<QJsonObject>(outcome);
        QCOMPARE(obj.value(QStringLiteral("valid")).toBool(), true);
        QCOMPARE(obj.value(QStringLiteral("category")).toString(), QStringLiteral("mutually_signed"));
        QCOMPARE(obj.value(QStringLiteral("game_id")).toString(), QString::fromLatin1(e.gameId().toHex()));
        QCOMPARE(obj.value(QStringLiteral("result")).toObject().value(QStringLiteral("result_digest")).toString(),
                 QString::fromLatin1(e.result->digest().toHex()));
        QVERIFY(std::holds_alternative<Failure>(MatchVerificationService::verifyFile(dir.filePath(QStringLiteral("none")))));
    }

    /// エンジンが指した対局の証跡は、対局者の種別を条件として持ち、開始合意の rulesHash と照らす
    void evidenceKeepsPlayerKinds()
    {
        const P2pTest::Player black;
        const P2pTest::Player white;
        const TimeControlTerms terms{};
        Evidence e;
        e.rules = RulesTerms{PlayerKind::Human, PlayerKind::Engine};
        e.agreement = P2pTest::makeAgreement(black, white, terms);
        e.agreement.rulesHash = e.rules.hash();
        e.timeControl = terms;
        e.blackStartSignature = black.playerKey.sign(e.agreement.toCbor());
        e.whiteStartSignature = white.playerKey.sign(e.agreement.toCbor());
        e.states = P2pTest::playMoves(e.gameId(), terms, {QStringLiteral("7g7f"), QStringLiteral("3c3d")}, black, white);
        e.result = P2pTest::resultFor(e, Outcome::WhiteWins, Cause::Resign);
        e.blackResultSignature = black.playerKey.sign(e.result->toCbor());
        e.whiteResultSignature = white.playerKey.sign(e.result->toCbor());

        const QByteArray bytes = e.encode();
        const std::optional<Evidence> decoded = Evidence::decode(bytes);
        QVERIFY(decoded);
        QCOMPARE(decoded->rules, e.rules);
        QCOMPARE(decoded->encode(), bytes);
        EvidenceReport report = EvidenceVerifier::verify(bytes);
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::MutuallySigned);
        QCOMPARE(report.rules, e.rules);

        // 証跡の種別だけを書き換えると、署名された開始合意の rulesHash と合わない
        Evidence tampered = e;
        tampered.rules = RulesTerms{};
        report = EvidenceVerifier::verify(tampered.encode());
        QVERIFY(!report.conditionsMatch);
        QVERIFY(!report.isValid());

        // verify-match の結果にも種別を出す
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("engine.sbqmatch"));
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(bytes);
        file.close();
        const auto outcome = MatchVerificationService::verifyFile(path);
        QVERIFY(std::holds_alternative<QJsonObject>(outcome));
        const QJsonObject obj = std::get<QJsonObject>(outcome);
        QCOMPARE(obj.value(QStringLiteral("valid")).toBool(), true);
        QCOMPARE(obj.value(QStringLiteral("black_player_kind")).toString(), QStringLiteral("human"));
        QCOMPARE(obj.value(QStringLiteral("white_player_kind")).toString(), QStringLiteral("engine"));
    }

    // ------------------------------------------------------------ フレーム

    /// HELLO の対応する版の一覧と、募集側の対局者の種別（版 2）
    void helloVersionsAndPlayerKind()
    {
        HelloPayload hello;
        hello.role = kRoleHost;
        hello.networkId = networkId("sbq-local-1");
        hello.peerId = QByteArray(38, '\x01');
        hello.connectionRandom = Crypto::randomBytes(32);
        hello.playerKey = Crypto::randomBytes(32);
        hello.sessionKey = Crypto::randomBytes(32);
        hello.displayName = QStringLiteral("将棋太郎");
        hello.offerId = zeroHash();
        hello.coin = Crypto::randomBytes(32);
        hello.timeControl = TimeControlTerms{};

        // 人間の HELLO は版 1 と同じ形（14 要素・値の版 1）で、対応する版の一覧だけが [1, 2]。
        // 版 1 だけの実装（一覧に 1 があれば受け付ける）も読める
        const CborValue human = hello.toCbor();
        QCOMPARE(human.items().size(), std::size_t(14));
        QCOMPARE(human.items()[1], CborValue::uint(1));
        QCOMPARE(human.items()[4], CborValue::array({CborValue::uint(1), CborValue::uint(2)}));
        std::optional<HelloPayload> parsed = HelloPayload::fromCbor(human);
        QVERIFY(parsed);
        QVERIFY(parsed->supportsPlayerKinds);
        QCOMPARE(parsed->playerKind, PlayerKind::Human);

        // 版 1 だけの実装の HELLO（一覧が [1]）は、今までと同じバイト列で、対局者の種別を扱えない相手として読む
        HelloPayload old = hello;
        old.supportsPlayerKinds = false;
        QCOMPARE(old.toCbor().items()[4], CborValue::array({CborValue::uint(1)}));
        parsed = HelloPayload::fromCbor(old.toCbor());
        QVERIFY(parsed);
        QVERIFY(!parsed->supportsPlayerKinds);
        QCOMPARE(parsed->playerKind, PlayerKind::Human);

        // エンジンで指す募集側の HELLO は、末尾に種別を足した版 2 の形。条件の Rules は種別を入れない値のまま
        HelloPayload engine = hello;
        engine.playerKind = PlayerKind::Engine;
        const CborValue withKind = engine.toCbor();
        QCOMPARE(withKind.items().size(), std::size_t(15));
        QCOMPARE(withKind.items()[1], CborValue::uint(2));
        QCOMPARE(withKind.items()[14], CborValue::text("engine"));
        QVERIFY(Rules::isV1(withKind.items()[13].items()[0]));
        parsed = HelloPayload::fromCbor(withKind);
        QVERIFY(parsed);
        QCOMPARE(parsed->playerKind, PlayerKind::Engine);
        QVERIFY(parsed->supportsPlayerKinds);
        QVERIFY(parsed->timeControl);

        // 版 2 の形で断るもの: 知らない種別・一覧に版 2 がない・申込側（申込側の種別は START_AGREEMENT で伝える）
        std::vector<CborValue> items = withKind.items();
        items[14] = CborValue::text("robot");
        QVERIFY(!HelloPayload::fromCbor(CborValue::array(items)));
        items = withKind.items();
        items[4] = CborValue::array({CborValue::uint(1)});
        QVERIFY(!HelloPayload::fromCbor(CborValue::array(items)));
        HelloPayload applicant = engine;
        applicant.role = kRoleApplicant;
        applicant.timeControl.reset();
        QVERIFY(!HelloPayload::fromCbor(applicant.toCbor()));
        // 値の版と要素の数が合わない形
        items = withKind.items();
        items[1] = CborValue::uint(1);
        QVERIFY(!HelloPayload::fromCbor(CborValue::array(items)));
        items = human.items();
        items[1] = CborValue::uint(2);
        QVERIFY(!HelloPayload::fromCbor(CborValue::array(items)));

        // START_AGREEMENT は Rules を運び、開始合意の rulesHash と一致しなければ読まない
        const P2pTest::Player black;
        const P2pTest::Player white;
        StartAgreementPayload payload;
        payload.rules = RulesTerms{PlayerKind::Engine, PlayerKind::Human};
        payload.timeControl = TimeControlTerms{};
        payload.agreement = P2pTest::makeAgreement(black, white, payload.timeControl);
        QVERIFY(!StartAgreementPayload::fromCbor(payload.toCbor()));   // rulesHash は双方人間の値のまま
        payload.agreement.rulesHash = payload.rules.hash();
        const std::optional<StartAgreementPayload> read = StartAgreementPayload::fromCbor(payload.toCbor());
        QVERIFY(read);
        QCOMPARE(read->rules, payload.rules);
        // 双方人間の START_AGREEMENT は、版 1 と同じバイト列
        StartAgreementPayload humanPayload;
        humanPayload.timeControl = TimeControlTerms{};
        humanPayload.agreement = P2pTest::makeAgreement(black, white, humanPayload.timeControl);
        QCOMPARE(humanPayload.toCbor().items()[1], Rules::v1());
        QVERIFY(StartAgreementPayload::fromCbor(humanPayload.toCbor()));
    }

    void framePayloadsRoundTrip()
    {
        HelloPayload hello;
        hello.role = kRoleHost;
        hello.networkId = networkId("sbq-local-1");
        hello.peerId = QByteArray(38, '\x01');
        hello.connectionRandom = Crypto::randomBytes(32);
        hello.playerKey = Crypto::randomBytes(32);
        hello.sessionKey = Crypto::randomBytes(32);
        hello.displayName = QStringLiteral("将棋太郎");
        hello.offerId = zeroHash();
        hello.coin = Crypto::randomBytes(32);
        hello.timeControl = TimeControlTerms{};
        Frame frame;
        frame.type = FrameType::Hello;
        frame.requestId = 1;
        frame.gameId = zeroHash();
        frame.payload = hello.toCbor();
        const std::optional<Frame> decoded = Frame::decode(frame.encode());
        QVERIFY(decoded);
        const std::optional<HelloPayload> h = HelloPayload::fromCbor(decoded->payload);
        QVERIFY(h);
        QCOMPARE(h->displayName, hello.displayName);
        QVERIFY(h->timeControl);

        // 規則に合わない表示名・申込側の条件は拒否する
        HelloPayload badName = hello;
        badName.displayName = (QStringLiteral("a") + QChar(0x202E) + QStringLiteral("b"));
        QVERIFY(!HelloPayload::fromCbor(badName.toCbor()));
        // 先頭の BOM は QString にすると消えるが、受け取ったバイト列で判定するので通らない
        HelloPayload bom = hello;
        bom.displayName = QString(QChar(0xFEFF)) + QStringLiteral("a");
        QVERIFY(!HelloPayload::fromCbor(bom.toCbor()));
        HelloPayload applicantWithConditions = hello;
        applicantWithConditions.role = kRoleApplicant;
        QVERIFY(!HelloPayload::fromCbor(applicantWithConditions.toCbor()));

        // INVITE_TOKEN は対局・UI側では読まない
        Frame token;
        token.type = FrameType::InviteToken;
        token.gameId = zeroHash();
        token.payload = CborValue::array({CborValue::bytes(QByteArray(32, 'x'))});
        QVERIFY(Frame::isWellFormedEnvelope(token.encode()));
        QVERIFY(!Frame::decode(token.encode()));

        // CONTROL の bodyHash の不一致
        ControlPayload control;
        control.control.gameId = zeroHash();
        control.control.connectionId = zeroHash();
        control.control.sequence = 1;
        control.control.kind = ControlKind::Ready;
        control.body = ControlBody::ready(zeroHash());
        control.control.bodyHash = Cbor::hash(control.body);
        control.signature = QByteArray(64, '\0');
        QVERIFY(ControlPayload::fromCbor(control.toCbor()));
        control.body = ControlBody::ready(QByteArray(32, '\x01'));
        QVERIFY(!ControlPayload::fromCbor(control.toCbor()));
    }
};

QTEST_GUILESS_MAIN(TestP2pProtocol)
#include "tst_p2p_protocol.moc"
