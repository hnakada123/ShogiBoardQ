/// @file tst_p2p_coordinator.cpp
/// @brief P2P対局の司令塔の結合試験（2つの司令塔を通信プロセスなしでつなぎ、盤・時計・棋譜・動作状態・証跡まで確かめる）

#include <QtTest>

#include <QSignalSpy>
#include <QTemporaryDir>

#include "appactivitystate.h"
#include "engineprocessmanager.h"
#include "engineslots.h"
#include "gameboard.h"
#include "matchrecordsink.h"
#include "p2pevidenceverifier.h"
#include "p2pgamecoordinator.h"
#include "p2pknownpeers.h"
#include "p2pplayerprofile.h"
#include "p2precordkinds.h"
#include "p2precordstore.h"
#include "p2ptransport.h"
#include "shogiboard.h"
#include "shogiclock.h"
#include "usi.h"

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
        , coordinator(P2pGameCoordinator::Deps{&board, &clock, &activity, &engines})
    {
        transport.peerId = peerId;
        transport.self = this;
        coordinator.setRecordSink(&sink);
        coordinator.setEnvironment(&transport, &store, &knownPeers, &playerKey);
    }

    GameBoard board;
    ShogiClock clock;
    AppActivityState activity;
    EngineSlots engines;
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

/// 対局用のモックのエンジン（最初の探索に 8c8d、次からは 3c3d を返す）で指す対局者
P2pGameCoordinator::PlayerSetup mockEngine()
{
    P2pGameCoordinator::PlayerSetup player;
    player.kind = PlayerKind::Engine;
    player.enginePath = QCoreApplication::applicationDirPath() + QStringLiteral("/mock_usi_match");
    player.engineName = QStringLiteral("P2P test engine");
    return player;
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
        QSignalSpy blackSounds(&black.board, &GameBoard::piecePlaced);
        QSignalSpy whiteSounds(&white.board, &GameBoard::piecePlaced);
        const QString startSfen = black.board.board()->position().toSfen();
        QVERIFY(black.clock.isRunning());
        QCOMPARE(black.coordinator.playMove(UsiMove::fromString(u"7g7f")), GameChannel::MoveResult::Sent);
        QVERIFY(!black.coordinator.isHumanToMove());
        // 駒を置いた時点に合わせる: 送ったときに駒音を知らせ、自分の時計を署名した残り時間で止める
        // （盤と棋譜は相手の承認を受けてから）
        QCOMPARE(blackSounds.count(), 1);
        QCOMPARE(blackSounds.first().at(0).value<Turn>(), Turn::Black);
        QCOMPARE(blackSounds.first().at(1).toInt(), 1);
        QVERIFY(!black.clock.isRunning());
        const GameState* sent = black.coordinator.match()->pendingState();
        QVERIFY(sent != nullptr);
        QVERIFY(sent->blackMainMs <= 60'000u - 1200u);
        QCOMPARE(black.clock.remainingMs(Turn::Black), static_cast<qint64>(sent->blackMainMs));
        QCOMPARE(black.board.board()->position().toSfen(), startSfen);
        QCOMPARE(black.sink.entries.size(), 0);
        const qint64 stopped = black.clock.remainingMs(Turn::Black);
        settle();
        // 承認を受けたら盤を動かし、相手の時計を進める。自分の時計は止めた値のままで、駒音はもう鳴らさない。
        // 相手の側は、手を受けて盤に指したときに鳴らす
        QCOMPARE(black.coordinator.match()->pendingState(), nullptr);
        QCOMPARE(blackSounds.count(), 1);
        QCOMPARE(whiteSounds.count(), 1);
        QVERIFY(black.clock.isRunning());
        QCOMPARE(black.clock.currentPlayer(), Turn::White);
        QCOMPARE(black.clock.remainingMs(Turn::Black), stopped);
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
        QCOMPARE(whiteSounds.count(), 2);
        settle();
        QCOMPARE(black.sink.entries.size(), 2);
        QCOMPARE(blackSounds.count(), 2);
        QCOMPARE(whiteSounds.count(), 2);

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

    /// 自分の手をエンジンに指させる対局: 交渉で相手に伝わり、エンジンの初期化が済むまで開始署名を送らず、
    /// エンジンの手は相手の承認を受けてから盤に指し、終局でエンジンに知らせて枠を空ける。証跡に種別が残る
    void engineGameFromStartToEvidence()
    {
        const QByteArray applicantId(38, 'c');
        const QByteArray hostId(38, 'j');
        Side applicant(applicantId, m_dir.filePath(QStringLiteral("a3")));
        Side host(hostId, m_dir.filePath(QStringLiteral("h3")));
        applicant.transport.other = &host;
        host.transport.other = &applicant;
        QSignalSpy engineStarting(&host.coordinator, &P2pGameCoordinator::engineStarting);
        QSignalSpy engineStarted(&host.coordinator, &P2pGameCoordinator::matchEngineStarted);
        QSignalSpy engineMoves(&host.coordinator, &P2pGameCoordinator::engineMovePlayed);
        QSignalSpy notices(&host.coordinator, &P2pGameCoordinator::engineNotice);
        QSignalSpy hostFinished(&host.coordinator, &P2pGameCoordinator::gameFinished);
        QSignalSpy hostSounds(&host.board, &GameBoard::piecePlaced);

        // 招待した側がエンジン（後手を希望。モックのエンジンの最初の手 8c8d は後手の手）
        P2pGameCoordinator::NegotiationRequest hostRequest = request(kRoleHost, applicantId, QStringLiteral("募集"));
        hostRequest.mySide = SidePreference::White;
        hostRequest.player = mockEngine();
        qputenv("SBQ_MATCH_INIT_DELAY_MS", "300");
        QVERIFY(host.coordinator.startNegotiation(hostRequest));
        QVERIFY(applicant.coordinator.startNegotiation(request(kRoleApplicant, hostId, QStringLiteral("申込"))));
        settle();
        qunsetenv("SBQ_MATCH_INIT_DELAY_MS");
        QCOMPARE(engineStarting.count(), 1);
        QCOMPARE(host.coordinator.negotiation()->stage(), Negotiation::Stage::Confirming);
        QCOMPARE(applicant.coordinator.negotiation()->stage(), Negotiation::Stage::Confirming);
        // 相手には、エンジンが指すことが条件確認の時点で伝わっている
        QCOMPARE(applicant.coordinator.opponentKind(), std::optional<PlayerKind>(PlayerKind::Engine));
        QCOMPARE(host.coordinator.opponentKind(), std::optional<PlayerKind>(PlayerKind::Human));
        QVERIFY(host.coordinator.playsWithEngine());
        QVERIFY(!applicant.coordinator.playsWithEngine());

        // エンジンの初期化（usi〜readyok）が済むまで、開始を押しても開始署名を送らない
        QVERIFY(host.coordinator.isEngineStarting());
        host.coordinator.confirmStart();
        QVERIFY(!host.coordinator.negotiation()->confirmed());
        QSignalSpy hostChanged(&host.coordinator, &P2pGameCoordinator::negotiationChanged);
        QTRY_VERIFY(!host.coordinator.isEngineStarting());
        QVERIFY(hostChanged.count() >= 1);
        QVERIFY(host.engines.engineInSlot(1));
        host.coordinator.confirmStart();
        applicant.coordinator.confirmStart();
        settle();
        QVERIFY(host.activity.is(AppActivityState::Activity::DuringP2pGame));
        QCOMPARE(engineStarted.count(), 1);
        QCOMPARE(engineStarted.first().at(0).value<Turn>(), Turn::White);
        QCOMPARE(engineStarted.first().at(1).toString(), QStringLiteral("P2P test engine"));
        QVERIFY(!host.coordinator.match()->iAmBlack());
        const RulesTerms expected{PlayerKind::Human, PlayerKind::Engine};
        QCOMPARE(host.coordinator.match()->rules(), expected);
        QCOMPARE(applicant.coordinator.match()->rules(), expected);
        // 棋譜の対局者は、エンジンで指していても表示名
        QCOMPARE(host.sink.started.players.humanName(Turn::White), QStringLiteral("募集"));

        // エンジンで指す側は、盤の手を受け付けない
        QVERIFY(!host.coordinator.isHumanToMove());
        QCOMPARE(host.coordinator.playMove(UsiMove::fromString(u"3c3d")), GameChannel::MoveResult::Rejected);

        // 先手の人間が指すと、後手のエンジンが応じる。エンジンの手も、相手の承認を受けてから盤に指す
        QCOMPARE(applicant.coordinator.playMove(UsiMove::fromString(u"7g7f")), GameChannel::MoveResult::Sent);
        QTRY_COMPARE(applicant.sink.entries.size(), 2);
        QTRY_COMPARE(host.sink.entries.size(), 2);
        QCOMPARE(host.sink.entries.at(1).move.move.toString(), QStringLiteral("8c8d"));
        QCOMPARE(host.board.board()->position().toSfen(), applicant.board.board()->position().toSfen());
        QCOMPARE(engineMoves.count(), 1);
        QCOMPARE(engineMoves.first().at(0).value<Turn>(), Turn::White);
        QCOMPARE(engineMoves.first().at(1).toInt(), 2);
        // 駒音は、相手の手もエンジンの手も、盤に指したときに1回ずつ
        QCOMPARE(hostSounds.count(), 2);
        // 自分の手番でも、エンジンで指す側は盤の手を受け付けない。投了は人ができる
        QVERIFY(!host.coordinator.isHumanToMove());

        // エンジンの持ち時間は、双方が署名した残り時間（秒読みは決まりのとおり）
        const MatchSearchTiming timing = host.coordinator.snapshot(Turn::White, false);
        const GameState& last = host.coordinator.match()->lastState();
        QCOMPARE(timing.input.wtimeMs, static_cast<qint64>(last.whiteMainMs));
        QCOMPARE(timing.input.btimeMs, static_cast<qint64>(last.blackMainMs));
        QCOMPARE(timing.input.timeControl.white.byoyomiMs, qint64(10'000));
        QCOMPARE(timing.input.sideToMove, Turn::White);

        QCOMPARE(applicant.coordinator.playMove(UsiMove::fromString(u"2g2f")), GameChannel::MoveResult::Sent);
        QTRY_COMPARE(host.sink.entries.size(), 4);
        QCOMPARE(host.sink.entries.at(3).move.move.toString(), QStringLiteral("3c3d"));
        QCOMPARE(engineMoves.count(), 2);

        // 先手の投了: エンジンに勝ちを知らせて、枠を空ける
        Usi* engine = host.engines.engineInSlot(1);
        QVERIFY(engine);
        QSignalSpy sent(engine->process(), &EngineProcessManager::commandSent);
        QVERIFY(applicant.coordinator.canResign());
        applicant.coordinator.resign();
        QTRY_COMPARE(hostFinished.count(), 1);
        QStringList commands;
        for (const QList<QVariant>& args : std::as_const(sent)) commands << args.at(0).toString();
        QVERIFY2(commands.contains(QStringLiteral("gameover win")), qPrintable(commands.join(QLatin1Char(','))));
        QVERIFY(!host.engines.engineInSlot(1));
        QCOMPARE(notices.count(), 0);
        for (Side* s : {&applicant, &host}) {
            QCOMPARE(s->coordinator.match()->settlement(), Settlement::MutuallySigned);
            QFile file(s->coordinator.lastEvidencePath());
            QVERIFY(file.open(QIODevice::ReadOnly));
            const EvidenceReport report = EvidenceVerifier::verify(file.readAll());
            QVERIFY2(report.isValid(), qPrintable(report.error));
            QCOMPARE(report.rules, expected);
        }
    }

    /// エンジンが指せない手を返したら、相手に送らずに投了する（反則は負け。相手は不正な手として対局を止めずに済む）
    void engineIllegalMoveResigns()
    {
        const QByteArray applicantId(38, 'd');
        const QByteArray hostId(38, 'k');
        Side applicant(applicantId, m_dir.filePath(QStringLiteral("a4")));
        Side host(hostId, m_dir.filePath(QStringLiteral("h4")));
        applicant.transport.other = &host;
        host.transport.other = &applicant;
        QSignalSpy notices(&applicant.coordinator, &P2pGameCoordinator::engineNotice);
        QSignalSpy hostFinished(&host.coordinator, &P2pGameCoordinator::gameFinished);

        // 申込側がエンジンで先手（招待した側が後手を希望）。モックのエンジンの最初の手 8c8d は先手には指せない
        P2pGameCoordinator::NegotiationRequest hostRequest = request(kRoleHost, applicantId, QStringLiteral("募集"));
        hostRequest.mySide = SidePreference::White;
        P2pGameCoordinator::NegotiationRequest applicantRequest = request(kRoleApplicant, hostId, QStringLiteral("申込"));
        applicantRequest.player = mockEngine();
        QVERIFY(host.coordinator.startNegotiation(hostRequest));
        QVERIFY(applicant.coordinator.startNegotiation(applicantRequest));
        QTRY_VERIFY(applicant.coordinator.negotiation()
                    && applicant.coordinator.negotiation()->stage() == Negotiation::Stage::Confirming
                    && !applicant.coordinator.isEngineStarting());
        settle();
        // 招待した側は、申込側の種別を START_AGREEMENT の Rules で知る
        QCOMPARE(host.coordinator.opponentKind(), std::optional<PlayerKind>(PlayerKind::Engine));
        host.coordinator.confirmStart();
        applicant.coordinator.confirmStart();
        QTRY_COMPARE(hostFinished.count(), 1);
        QCOMPARE(notices.count(), 1);
        for (Side* s : {&applicant, &host}) {
            QCOMPARE(s->coordinator.match()->settlement(), Settlement::MutuallySigned);
            QVERIFY(s->coordinator.match()->agreedResult());
            QCOMPARE(s->coordinator.match()->agreedResult()->outcome, Outcome::WhiteWins);
            QCOMPARE(s->coordinator.match()->agreedResult()->cause, Cause::Resign);
            QCOMPARE(s->sink.entries.size(), 1);
            QVERIFY(s->activity.is(AppActivityState::Activity::Idle));
        }
        QVERIFY(!applicant.engines.engineInSlot(1));
    }

    /// エンジンを起動できなければ、対局を始めずに交渉を取りやめる（相手には取りやめが伝わる）
    void engineStartFailureCancelsNegotiation()
    {
        const QByteArray applicantId(38, 'e');
        const QByteArray hostId(38, 'l');
        Side applicant(applicantId, m_dir.filePath(QStringLiteral("a5")));
        Side host(hostId, m_dir.filePath(QStringLiteral("h5")));
        applicant.transport.other = &host;
        host.transport.other = &applicant;
        QSignalSpy notices(&host.coordinator, &P2pGameCoordinator::engineNotice);
        QSignalSpy hostEnded(&host.coordinator, &P2pGameCoordinator::negotiationEnded);
        QSignalSpy applicantEnded(&applicant.coordinator, &P2pGameCoordinator::negotiationEnded);

        P2pGameCoordinator::NegotiationRequest hostRequest = request(kRoleHost, applicantId, QStringLiteral("募集"));
        hostRequest.player = mockEngine();
        hostRequest.player.enginePath = m_dir.filePath(QStringLiteral("no-such-engine"));
        QVERIFY(host.coordinator.startNegotiation(hostRequest));
        QVERIFY(applicant.coordinator.startNegotiation(request(kRoleApplicant, hostId, QStringLiteral("申込"))));
        QTRY_COMPARE(notices.count(), 1);
        QTRY_COMPARE(applicantEnded.count(), 1);
        QCOMPARE(hostEnded.count(), 1);
        QCOMPARE(hostEnded.first().at(0).toBool(), false);
        QCOMPARE(applicantEnded.first().at(0).toBool(), false);
        QVERIFY(!host.coordinator.isBusy());
        QVERIFY(!applicant.coordinator.isBusy());
        QVERIFY(host.activity.is(AppActivityState::Activity::Idle));

        // エンジンの場所の無い対局者・エンジンの枠の無い司令塔では、交渉を始めない
        hostRequest.player.enginePath.clear();
        QVERIFY(!host.coordinator.startNegotiation(hostRequest));
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

    /// 招待で接続した側は、相手の応答を1つも受けないうちに接続を閉じられたら、招待が受け付けられなかったと知らせる
    /// （招待した側は、招待のトークンの合わない接続を、何も返さずに閉じる。合言葉が違う・期限切れ・使用済み）。
    /// 相手の応答を受けた後に切れたとき・招待した側では知らせない
    void closedBeforeTheHostAnswersIsARefusedInvite()
    {
        const QByteArray applicantId(38, 'b');
        const QByteArray hostId(38, 'i');
        {
            // 相手（招待した側）が何も返さずに閉じた
            Side applicant(applicantId, m_dir.filePath(QStringLiteral("a-refused")));
            QSignalSpy ended(&applicant.coordinator, &P2pGameCoordinator::negotiationEnded);
            QSignalSpy refused(&applicant.coordinator, &P2pGameCoordinator::inviteRefused);
            QVERIFY(applicant.coordinator.startNegotiation(request(kRoleApplicant, hostId, QStringLiteral("申込"))));
            settle();
            QCOMPARE(applicant.coordinator.negotiation()->stage(), Negotiation::Stage::AwaitingHello);
            applicant.coordinator.onConnectionClosed(1);
            QCOMPARE(ended.count(), 1);
            QCOMPARE(ended.first().at(0).toBool(), false);
            QCOMPARE(refused.count(), 1);
            QVERIFY(!applicant.coordinator.isBusy());
            // ほかの接続が閉じても知らせない
            applicant.coordinator.onConnectionClosed(1);
            QCOMPARE(refused.count(), 1);
        }
        {
            // 条件確認まで進んだ後に切れたときは、招待は受け付けられている
            Side applicant(applicantId, m_dir.filePath(QStringLiteral("a-answered")));
            Side host(hostId, m_dir.filePath(QStringLiteral("h-answered")));
            applicant.transport.other = &host;
            host.transport.other = &applicant;
            QSignalSpy applicantRefused(&applicant.coordinator, &P2pGameCoordinator::inviteRefused);
            QSignalSpy hostRefused(&host.coordinator, &P2pGameCoordinator::inviteRefused);
            QVERIFY(host.coordinator.startNegotiation(request(kRoleHost, applicantId, QStringLiteral("募集"))));
            QVERIFY(applicant.coordinator.startNegotiation(request(kRoleApplicant, hostId, QStringLiteral("申込"))));
            settle();
            QCOMPARE(applicant.coordinator.negotiation()->stage(), Negotiation::Stage::Confirming);
            applicant.coordinator.onConnectionClosed(1);
            host.coordinator.onConnectionClosed(1);
            QCOMPARE(applicantRefused.count(), 0);
            QCOMPARE(hostRefused.count(), 0);
        }
        {
            // 招待した側: 相手が HELLO を送る前に切れても、招待を断られたのではない
            Side host(hostId, m_dir.filePath(QStringLiteral("h-waiting")));
            QSignalSpy refused(&host.coordinator, &P2pGameCoordinator::inviteRefused);
            QVERIFY(host.coordinator.startNegotiation(request(kRoleHost, applicantId, QStringLiteral("募集"))));
            settle();
            host.coordinator.onConnectionClosed(1);
            QCOMPARE(refused.count(), 0);
            QVERIFY(!host.coordinator.isBusy());
        }
    }

    void journalIsRecoveredAsUnconfirmedRecord()
    {
        // 対局の途中でアプリが終わった: ジャーナルを証跡に直す（対局は続けない）
        const QString root = m_dir.filePath(QStringLiteral("recover"));
        P2pRecordStore store(root);
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
        QVERIFY(journal->append(RecordKinds::start(a, RulesTerms{}, TimeControlTerms{}, kSideBlack, QStringLiteral("相手"))));
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
        // 同じ保存先を使えるアプリは1つ（2つ目のアプリは開けない）
        P2pPlayerProfile first(root);
        QVERIFY(first.open());
        P2pPlayerProfile second(root);
        QVERIFY(!second.open());
        first.close();
        QVERIFY(second.open());
    }
};

QTEST_MAIN(TestP2pCoordinator)
#include "tst_p2p_coordinator.moc"
