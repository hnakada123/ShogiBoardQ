/// @file tst_p2p_coordinator.cpp
/// @brief P2P対局の司令塔の結合試験（2つの司令塔を通信プロセスなしでつなぎ、盤・時計・棋譜・動作状態・証跡まで確かめる）

#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "appactivitystate.h"
#include "gameboard.h"
#include "matchrecordsink.h"
#include "p2pevidenceverifier.h"
#include "p2pgamecoordinator.h"
#include "p2pknownpeers.h"
#include "p2precordkinds.h"
#include "p2precordstore.h"
#include "p2ptransport.h"
#include "shogiboard.h"
#include "shogiclock.h"

using namespace P2p;

namespace {

/// 棋譜の受け手（書かれた行を覚える）
class FakeSink final : public MatchRecordSink
{
public:
    void beginRecord(const MatchRecordStart& start) override
    {
        started = start;
        entries.clear();
        finished = false;
    }
    bool canAppendRecord() const override { return !finished; }
    void appendRecord(const RecordEntry& entry) override { entries.append(entry); }
    bool canUndoRecords(int) const override { return false; }
    void undoRecords(int) override {}
    void finishRecord(bool) override { finished = true; }

    MatchRecordStart started;
    QList<RecordEntry> entries;
    bool finished = false;
};

class Side;

/// 2つの司令塔をつなぐ偽の通信の窓口（フレームは次のイベントループで相手へ渡す）
class FakeTransport : public P2pTransport
{
public:
    QByteArray peerId;
    Side* self = nullptr;
    Side* other = nullptr;
    bool connected = true;

    QByteArray localPeerId() const override { return peerId; }
    QByteArray networkId() const override { return P2p::networkId("sbq-local-1"); }
    bool sendFrame(quint64 connection, const QByteArray& frame) override;
    void closeConnection(quint64) override {}
    void setGamePeer(const QString& id) override { gamePeer = id; }
    void reconnectGamePeer(const QString&) override {}

    QString gamePeer;
};

class Side
{
public:
    explicit Side(const QByteArray& peerId, const QString& root)
        : store(root)
        , knownPeers(root + QStringLiteral("/peers.cbor"))
        , coordinator(P2pGameCoordinator::Deps{&board, &clock, &activity})
    {
        transport.peerId = peerId;
        transport.self = this;
        coordinator.setRecordSink(&sink);
        coordinator.setEnvironment(&transport, &store, &knownPeers, &playerKey);
        QVERIFY(store.lock());
    }

    GameBoard board;
    ShogiClock clock;
    AppActivityState activity;
    FakeSink sink;
    FakeTransport transport;
    SigningKey playerKey = SigningKey::generate();
    P2pRecordStore store;
    P2pKnownPeers knownPeers;
    P2pGameCoordinator coordinator;
};

bool FakeTransport::sendFrame(quint64 connection, const QByteArray& frame)
{
    if (!connected || !other) return false;
    Side* target = other;
    QMetaObject::invokeMethod(
        &target->coordinator, [target, connection, frame]() { target->coordinator.onFrame(connection, frame); },
        Qt::QueuedConnection);
    return true;
}

P2pGameCoordinator::NegotiationRequest request(quint8 role, const QByteArray& opponentPeerId, const QString& name)
{
    P2pGameCoordinator::NegotiationRequest r;
    r.connection = 1;
    r.opponentPeerId = QString::fromLatin1(opponentPeerId.toHex());
    r.opponentPeerIdBytes = opponentPeerId;
    r.role = role;
    r.myDisplayName = name;
    r.hostTerms = TimeControlTerms{60'000, 10'000};
    return r;
}

} // namespace

class TestP2pCoordinator : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_dir;

    void settle()
    {
        for (int i = 0; i < 50; ++i) QCoreApplication::processEvents();
    }

private slots:
    void initTestCase()
    {
        QVERIFY(Crypto::initialize());
        QVERIFY(m_dir.isValid());
    }

    void inviteGameFromStartToEvidence()
    {
        const QByteArray applicantId(38, 'a');
        const QByteArray hostId(38, 'h');
        Side applicant(applicantId, m_dir.filePath(QStringLiteral("a")));
        Side host(hostId, m_dir.filePath(QStringLiteral("h")));
        applicant.transport.other = &host;
        host.transport.other = &applicant;

        QSignalSpy hostStarted(&host.coordinator, &P2pGameCoordinator::gameStarted);
        QSignalSpy applicantFinished(&applicant.coordinator, &P2pGameCoordinator::gameFinished);
        QSignalSpy hostFinished(&host.coordinator, &P2pGameCoordinator::gameFinished);

        QVERIFY(host.coordinator.startNegotiation(request(kRoleHost, applicantId, QStringLiteral("募集"))));
        QVERIFY(applicant.coordinator.startNegotiation(request(kRoleApplicant, hostId, QStringLiteral("申込"))));
        settle();
        QCOMPARE(applicant.coordinator.negotiation()->stage(), Negotiation::Stage::Confirming);
        QCOMPARE(host.coordinator.negotiation()->stage(), Negotiation::Stage::Confirming);
        QCOMPARE(applicant.coordinator.opponentDisplayName(), QStringLiteral("募集"));
        // 対局はまだ始まっていない（待機のまま）
        QVERIFY(applicant.activity.is(AppActivityState::Activity::Idle));

        host.coordinator.confirmStart();
        applicant.coordinator.confirmStart();
        settle();
        QCOMPARE(hostStarted.count(), 1);
        QVERIFY(applicant.activity.is(AppActivityState::Activity::DuringP2pGame));
        QVERIFY(host.activity.is(AppActivityState::Activity::DuringP2pGame));
        QCOMPARE(applicant.transport.gamePeer, QString::fromLatin1(hostId.toHex()));
        // 棋譜の対局者は双方が名乗った表示名、対局情報に対局ID
        Side& black = applicant.coordinator.match()->iAmBlack() ? applicant : host;
        Side& white = applicant.coordinator.match()->iAmBlack() ? host : applicant;
        const QStringList names{black.sink.started.players.humanName(Turn::Black),
                                black.sink.started.players.humanName(Turn::White)};
        QVERIFY(names.contains(QStringLiteral("申込")) && names.contains(QStringLiteral("募集")));
        QCOMPARE(white.sink.started.players.humanName(Turn::Black), names.at(0));
        QCOMPARE(black.sink.started.extraInfo.size(), 1);
        QCOMPARE(black.sink.started.extraInfo.first().second, QString::fromLatin1(black.coordinator.match()->gameId().toHex()));

        // 手番でない側の手は受け付けない。送った手は相手の承認を受けてから盤に指す
        QCOMPARE(white.coordinator.playMove(UsiMove::fromString(u"3c3d")), GameChannel::MoveResult::Rejected);
        QVERIFY(black.coordinator.isHumanToMove());
        // 1秒を超えて考えてから指す。署名する消費時間はミリ秒の端数を持つ
        QTest::qWait(1200);
        QCOMPARE(black.coordinator.playMove(UsiMove::fromString(u"7g7f")), GameChannel::MoveResult::Sent);
        QVERIFY(!black.coordinator.isHumanToMove());
        settle();
        QCOMPARE(black.board.board()->position().toSfen(), white.board.board()->position().toSfen());
        QCOMPARE(black.sink.entries.size(), 1);
        QCOMPARE(white.sink.entries.size(), 1);
        // 棋譜にはローカル対局と同じく秒の単位で書く（「00:01/00:00:01」。ミリ秒の端数で棋譜欄の幅が変わらない）
        for (const Side* s : {&black, &white}) {
            const RecordTime& time = s->sink.entries.first().time;
            QVERIFY2(time.elapsedMs >= 1000 && time.elapsedMs % 1000 == 0, qPrintable(time.toText()));
            QCOMPARE(time.totalMs, time.elapsedMs);
            QVERIFY2(!time.toText().contains(QLatin1Char('.')), qPrintable(time.toText()));
        }
        QVERIFY(black.coordinator.match()->confirmedStates().back().state.spentMs >= 1200u);
        QVERIFY(white.coordinator.isHumanToMove());
        QCOMPARE(white.coordinator.playMove(UsiMove::fromString(u"3c3d")), GameChannel::MoveResult::Sent);
        settle();
        QCOMPARE(black.sink.entries.size(), 2);

        // 先手の投了: 結果の一致で棋譜を確定し、P2P対局中を手放す。両方の署名がそろって証跡を保存する
        QVERIFY(black.coordinator.canResign());
        black.coordinator.resign();
        settle();
        QCOMPARE(applicantFinished.count(), 1);
        QCOMPARE(hostFinished.count(), 1);
        for (Side* s : {&applicant, &host}) {
            QVERIFY(s->activity.is(AppActivityState::Activity::Idle));
            QVERIFY(s->sink.finished);
            QVERIFY(s->sink.entries.last().isTerminal());
            QCOMPARE(s->sink.entries.last().terminal, TerminalType::Resign);
            QCOMPARE(s->coordinator.match()->settlement(), Settlement::MutuallySigned);
            const QString path = s->coordinator.lastEvidencePath();
            QVERIFY(QFile::exists(path));
            QFile file(path);
            QVERIFY(file.open(QIODevice::ReadOnly));
            const EvidenceReport report = EvidenceVerifier::verify(file.readAll());
            QVERIFY2(report.isValid(), qPrintable(report.error));
            QCOMPARE(report.category, EvidenceReport::Category::MutuallySigned);
            // 既知の相手として、署名で確かめたプレイヤーID を数える
            QVERIFY(s->knownPeers.find(s == &applicant ? host.playerKey.publicKey() : applicant.playerKey.publicKey()));
            // ジャーナルは消え、履歴に1件
            const QList<P2pRecordStore::Entry> history = s->store.history();
            QCOMPARE(history.size(), 1);
            QVERIFY(!history.first().hasJournal);
            QCOMPARE(history.first().info.settlement, Settlement::MutuallySigned);
        }
        QVERIFY(!applicant.coordinator.isBusy());
    }

    void cancelBeforeStartReturnsToIdle()
    {
        const QByteArray applicantId(38, 'b');
        const QByteArray hostId(38, 'i');
        Side applicant(applicantId, m_dir.filePath(QStringLiteral("a2")));
        Side host(hostId, m_dir.filePath(QStringLiteral("h2")));
        applicant.transport.other = &host;
        host.transport.other = &applicant;
        QSignalSpy applicantEnded(&applicant.coordinator, &P2pGameCoordinator::negotiationEnded);
        QSignalSpy hostEnded(&host.coordinator, &P2pGameCoordinator::negotiationEnded);
        QVERIFY(host.coordinator.startNegotiation(request(kRoleHost, applicantId, QStringLiteral("募集"))));
        QVERIFY(applicant.coordinator.startNegotiation(request(kRoleApplicant, hostId, QStringLiteral("申込"))));
        settle();
        host.coordinator.cancelNegotiation();
        settle();
        QCOMPARE(applicantEnded.count(), 1);
        QCOMPARE(applicantEnded.first().at(0).toBool(), false);
        QCOMPARE(hostEnded.count(), 1);
        QCOMPARE(hostEnded.first().at(0).toBool(), false);
        // 取りやめた後は、同じ相手とすぐに交渉をやり直せる（招待は1回ごとなので、再申込みの拒否は持たない）
        QVERIFY(host.coordinator.startNegotiation(request(kRoleHost, applicantId, QStringLiteral("募集"))));
        QVERIFY(applicant.coordinator.startNegotiation(request(kRoleApplicant, hostId, QStringLiteral("申込"))));
        settle();
        QCOMPARE(host.coordinator.negotiation()->stage(), Negotiation::Stage::Confirming);
        host.coordinator.cancelNegotiation();
        settle();
        QVERIFY(!applicant.coordinator.isBusy());
        QVERIFY(!host.coordinator.isBusy());
        QVERIFY(applicant.activity.is(AppActivityState::Activity::Idle));
    }

    void journalIsRecoveredAsUnconfirmedRecord()
    {
        // 対局の途中でアプリが終わった: ジャーナルを証跡に直す（対局は続けない）
        const QString root = m_dir.filePath(QStringLiteral("recover"));
        P2pRecordStore store(root);
        QVERIFY(store.lock());
        const SigningKey black = SigningKey::generate();
        const SigningKey white = SigningKey::generate();
        StartAgreement a;
        a.networkId = networkId("sbq-local-1");
        a.blackNonce = Crypto::randomBytes(32);
        a.whiteNonce = Crypto::randomBytes(32);
        a.blackPlayerKey = black.publicKey();
        a.whitePlayerKey = white.publicKey();
        a.blackSessionKey = Crypto::randomBytes(32);
        a.whiteSessionKey = Crypto::randomBytes(32);
        a.blackPeerIdHash = Cbor::sha256("x");
        a.whitePeerIdHash = Cbor::sha256("y");
        a.rulesHash = Rules::v1Hash();
        a.timeControlHash = TimeControlTerms{}.hash();
        a.networkPolicyHash = NetworkPolicy::v1().hash();
        a.startBefore = 1;
        std::unique_ptr<P2pJournal> journal = store.createJournal(a.gameId());
        QVERIFY(journal);
        QVERIFY(journal->append(RecordKinds::start(a, TimeControlTerms{}, kSideBlack, QStringLiteral("相手"))));
        journal->close();
        // 途中で切れた末尾は読まない
        QFile file(journal->path());
        QVERIFY(file.open(QIODevice::Append));
        file.write(QByteArray("\x00\x00\x10\x00garbage", 11));
        file.close();
        QCOMPARE(store.recoverJournals(), 1);
        const QList<P2pRecordStore::Entry> history = store.history();
        QCOMPARE(history.size(), 1);
        QCOMPARE(history.first().info.settlement, Settlement::Interrupted);
        QCOMPARE(history.first().gameId, a.gameId());
        // 2つ目のアプリはロックを取れない
        P2pRecordStore second(root);
        QVERIFY(!second.lock());
    }
};

QTEST_MAIN(TestP2pCoordinator)
#include "tst_p2p_coordinator.moc"
