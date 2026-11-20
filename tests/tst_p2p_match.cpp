/// @file tst_p2p_match.cpp
/// @brief 開始までの交渉と対局の状態機械の試験（2人分を通信プロセスなしでつなぐ。実装仕様 13.1節）

#include <QtTest>

#include <deque>
#include <functional>

#include "matchending.h"
#include "p2p_test_util.h"
#include "p2pevidenceverifier.h"
#include "p2pgamerules.h"
#include "p2pmatch.h"
#include "p2pmatchtimer.h"
#include "p2pnegotiation.h"
#include "p2precordkinds.h"

using namespace P2p;

namespace {

/// 試験の時計（ミリ秒）
struct FakeClock {
    qint64 ms = 1'000'000;
    quint64 seconds() const { return 1'800'000'000ULL + static_cast<quint64>(ms / 1000); }
};

/// 1人分（プレイヤー鍵・交渉・対局・ジャーナル）
struct Side {
    SigningKey playerKey = SigningKey::generate();
    QByteArray peerId;
    std::unique_ptr<Negotiation> negotiation;
    std::unique_ptr<Match> match;
    std::vector<CborValue> journal;
    std::vector<MatchEvent> events;
    bool failPersist = false;
};

QByteArray fakePeerId(char c) { return QByteArray(38, c); }

/// 2人をつなぎ、フレームを互いに渡す
class Pair
{
public:
    FakeClock clock;
    Side applicant;
    Side host;
    bool connected = true;
    std::deque<Frame> toHost;
    std::deque<Frame> toApplicant;
    /// 相手へ渡す直前のフレームを書き換える（版 1 だけの実装・条件を偽る相手を試す）
    std::function<void(Frame&)> rewriteToHost;
    std::function<void(Frame&)> rewriteToApplicant;

    Pair(SidePreference applicantSide = SidePreference::None, SidePreference hostSide = SidePreference::None)
    {
        applicant.peerId = fakePeerId('a');
        host.peerId = fakePeerId('h');
        applicant.negotiation = std::make_unique<Negotiation>(config(kRoleApplicant, applicantSide), &applicant.playerKey);
        host.negotiation = std::make_unique<Negotiation>(config(kRoleHost, hostSide), &host.playerKey);
    }

    /// 対局者の種別を決めて、交渉を作り直す
    void setKinds(PlayerKind applicantKind, PlayerKind hostKind, SidePreference hostSide = SidePreference::None)
    {
        NegotiationConfig a = config(kRoleApplicant, SidePreference::None);
        a.myKind = applicantKind;
        NegotiationConfig h = config(kRoleHost, hostSide);
        h.myKind = hostKind;
        applicant.negotiation = std::make_unique<Negotiation>(a, &applicant.playerKey);
        host.negotiation = std::make_unique<Negotiation>(h, &host.playerKey);
    }

    NegotiationConfig config(quint8 role, SidePreference side)
    {
        NegotiationConfig c;
        c.role = role;
        c.networkId = networkId("sbq-local-1");
        c.myPeerId = role == kRoleApplicant ? applicant.peerId : host.peerId;
        c.opponentPeerId = role == kRoleApplicant ? host.peerId : applicant.peerId;
        c.myDisplayName = role == kRoleApplicant ? QStringLiteral("申込") : QStringLiteral("募集");
        c.mySide = side;
        c.hostTerms = TimeControlTerms{60'000, 10'000};
        c.nowSeconds = [this]() { return clock.seconds(); };
        return c;
    }

    /// 送るフレームをバイト列にして読み直してから相手へ渡す（外枠の往復も試す）
    static void collect(std::vector<Frame> frames, std::deque<Frame>& queue)
    {
        for (const Frame& f : frames) {
            const std::optional<Frame> decoded = Frame::decode(f.encode());
            QVERIFY(decoded);
            queue.push_back(*decoded);
        }
    }

    void pump()
    {
        for (int guard = 0; guard < 1000; ++guard) {
            for (Side* s : {&applicant, &host}) {
                std::deque<Frame>& out = s == &applicant ? toHost : toApplicant;
                if (s->match) {
                    collect(s->match->takeOutgoing(), out);
                    for (MatchEvent& e : s->match->takeEvents()) s->events.push_back(std::move(e));
                } else if (s->negotiation) {
                    collect(s->negotiation->takeOutgoing(), out);
                }
            }
            if (!connected) {
                toHost.clear();
                toApplicant.clear();
                return;
            }
            if (toHost.empty() && toApplicant.empty()) return;
            if (!toHost.empty()) {
                Frame f = toHost.front();
                toHost.pop_front();
                if (rewriteToHost) rewriteToHost(f);
                deliver(host, f);
            }
            if (!toApplicant.empty()) {
                Frame f = toApplicant.front();
                toApplicant.pop_front();
                if (rewriteToApplicant) rewriteToApplicant(f);
                deliver(applicant, f);
            }
        }
        QFAIL("frames did not settle");
    }

    static void deliver(Side& side, const Frame& frame)
    {
        if (side.match) side.match->receive(frame);
        else side.negotiation->receive(frame);
    }

    /// 両方の開始署名がそろったら対局へ移る
    /// run が false なら、対局を作るだけで begin しない（開始の手続きの途中を試す）
    void startMatches(bool run = true)
    {
        for (Side* s : {&applicant, &host}) {
            Negotiation& n = *s->negotiation;
            QCOMPARE(n.stage(), Negotiation::Stage::Started);
            MatchSetup setup;
            setup.agreement = *n.agreement();
            setup.rules = n.rules();
            setup.terms = n.terms();
            setup.iAmBlack = n.iAmBlack();
            setup.blackStartSignature = n.blackStartSignature();
            setup.whiteStartSignature = n.whiteStartSignature();
            setup.myConnectionRandom = n.myConnectionRandom();
            setup.opponentConnectionRandom = n.opponentConnectionRandom();
            setup.nextRequestId = n.nextRequestId();
            setup.opponentDisplayName = n.opponent()->displayName;
            Side* side = s;
            s->match = std::make_unique<Match>(
                setup, n.takeSessionKey(), &s->playerKey, [this]() { return clock.ms; },
                [side](const CborValue& record) {
                    if (side->failPersist) return false;
                    side->journal.push_back(record);
                    return true;
                });
        }
        if (!run) return;
        applicant.match->begin();
        host.match->begin();
        pump();
    }

    void negotiateAndStart(bool run = true)
    {
        applicant.negotiation->begin();
        pump();
        QCOMPARE(applicant.negotiation->stage(), Negotiation::Stage::Confirming);
        QCOMPARE(host.negotiation->stage(), Negotiation::Stage::Confirming);
        host.negotiation->confirm();
        applicant.negotiation->confirm();
        pump();
        startMatches(run);
    }

    Side& black() { return applicant.match->iAmBlack() ? applicant : host; }
    Side& white() { return applicant.match->iAmBlack() ? host : applicant; }

    bool hasEvent(const Side& s, MatchEvent::Type type) const
    {
        for (const MatchEvent& e : s.events) {
            if (e.type == type) return true;
        }
        return false;
    }

    void play(Side& side, const char* move, qint64 thinkMs = 1000)
    {
        clock.ms += thinkMs;
        QCOMPARE(side.match->playMove(UsiMove::fromString(QString::fromLatin1(move))), Match::MoveError::None);
        pump();
    }
};

/// 自分の対局1つと、相手の鍵。相手のフレームは試験で作る（改造したクライアントに見立てる）
class LoneMatch
{
public:
    explicit LoneMatch(bool iAmBlack)
    {
        const P2pTest::Player& black = iAmBlack ? me : opponent;
        const P2pTest::Player& white = iAmBlack ? opponent : me;
        setup.agreement = P2pTest::makeAgreement(black, white, terms);
        setup.terms = terms;
        setup.iAmBlack = iAmBlack;
        setup.blackStartSignature = black.playerKey.sign(setup.agreement.toCbor());
        setup.whiteStartSignature = white.playerKey.sign(setup.agreement.toCbor());
        setup.myConnectionRandom = Crypto::randomBytes(32);
        setup.opponentConnectionRandom = Crypto::randomBytes(32);
        gameId = setup.agreement.gameId();
        connection = connectionId(gameId, iAmBlack ? setup.myConnectionRandom : setup.opponentConnectionRandom,
                                  iAmBlack ? setup.opponentConnectionRandom : setup.myConnectionRandom);
        match = std::make_unique<Match>(setup, std::move(me.sessionKey), &me.playerKey, [this]() { return clockMs; },
                                        [this](const CborValue& record) {
                                            journal.push_back(record);
                                            return true;
                                        });
    }
    LoneMatch(const LoneMatch&) = delete;
    LoneMatch& operator=(const LoneMatch&) = delete;

    void deliver(FrameType type, const CborValue& payload)
    {
        Frame f;
        f.type = type;
        f.requestId = m_requestId++;
        f.gameId = gameId;
        f.payload = payload;
        match->receive(f);
    }
    QByteArray approval(const GameState& state) const
    {
        return opponent.sessionKey.sign(stateApproval(gameId, state.ply, state.hash()));
    }
    /// 相手が状態を提案する（State0 は先手の相手が提案する）
    void propose(const GameState& state) { deliver(FrameType::StatePropose, StateProposePayload{state, approval(state)}.toCbor()); }
    /// 相手が自分の提案を承認する
    void accept(const GameState& state)
    {
        deliver(FrameType::StateAccept, StateAcceptPayload{state.ply, state.hash(), approval(state)}.toCbor());
    }
    /// 相手が制御メッセージを送る（番号は1から順に）
    void control(ControlKind kind, const CborValue& body)
    {
        ControlPayload payload;
        payload.body = body;
        payload.control.gameId = gameId;
        payload.control.connectionId = connection;
        payload.control.sequence = ++m_controlSequence;
        payload.control.kind = kind;
        payload.control.bodyHash = Cbor::hash(body);
        payload.signature = opponent.sessionKey.sign(payload.control.toCbor());
        deliver(FrameType::Control, payload.toCbor());
    }
    /// 今の最新の状態を指す結果
    FinalResult resultFor(Outcome outcome, Cause cause) const
    {
        std::vector<GameState> states;
        for (const SignedState& s : match->confirmedStates()) states.push_back(s.state);
        FinalResult r;
        r.gameId = gameId;
        r.finalStateHash = states.back().hash();
        r.transcriptHash = transcriptHash(gameId, states);
        r.ply = states.back().ply;
        r.outcome = outcome;
        r.cause = cause;
        return r;
    }
    /// 自分が送ったフレームに、RESULT_DECLINE があり、RESULT_SIGNATURE がないか
    bool declinedWithoutSigning()
    {
        bool declined = false;
        for (const Frame& f : match->takeOutgoing()) {
            if (f.type == FrameType::ResultSignature) return false;
            const std::optional<ControlPayload> sent =
                f.type == FrameType::Control ? ControlPayload::fromCbor(f.payload) : std::nullopt;
            if (sent && sent->control.kind == ControlKind::ResultDecline) declined = true;
        }
        return declined;
    }

    TimeControlTerms terms{60'000, 10'000};
    P2pTest::Player me;
    P2pTest::Player opponent;
    MatchSetup setup;
    QByteArray gameId;
    QByteArray connection;
    qint64 clockMs = 10'000'000;   ///< アプリを起動してから長く経った値（持ち時間＋秒読みの何倍も）
    std::vector<CborValue> journal;
    std::unique_ptr<Match> match;

private:
    quint64 m_requestId = 1;
    quint64 m_controlSequence = 0;
};

/// HELLO の対応する版の一覧から版 2 を外す（版 1 だけの実装の HELLO にする）
void dropPlayerKindSupport(Frame& frame)
{
    if (frame.type != FrameType::Hello) return;
    std::optional<HelloPayload> hello = HelloPayload::fromCbor(frame.payload);
    QVERIFY(hello);
    hello->supportsPlayerKinds = false;
    frame.payload = hello->toCbor();
}

} // namespace

class TestP2pMatch : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QVERIFY(Crypto::initialize()); }

    /// 計時の記録: 自分の消費時間、相手の手番の観測時間、観測を始めてから切れていた時間（監査 2026-11-12 R6）
    void matchTimerKeepsTurnsAndDisconnections()
    {
        MatchTimer timer;
        // 手番が始まる前の時間は消費時間に数えない（監査 2026-11-13 P1）
        QVERIFY(!timer.myTurnStarted());
        QCOMPARE(timer.mySpentMs(900'000), qint64(0));
        timer.startMyTurn(1000);
        QVERIFY(timer.myTurnStarted());
        QCOMPARE(timer.mySpentMs(3500), qint64(2500));
        QCOMPARE(timer.mySpentMs(500), qint64(0));   // 負にしない

        // 観測の前の切断は、観測の許容に入れない
        timer.disconnected(4000);
        QCOMPARE(timer.currentDisconnectMs(4500), qint64(500));
        timer.disconnected(4200);                    // 切れている間に呼んでも始まりは変わらない
        timer.reconnected(5000);
        QCOMPARE(timer.currentDisconnectMs(5000), qint64(-1));
        QCOMPARE(timer.disconnectedTotalMs(9000), qint64(1000));

        timer.startObserving(6000);
        QCOMPARE(timer.disconnectedSinceObservingMs(6000), qint64(0));
        timer.disconnected(7000);
        // 切れている間も、観測を始めてからの切断の時間に入る
        QCOMPARE(timer.disconnectedSinceObservingMs(7300), qint64(300));
        timer.reconnected(8000);
        QCOMPARE(timer.observedMs(10000), qint64(4000));
        QCOMPARE(timer.disconnectedSinceObservingMs(10000), qint64(1000));
        QCOMPARE(timer.disconnectedTotalMs(10000), qint64(2000));
    }

    void negotiationAgreesOnSidesAndKeys()
    {
        Pair pair(SidePreference::White, SidePreference::None);
        pair.applicant.negotiation->begin();
        pair.pump();
        const Negotiation& a = *pair.applicant.negotiation;
        const Negotiation& h = *pair.host.negotiation;
        QCOMPARE(a.stage(), Negotiation::Stage::Confirming);
        QCOMPARE(h.stage(), Negotiation::Stage::Confirming);
        QVERIFY(!a.iAmBlack());   // 後手希望が通る
        QVERIFY(h.iAmBlack());
        QCOMPARE(a.gameId(), h.gameId());
        QVERIFY(!a.sidesByCoin());
        QCOMPARE(a.terms(), (TimeControlTerms{60'000, 10'000}));
        QCOMPARE(a.opponent()->displayName, QStringLiteral("募集"));
        // 相手の開始署名を確かめるまでは申告
        QVERIFY(!a.opponentVerified());
        QVERIFY(!h.opponentVerified());
    }

    /// 募集側は、先に開始を押していても、申込側の開始署名を確かめるまで署名を送らない
    void hostSignsOnlyAfterApplicant()
    {
        Pair pair;
        pair.applicant.negotiation->begin();
        pair.pump();
        pair.host.negotiation->confirm();
        pair.pump();
        QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::AwaitingSignature);
        QVERIFY(pair.toApplicant.empty());
        QVERIFY(pair.host.negotiation->blackStartSignature().isEmpty() || pair.host.negotiation->whiteStartSignature().isEmpty());
        pair.applicant.negotiation->confirm();
        pair.pump();
        QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::Started);
        QCOMPARE(pair.applicant.negotiation->stage(), Negotiation::Stage::Started);
        QVERIFY(pair.host.negotiation->opponentVerified());
    }

    void incompatibleSidesAreRejected()
    {
        Pair pair(SidePreference::Black, SidePreference::Black);
        pair.applicant.negotiation->begin();
        pair.pump();
        QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::Failed);
        QCOMPARE(pair.applicant.negotiation->stage(), Negotiation::Stage::Failed);
        QVERIFY(pair.applicant.negotiation->failedByPeer());
        QCOMPARE(*pair.applicant.negotiation->failure(), ErrorCode::ConditionMismatch);
    }

    void blockedApplicantGetsBusy()
    {
        Pair pair;
        NegotiationConfig c = pair.config(kRoleHost, SidePreference::None);
        c.isBlocked = [](const QByteArray&) { return true; };
        pair.host.negotiation = std::make_unique<Negotiation>(c, &pair.host.playerKey);
        pair.applicant.negotiation->begin();
        pair.pump();
        QCOMPARE(*pair.applicant.negotiation->failure(), ErrorCode::Busy);
    }

    /// 友だちとの交渉は、相手がその友だちのプレイヤーID を名乗らなければ進めない（受ける側・申し込む側とも）
    void friendNegotiationRequiresTheFriendsPlayerId()
    {
        const SigningKey stranger = SigningKey::generate();
        {
            // 受ける側: 申し込んできた相手が、知らせを預けた友だちの ID を名乗っている
            Pair pair;
            NegotiationConfig c = pair.config(kRoleHost, SidePreference::None);
            c.expectedPlayerKey = pair.applicant.playerKey.publicKey();
            pair.host.negotiation = std::make_unique<Negotiation>(c, &pair.host.playerKey);
            pair.applicant.negotiation->begin();
            pair.pump();
            QVERIFY(!pair.host.negotiation->failure());
            QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::Confirming);
        }
        {
            // 受ける側: 別の ID を名乗る相手は断る
            Pair pair;
            NegotiationConfig c = pair.config(kRoleHost, SidePreference::None);
            c.expectedPlayerKey = stranger.publicKey();
            pair.host.negotiation = std::make_unique<Negotiation>(c, &pair.host.playerKey);
            pair.applicant.negotiation->begin();
            pair.pump();
            QCOMPARE(*pair.host.negotiation->failure(), ErrorCode::ConditionMismatch);
            QVERIFY(pair.applicant.negotiation->failure());
        }
        {
            // 申し込む側: 応えた相手が、申し込んだ友だちの ID を名乗っていなければ打ち切る
            Pair pair;
            NegotiationConfig c = pair.config(kRoleApplicant, SidePreference::None);
            c.expectedPlayerKey = stranger.publicKey();
            pair.applicant.negotiation = std::make_unique<Negotiation>(c, &pair.applicant.playerKey);
            pair.applicant.negotiation->begin();
            pair.pump();
            QCOMPARE(*pair.applicant.negotiation->failure(), ErrorCode::ConditionMismatch);
        }
    }

    void expiredAgreementIsNotSigned()
    {
        Pair pair;
        pair.applicant.negotiation->begin();
        pair.pump();
        pair.clock.ms += 181'000;   // 3分を過ぎた
        pair.applicant.negotiation->confirm();
        pair.pump();
        QCOMPARE(pair.applicant.negotiation->stage(), Negotiation::Stage::Failed);
        QCOMPARE(*pair.applicant.negotiation->failure(), ErrorCode::Expired);
    }

    /// 対局者の種別（人間・エンジン）は交渉で相手に伝わり、双方が署名する開始合意の Rules に、決まった先後で入る
    void playerKindsAreAgreedBySide_data()
    {
        QTest::addColumn<bool>("applicantEngine");
        QTest::addColumn<bool>("hostEngine");
        QTest::addColumn<int>("hostSide");
        QTest::newRow("engine-host-black") << false << true << int(SidePreference::Black);
        QTest::newRow("engine-host-white") << false << true << int(SidePreference::White);
        QTest::newRow("engine-applicant-black") << true << false << int(SidePreference::White);
        QTest::newRow("engine-applicant-white") << true << false << int(SidePreference::Black);
        QTest::newRow("both-engines") << true << true << int(SidePreference::None);
        QTest::newRow("both-humans") << false << false << int(SidePreference::None);
    }

    void playerKindsAreAgreedBySide()
    {
        QFETCH(bool, applicantEngine);
        QFETCH(bool, hostEngine);
        QFETCH(int, hostSide);
        const PlayerKind applicantKind = applicantEngine ? PlayerKind::Engine : PlayerKind::Human;
        const PlayerKind hostKind = hostEngine ? PlayerKind::Engine : PlayerKind::Human;
        Pair pair;
        pair.setKinds(applicantKind, hostKind, static_cast<SidePreference>(hostSide));
        pair.applicant.negotiation->begin();
        pair.pump();
        const Negotiation& a = *pair.applicant.negotiation;
        const Negotiation& h = *pair.host.negotiation;
        QCOMPARE(a.stage(), Negotiation::Stage::Confirming);
        QCOMPARE(h.stage(), Negotiation::Stage::Confirming);
        // 条件確認画面を出す時点で、双方が相手の種別を知っている
        QCOMPARE(a.opponentKind(), std::optional<PlayerKind>(hostKind));
        QCOMPARE(h.opponentKind(), std::optional<PlayerKind>(applicantKind));
        const RulesTerms expected = RulesTerms::forSides(a.iAmBlack(), applicantKind, hostKind);
        QCOMPARE(a.rules(), expected);
        QCOMPARE(h.rules(), expected);
        QCOMPARE(a.agreement()->rulesHash, expected.hash());
        QCOMPARE(a.gameId(), h.gameId());
        // 双方人間の対局の開始合意は、版 1 と同じ rulesHash
        QCOMPARE(expected.hash() == Rules::v1Hash(), !applicantEngine && !hostEngine);

        // 対局して、証跡に同じ種別が残る
        pair.host.negotiation->confirm();
        pair.applicant.negotiation->confirm();
        pair.pump();
        pair.startMatches();
        pair.play(pair.black(), "7g7f");
        QVERIFY(pair.white().match->resign());
        pair.pump();
        for (Side* s : {&pair.applicant, &pair.host}) {
            QCOMPARE(s->match->settlement(), Settlement::MutuallySigned);
            const EvidenceReport report = EvidenceVerifier::verify(s->match->evidence().encode());
            QVERIFY2(report.isValid(), qPrintable(report.error));
            QCOMPARE(report.rules, expected);
            // ジャーナルを読み戻しても同じ証跡（再起動の後の復旧）
            const std::optional<JournalReplay> replay = RecordKinds::replay(s->journal);
            QVERIFY(replay);
            QCOMPARE(replay->evidence.encode(), s->match->evidence().encode());
        }
    }

    /// 版 1 だけの実装（対局者の種別を扱えない）とは、人間どうしなら今までどおり対局でき、エンジンが指す対局は始めない
    void versionOneOpponentPlaysOnlyHumanGames()
    {
        {
            // 申込側が版 1 だけ: 双方人間なら、双方人間の Rules で合意する
            Pair pair;
            pair.rewriteToHost = &dropPlayerKindSupport;
            pair.applicant.negotiation->begin();
            pair.pump();
            QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::Confirming);
            QCOMPARE(pair.applicant.negotiation->stage(), Negotiation::Stage::Confirming);
            QVERIFY(!pair.host.negotiation->opponent()->supportsPlayerKinds);
            QCOMPARE(pair.host.negotiation->agreement()->rulesHash, Rules::v1Hash());
        }
        {
            // 募集側が版 1 だけ
            Pair pair;
            pair.rewriteToApplicant = &dropPlayerKindSupport;
            pair.applicant.negotiation->begin();
            pair.pump();
            QCOMPARE(pair.applicant.negotiation->stage(), Negotiation::Stage::Confirming);
            QVERIFY(!pair.applicant.negotiation->opponent()->supportsPlayerKinds);
            QCOMPARE(pair.applicant.negotiation->agreement()->rulesHash, Rules::v1Hash());
        }
        {
            // エンジンで指す募集側は、版 1 だけの申込側を、自分の HELLO を返さずに断る（UNSUPPORTED_VERSION）
            Pair pair;
            pair.setKinds(PlayerKind::Human, PlayerKind::Engine);
            pair.rewriteToHost = &dropPlayerKindSupport;
            pair.applicant.negotiation->begin();
            pair.pump();
            QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::Failed);
            QCOMPARE(*pair.host.negotiation->failure(), ErrorCode::UnsupportedVersion);
            QVERIFY(!pair.host.negotiation->failedByPeer());
            QVERIFY(!pair.applicant.negotiation->opponent());
            QCOMPARE(*pair.applicant.negotiation->failure(), ErrorCode::UnsupportedVersion);
            QVERIFY(pair.applicant.negotiation->failedByPeer());
        }
        {
            // エンジンで指す申込側は、募集側が版 1 だけだと分かったら断る（開始合意を作らない）
            Pair pair;
            pair.setKinds(PlayerKind::Engine, PlayerKind::Human);
            pair.rewriteToApplicant = &dropPlayerKindSupport;
            pair.applicant.negotiation->begin();
            pair.pump();
            QCOMPARE(pair.applicant.negotiation->stage(), Negotiation::Stage::Failed);
            QCOMPARE(*pair.applicant.negotiation->failure(), ErrorCode::UnsupportedVersion);
            QVERIFY(!pair.applicant.negotiation->failedByPeer());
            QVERIFY(!pair.applicant.negotiation->agreement());
            QCOMPARE(*pair.host.negotiation->failure(), ErrorCode::UnsupportedVersion);
        }
        {
            // 版 2 に対応すると言っていない申込側が、エンジンと書いた Rules を送ってきたら断る
            Pair pair;
            pair.setKinds(PlayerKind::Engine, PlayerKind::Human);
            pair.rewriteToHost = &dropPlayerKindSupport;
            pair.applicant.negotiation->begin();
            pair.pump();
            QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::Failed);
            QCOMPARE(*pair.host.negotiation->failure(), ErrorCode::ConditionMismatch);
        }
    }

    /// 募集側は、開始合意の Rules の自分の側が、自分の種別と違えば断る（相手に種別を書き換えさせない）
    void hostRejectsRulesThatMisstateItsKind()
    {
        Pair pair;
        pair.setKinds(PlayerKind::Human, PlayerKind::Human);
        pair.rewriteToHost = [](Frame& frame) {
            if (frame.type != FrameType::StartAgreement) return;
            std::optional<StartAgreementPayload> payload = StartAgreementPayload::fromCbor(frame.payload);
            QVERIFY(payload);
            // 双方をエンジンと書いた Rules と、それに合わせた rulesHash
            payload->rules = RulesTerms{PlayerKind::Engine, PlayerKind::Engine};
            payload->agreement.rulesHash = payload->rules.hash();
            frame.payload = payload->toCbor();
        };
        pair.applicant.negotiation->begin();
        pair.pump();
        QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::Failed);
        QCOMPARE(*pair.host.negotiation->failure(), ErrorCode::ConditionMismatch);
    }

    void forgedStartSignatureIsRejected()
    {
        Pair pair;
        pair.applicant.negotiation->begin();
        pair.pump();
        pair.host.negotiation->confirm();
        // 他人の鍵で作った開始署名
        const SigningKey stranger = SigningKey::generate();
        Frame forged;
        forged.type = FrameType::StartSignature;
        forged.requestId = 9;
        forged.gameId = pair.applicant.negotiation->gameId();
        forged.payload = startSignaturePayload(
            SideSignature{pair.applicant.negotiation->iAmBlack() ? kSideBlack : kSideWhite,
                          stranger.sign(pair.applicant.negotiation->agreement()->toCbor())});
        pair.host.negotiation->receive(forged);
        QCOMPARE(pair.host.negotiation->stage(), Negotiation::Stage::Failed);
        QCOMPARE(*pair.host.negotiation->failure(), ErrorCode::InvalidSignature);
        // 自分の開始署名は送っていない
        QVERIFY(pair.host.negotiation->blackStartSignature().isEmpty());
        QVERIFY(pair.host.negotiation->whiteStartSignature().isEmpty());
    }

    void fullGameEndsByResignWithBothSignatures()
    {
        Pair pair;
        pair.negotiateAndStart();
        QVERIFY(pair.hasEvent(pair.black(), MatchEvent::Type::Ready));
        QVERIFY(pair.hasEvent(pair.white(), MatchEvent::Type::Ready));
        pair.play(pair.black(), "7g7f");
        pair.play(pair.white(), "3c3d");
        QCOMPARE(pair.black().match->lastState().ply, quint32(2));
        QCOMPARE(pair.white().match->lastState().ply, quint32(2));
        // 相手の手番の投了はできない
        QVERIFY(!pair.white().match->resign());
        QVERIFY(pair.black().match->resign());
        pair.pump();
        for (Side* s : {&pair.applicant, &pair.host}) {
            QCOMPARE(s->match->phase(), Match::Phase::Finished);
            QCOMPARE(s->match->settlement(), Settlement::MutuallySigned);
            const EvidenceReport report = EvidenceVerifier::verify(s->match->evidence().encode());
            QVERIFY2(report.isValid(), qPrintable(report.error));
            QCOMPARE(report.category, EvidenceReport::Category::MutuallySigned);
            QCOMPARE(report.result->outcome, Outcome::WhiteWins);
            QCOMPARE(report.result->cause, Cause::Resign);
        }
        QCOMPARE(pair.black().match->evidence().encode(), pair.white().match->evidence().encode());
    }

    /// 対局が書いたジャーナルを読み戻すと、対局の証跡と同じになる（監査 2026-11-12 R2。再起動の後の復旧と同じ読み戻し）
    void journalReplaysToTheSameEvidence()
    {
        Pair pair;
        pair.negotiateAndStart();
        pair.play(pair.black(), "7g7f");
        pair.play(pair.white(), "3c3d");
        QVERIFY(pair.black().match->resign());
        pair.pump();
        for (Side* s : {&pair.applicant, &pair.host}) {
            QCOMPARE(s->match->settlement(), Settlement::MutuallySigned);
            const std::optional<JournalReplay> replay = RecordKinds::replay(s->journal);
            QVERIFY(replay);
            QCOMPARE(replay->evidence.encode(), s->match->evidence().encode());
            QCOMPARE(replay->mySide, s->match->iAmBlack() ? kSideBlack : kSideWhite);
            QCOMPARE(replay->opponentDisplayName, s == &pair.applicant ? QStringLiteral("募集") : QStringLiteral("申込"));
            QCOMPARE(replay->settlement, std::optional<Settlement>(s->match->settlement()));
            QVERIFY(!replay->hasConflictProof);
            QVERIFY(!s->match->hasConflictProof());
        }
        // 開始の件がなければ読み戻さない
        QVERIFY(!RecordKinds::replay({}));
        std::vector<CborValue> withoutStart(pair.black().journal.begin() + 1, pair.black().journal.end());
        QVERIFY(!RecordKinds::replay(withoutStart));
    }

    /// 結果の確定のしかたと、ジャーナル・履歴に書く語は1対1（監査 2026-11-12 R3）
    void settlementNamesRoundTrip()
    {
        QSet<QString> names;
        for (const Settlement s : {Settlement::None, Settlement::MutuallySigned, Settlement::SignatureMissing,
                                   Settlement::Aborted, Settlement::Error, Settlement::Disconnected,
                                   Settlement::Interrupted}) {
            names.insert(settlementName(s));
            QCOMPARE(settlementFromName(settlementName(s)), std::optional<Settlement>(s));
        }
        QCOMPARE(names.size(), 7);
        QVERIFY(!settlementFromName(u"resign"));
        QVERIFY(!settlementFromName(u""));
    }

    /// 相手の矛盾した署名の証明の件があれば、読み戻しで分かる（ジャーナルを残す）
    void journalReplayFindsTheConflictProof()
    {
        Pair pair;
        pair.negotiateAndStart();
        pair.play(pair.black(), "7g7f");
        std::vector<CborValue> records = pair.white().journal;
        const SignedState& last = pair.white().match->confirmedStates().back();
        records.push_back(RecordKinds::conflict(last.state, last.blackApproval, last.state, last.blackApproval));
        const std::optional<JournalReplay> replay = RecordKinds::replay(records);
        QVERIFY(replay);
        QVERIFY(replay->hasConflictProof);
        QVERIFY(!replay->settlement);
        QCOMPARE(replay->evidence.states.size(), std::size_t(2));
    }

    void repetitionIsAgreedAutomatically()
    {
        Pair pair;
        pair.negotiateAndStart();
        const QStringList moves = P2pTest::repetitionMoves();
        for (int i = 0; i < moves.size(); ++i) {
            pair.play(i % 2 == 0 ? pair.black() : pair.white(), moves.at(i).toLatin1().constData());
        }
        for (Side* s : {&pair.applicant, &pair.host}) {
            QCOMPARE(s->match->settlement(), Settlement::MutuallySigned);
            QCOMPARE(s->match->agreedResult()->cause, Cause::Repetition);
        }
    }

    void drawOfferNeedsTheOtherUser()
    {
        Pair pair;
        pair.negotiateAndStart();
        pair.play(pair.black(), "7g7f");
        QVERIFY(!pair.black().match->offerAwaitingAnswer());
        QCOMPARE(pair.black().match->offer(Cause::AgreedDraw), Match::OfferResult::Sent);
        // 申し出た側は、相手の答えを待っている申し出が分かる（尋ねられた側のものではない）
        QCOMPARE(pair.black().match->offerAwaitingAnswer(), std::optional<Cause>(Cause::AgreedDraw));
        // 答えを待っている間は送り直さない
        QCOMPARE(pair.black().match->offer(Cause::AgreedDraw), Match::OfferResult::AwaitingAnswer);
        QCOMPARE(pair.black().match->offerAwaitingAnswer(), std::optional<Cause>(Cause::AgreedDraw));
        pair.pump();
        QVERIFY(pair.white().match->hasPendingOffer());
        QVERIFY(!pair.white().match->offerAwaitingAnswer());
        pair.white().match->answerOffer(false);
        pair.pump();
        QVERIFY(pair.hasEvent(pair.black(), MatchEvent::Type::ProposalDeclined));
        QCOMPARE(pair.black().match->phase(), Match::Phase::Playing);
        QVERIFY(!pair.black().match->offerAwaitingAnswer());   // 断られたら、もう待っていない
        // 答えを待つ間に手が進んだら、申し出は無効になる。指した手の確認を待つ間は、別の申し出を送れない
        QCOMPARE(pair.white().match->offer(Cause::AgreedDraw), Match::OfferResult::Sent);
        QVERIFY(pair.white().match->offerAwaitingAnswer());
        pair.clock.ms += 1000;
        QCOMPARE(pair.white().match->playMove(UsiMove::fromString(u"3c3d")), Match::MoveError::None);
        QCOMPARE(pair.white().match->offer(Cause::AgreedAbort), Match::OfferResult::Unavailable);
        pair.pump();
        QCOMPARE(pair.white().match->lastState().ply, quint32(2));
        QVERIFY(!pair.white().match->offerAwaitingAnswer());
        // 後手が中断を申し出て先手が受ける
        pair.play(pair.black(), "2g2f");
        QCOMPARE(pair.white().match->offer(Cause::AgreedAbort), Match::OfferResult::Sent);
        QCOMPARE(pair.white().match->offerAwaitingAnswer(), std::optional<Cause>(Cause::AgreedAbort));
        pair.pump();
        pair.black().match->answerOffer(true);
        pair.pump();
        // 結果が決まった後は、待っている申し出はない
        QVERIFY(!pair.white().match->offerAwaitingAnswer());
        QCOMPARE(pair.black().match->agreedResult()->outcome, Outcome::Aborted);
        QCOMPARE(pair.white().match->settlement(), Settlement::MutuallySigned);
    }

    /// 断られた申し出は、同じ手数でももう一度申し出られる（相手にもう一度尋ねる）。答えを待っている間は送り直さない
    void declinedOfferCanBeMadeAgain()
    {
        Pair pair;
        pair.negotiateAndStart();
        pair.play(pair.black(), "7g7f");
        Side& offerer = pair.white();   // 手番の側が申し出る
        Side& asked = pair.black();
        const auto offersReceived = [&asked]() {
            return std::count_if(asked.events.begin(), asked.events.end(),
                                 [](const MatchEvent& e) { return e.type == MatchEvent::Type::OfferReceived; });
        };
        const auto declines = [&offerer]() {
            return std::count_if(offerer.events.begin(), offerer.events.end(),
                                 [](const MatchEvent& e) { return e.type == MatchEvent::Type::ProposalDeclined; });
        };

        QCOMPARE(offerer.match->offer(Cause::AgreedAbort), Match::OfferResult::Sent);
        pair.pump();
        QCOMPARE(offersReceived(), 1);
        // 答えを待っている間にもう一度押しても、相手に2回は尋ねない
        QCOMPARE(offerer.match->offer(Cause::AgreedAbort), Match::OfferResult::AwaitingAnswer);
        pair.pump();
        QCOMPARE(offersReceived(), 1);
        asked.match->answerOffer(false);
        pair.pump();
        QCOMPARE(declines(), 1);
        QCOMPARE(offerer.match->phase(), Match::Phase::Playing);

        // 断られた後、同じ手数でもう一度申し出る: 相手にもう一度尋ねる
        QCOMPARE(offerer.match->offer(Cause::AgreedAbort), Match::OfferResult::Sent);
        pair.pump();
        QCOMPARE(offersReceived(), 2);
        QVERIFY(asked.match->hasPendingOffer());
        asked.match->answerOffer(false);
        pair.pump();
        QCOMPARE(declines(), 2);
        // 引き分けの申し出も同じ。答えを待つ間に中断へ替えることはできるが、相手が答えるまでは、引き分けと中断を
        // 交互に送り直せない（相手に尋ねるのは、相手が答えるまでの間にそれぞれ1回まで）
        QCOMPARE(offerer.match->offer(Cause::AgreedDraw), Match::OfferResult::Sent);
        pair.pump();
        QCOMPARE(offersReceived(), 3);
        QCOMPARE(offerer.match->offer(Cause::AgreedAbort), Match::OfferResult::Sent);
        pair.pump();
        QCOMPARE(offersReceived(), 4);
        QCOMPARE(offerer.match->offer(Cause::AgreedDraw), Match::OfferResult::AwaitingAnswer);
        QCOMPARE(offerer.match->offer(Cause::AgreedAbort), Match::OfferResult::AwaitingAnswer);
        // 待っているのは、相手が尋ねられている最後の申し出（中断）
        QCOMPARE(offerer.match->offerAwaitingAnswer(), std::optional<Cause>(Cause::AgreedAbort));
        pair.pump();
        QCOMPARE(offersReceived(), 4);
        asked.match->answerOffer(false);
        pair.pump();
        QCOMPARE(declines(), 3);
        QCOMPARE(offerer.match->offer(Cause::AgreedDraw), Match::OfferResult::Sent);
        pair.pump();
        QCOMPARE(offersReceived(), 5);
        asked.match->answerOffer(false);
        pair.pump();
        QCOMPARE(declines(), 4);

        // 3回目の中断の申し出を相手が承諾すれば、合意の中断で終わる
        QCOMPARE(offerer.match->offer(Cause::AgreedAbort), Match::OfferResult::Sent);
        pair.pump();
        asked.match->answerOffer(true);
        pair.pump();
        for (Side* s : {&offerer, &asked}) {
            QCOMPARE(s->match->settlement(), Settlement::MutuallySigned);
            QCOMPARE(s->match->agreedResult()->cause, Cause::AgreedAbort);
            QCOMPARE(s->match->agreedResult()->ply, quint32(1));
        }
    }

    /// 届くのが許容差より遅れた手（観測 − 申告 > 10秒）は承認せずに中断する
    void clockMismatchStopsTheGame()
    {
        Pair pair;
        pair.negotiateAndStart();
        Side& black = pair.black();
        Side& white = pair.white();
        pair.clock.ms += 1000;
        QCOMPARE(black.match->playMove(UsiMove::fromString(u"7g7f")), Match::MoveError::None);
        const std::vector<Frame> frames = black.match->takeOutgoing();
        pair.clock.ms += 15'000;
        for (const Frame& f : frames) white.match->receive(f);
        QCOMPARE(white.match->phase(), Match::Phase::Finished);
        QCOMPARE(white.match->settlement(), Settlement::Error);
        QCOMPARE(*white.match->endingError(), ErrorCode::ClockMismatch);
        QCOMPARE(white.match->lastState().ply, quint32(0));
    }

    void observedTimeLimitsSelfReport()
    {
        Pair pair;
        pair.negotiateAndStart();
        Side& black = pair.black();
        Side& white = pair.white();
        // 先手は 15秒考えて指す。後手の観測は 15秒（許容差 10秒）なので受理する
        pair.play(black, "7g7f", 15'000);
        QCOMPARE(white.match->lastState().ply, quint32(1));
        QCOMPARE(white.match->lastState().spentMs, quint64(15'000));
    }

    void ownTimeoutIsDeclared()
    {
        Pair pair;
        pair.negotiateAndStart();
        // 先手が 60秒 + 秒読み10秒を使い切る
        pair.clock.ms += 70'000;
        pair.black().match->tick();
        pair.pump();
        for (Side* s : {&pair.applicant, &pair.host}) {
            QCOMPARE(s->match->settlement(), Settlement::MutuallySigned);
            QCOMPARE(s->match->agreedResult()->cause, Cause::Timeout);
            QCOMPARE(s->match->agreedResult()->outcome, Outcome::WhiteWins);
        }
        QCOMPARE(pair.black().match->playMove(UsiMove::fromString(u"7g7f")), Match::MoveError::Finished);
    }

    void reconnectResendsTheUnconfirmedMove()
    {
        Pair pair;
        pair.negotiateAndStart();
        Side& black = pair.black();
        Side& white = pair.white();
        // 送った直後に回線が切れる（相手に届かない）
        pair.connected = false;
        pair.clock.ms += 1000;
        QCOMPARE(black.match->playMove(UsiMove::fromString(u"7g7f")), Match::MoveError::None);
        pair.pump();
        QCOMPARE(white.match->lastState().ply, quint32(0));
        black.match->connectionLost();
        white.match->connectionLost();
        pair.pump();
        QCOMPARE(black.match->phase(), Match::Phase::Reconnecting);
        // 再接続
        pair.clock.ms += 5000;
        pair.connected = true;
        black.match->connectionEstablished();
        white.match->connectionEstablished();
        pair.pump();
        QCOMPARE(black.match->phase(), Match::Phase::Playing);
        QCOMPARE(white.match->phase(), Match::Phase::Playing);
        QCOMPARE(white.match->lastState().ply, quint32(1));
        QCOMPARE(black.match->lastState().ply, quint32(1));
        QVERIFY(pair.hasEvent(black, MatchEvent::Type::Resumed));
    }

    void reconnectSyncsAMissingAcceptance()
    {
        Pair pair;
        pair.negotiateAndStart();
        Side& black = pair.black();
        Side& white = pair.white();
        // 先手の手は届き後手が承認したが、承認が先手に届かない
        pair.clock.ms += 1000;
        QCOMPARE(black.match->playMove(UsiMove::fromString(u"7g7f")), Match::MoveError::None);
        // 後手へだけ渡し、後手の承認は失われる
        for (const Frame& f : black.match->takeOutgoing()) white.match->receive(f);
        white.match->takeOutgoing();
        QCOMPARE(white.match->lastState().ply, quint32(1));
        QCOMPARE(black.match->lastState().ply, quint32(0));
        black.match->connectionLost();
        white.match->connectionLost();
        pair.clock.ms += 2000;
        black.match->connectionEstablished();
        white.match->connectionEstablished();
        pair.pump();
        QCOMPARE(black.match->lastState().ply, quint32(1));
        QVERIFY(!black.match->hasPendingMove());
        QCOMPARE(black.match->phase(), Match::Phase::Playing);
        // 照合の後の SYNC_REQUEST には応えない（全状態を何度でも送り返さない。監査 2026-11-13 P17）
        Frame again;
        again.type = FrameType::SyncRequest;
        again.requestId = 999;
        again.gameId = white.match->gameId();
        again.payload = SyncRequestPayload{1, white.match->confirmedStates().front().state.hash()}.toCbor();
        white.match->receive(again);
        for (const Frame& f : white.match->takeOutgoing()) QVERIFY(f.type != FrameType::SyncChunk);
        QCOMPARE(white.match->phase(), Match::Phase::Playing);
    }

    void reconnectTimeoutLeavesUnconfirmedRecord()
    {
        Pair pair;
        pair.negotiateAndStart();
        pair.play(pair.black(), "7g7f");
        pair.black().match->connectionLost();
        pair.clock.ms += 121'000;
        pair.black().match->tick();
        QCOMPARE(pair.black().match->settlement(), Settlement::Disconnected);
        const EvidenceReport report = EvidenceVerifier::verify(pair.black().match->evidence().encode());
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::Unconfirmed);
    }

    /// 開始局面（State0）の承認がそろう前に対局が終わっても、最新の状態を読んで落ちない。
    /// 相手が何も送らないまま開始の期限が過ぎたとき・最初に壊れた提案が届いたときに、司令塔は終局の行を書くために
    /// 最新の状態を読む（以前は、空の列の末尾を読んでいた）
    void endingBeforeState0IsHandled()
    {
        {
            // 相手が State0 を送らないまま、開始の期限（30秒）が過ぎる
            Pair pair;
            pair.negotiateAndStart(false);
            Side& white = pair.white();
            white.match->begin();
            pair.clock.ms += 31'000;
            white.match->tick();
            QCOMPARE(white.match->phase(), Match::Phase::Finished);
            QCOMPARE(white.match->settlement(), Settlement::Error);
            QVERIFY(white.match->confirmedStates().empty());
            QCOMPARE(white.match->lastState().ply, 0u);
            QVERIFY(!white.match->isMyTurn());
            // 結果も状態もない記録として読める
            const EvidenceReport report = EvidenceVerifier::verify(white.match->evidence().encode());
            QCOMPARE(report.category, EvidenceReport::Category::Unconfirmed);
        }
        {
            // 最初の対局フレームが、形の合わない STATE_PROPOSE
            Pair pair;
            pair.negotiateAndStart(false);
            Side& white = pair.white();
            white.match->begin();
            Frame bad;
            bad.type = FrameType::StatePropose;
            bad.requestId = 99;
            bad.gameId = white.match->gameId();
            bad.payload = CborValue::array({});
            white.match->receive(bad);
            QCOMPARE(white.match->phase(), Match::Phase::Finished);
            QCOMPARE(white.match->settlement(), Settlement::Error);
            QCOMPARE(white.match->endingError(), std::optional<ErrorCode>(ErrorCode::InvalidMove));
        }
    }

    /// 開始の手続きの途中（先手が READY を待つ間）に後手が「先手の時間切れ」を提案しても、先手は同意しない。
    /// 以前は、READY の前の消費時間を時計の起点（0）から数え、アプリを起動して持ち時間より長く経っていると、
    /// 0手目の時間切れ負けに署名していた（監査 2026-11-13 P1）
    void resultProposalBeforeReadyIsDeclined()
    {
        LoneMatch lone(true);
        Match& match = *lone.match;
        match.begin();
        QVERIFY(!match.takeOutgoing().empty());   // State0 の提案

        // 後手（改造したクライアント）が State0 を承認する。READY は送らない
        const GameState state0 = GameRules::initialState(lone.gameId, lone.terms);
        lone.accept(state0);
        QCOMPARE(match.phase(), Match::Phase::Opening);
        QCOMPARE(match.confirmedStates().size(), size_t(1));

        // READY の代わりに、0手目の「先手の時間切れ」を提案する
        lone.control(ControlKind::ResultPropose, lone.resultFor(Outcome::WhiteWins, Cause::Timeout).toCbor());

        // 同意も署名もせず、断る
        QVERIFY(!match.agreedResult());
        for (const MatchEvent& e : match.takeEvents()) QVERIFY(e.type != MatchEvent::Type::ResultAgreed);
        QVERIFY(lone.declinedWithoutSigning());
        QCOMPARE(match.phase(), Match::Phase::Opening);
    }

    /// 千日手（棋譜から決まる終局）に達した局面では、相手の投了の申し出に同意しない。以前は棋譜を見ずに同意して署名し、
    /// 自分の証跡が「署名した結果が棋譜と食い違う」として検証に通らなくなっていた（監査 2026-11-13 P7）
    void resultConflictingWithRecordIsDeclined()
    {
        LoneMatch lone(false);   // 自分は後手、相手（改造したクライアント）は先手
        Match& match = *lone.match;
        lone.clockMs = 1'000'000;
        match.begin();
        GameState state = GameRules::initialState(lone.gameId, lone.terms);
        lone.propose(state);
        QCOMPARE(match.phase(), Match::Phase::Playing);
        const QStringList moves = P2pTest::repetitionMoves();
        for (int i = 0; i < moves.size(); ++i) {
            lone.clockMs += 1000;
            const UsiMove move = UsiMove::fromString(moves.at(i));
            const bool myMove = i % 2 == 1;
            if (myMove) QCOMPARE(match.playMove(move), Match::MoveError::None);
            const std::optional<GameState> next = GameRules::nextState(state, move, 1000, lone.terms);
            QVERIFY(next);
            state = *next;
            if (myMove) lone.accept(state);
            else lone.propose(state);
            QCOMPARE(match.lastState().ply, quint32(i + 1));
        }
        (void)match.takeOutgoing();
        (void)match.takeEvents();
        // 千日手なので、自分の合意の申し出もできない
        QCOMPARE(match.offer(Cause::AgreedAbort), Match::OfferResult::Unavailable);

        // 相手（先手）が自分の投了を申し出る
        lone.control(ControlKind::ResultPropose, lone.resultFor(Outcome::WhiteWins, Cause::Resign).toCbor());
        QVERIFY(!match.agreedResult());
        QVERIFY(lone.declinedWithoutSigning());
    }

    /// 形の合わない制御メッセージはジャーナルに書かない。bodyHash が合えば body は任意の配列なので、大きな body で
    /// ジャーナルの上限を使わせない（監査 2026-11-13 P18）
    void malformedControlIsNotJournaled()
    {
        LoneMatch lone(true);
        Match& match = *lone.match;
        match.begin();
        const GameState state0 = GameRules::initialState(lone.gameId, lone.terms);
        lone.accept(state0);
        lone.control(ControlKind::Ready, ControlBody::ready(state0.hash()));
        QCOMPARE(match.phase(), Match::Phase::Playing);
        const size_t before = lone.journal.size();
        // 断りの body は [bodyHash] だが、大きな bytes を入れる
        lone.control(ControlKind::ResultDecline, CborValue::array({CborValue::bytes(QByteArray(60'000, 'x'))}));
        QCOMPARE(lone.journal.size(), before);
        QCOMPARE(match.phase(), Match::Phase::Playing);
        // 形の合う制御メッセージは書く
        lone.control(ControlKind::ResultDecline, ControlBody::decline(QByteArray(32, 'h')));
        QCOMPARE(lone.journal.size(), before + 1);
    }

    /// 承認済みの手に対する承認の再送は、自分に未承認の手があっても無視する。
    /// 提案の再送（5秒）に相手が承認を送り直し、それが、こちらが次の手を提案した後に届くことがある
    void lateDuplicateAcceptanceIsIgnored()
    {
        Pair pair;
        pair.negotiateAndStart();
        Side& black = pair.black();
        Side& white = pair.white();
        const auto ofType = [](const std::vector<Frame>& frames, FrameType type) {
            std::vector<Frame> out;
            for (const Frame& f : frames) {
                if (f.type == type) out.push_back(f);
            }
            return out;
        };
        // 先手の初手。後手は、提案を2回（再送）受けて、承認を2回送る
        pair.clock.ms += 1000;
        QCOMPARE(black.match->playMove(UsiMove::fromString(u"7g7f")), Match::MoveError::None);
        const std::vector<Frame> proposal = black.match->takeOutgoing();
        QCOMPARE(proposal.size(), std::size_t(1));
        white.match->receive(proposal.front());
        white.match->receive(proposal.front());
        const std::vector<Frame> accepts = ofType(white.match->takeOutgoing(), FrameType::StateAccept);
        QCOMPARE(accepts.size(), std::size_t(2));
        black.match->receive(accepts[0]);
        QCOMPARE(black.match->lastState().ply, quint32(1));
        // 後手が指し、先手が受けて、次の手を提案する（未承認の手がある）
        pair.clock.ms += 1000;
        QCOMPARE(white.match->playMove(UsiMove::fromString(u"3c3d")), Match::MoveError::None);
        for (const Frame& f : white.match->takeOutgoing()) black.match->receive(f);
        QCOMPARE(black.match->lastState().ply, quint32(2));
        black.match->takeOutgoing();
        pair.clock.ms += 1000;
        QCOMPARE(black.match->playMove(UsiMove::fromString(u"2g2f")), Match::MoveError::None);
        QVERIFY(black.match->hasPendingMove());
        // 遅れて届いた、初手への2つ目の承認
        black.match->receive(accepts[1]);
        QCOMPARE(black.match->phase(), Match::Phase::Playing);
        QVERIFY(black.match->hasPendingMove());
        // 知らない状態への承認は、これまでどおり止める
        StateAcceptPayload unknown = *StateAcceptPayload::fromCbor(accepts[1].payload);
        unknown.stateHash = QByteArray(32, '\x55');
        Frame bad = accepts[1];
        bad.payload = unknown.toCbor();
        black.match->receive(bad);
        QCOMPARE(black.match->settlement(), Settlement::Error);
        QCOMPARE(black.match->endingError(), std::optional<ErrorCode>(ErrorCode::StateConflict));
    }

    /// 結果が一致した後に届いた着手・承認は取り込まない。引き分けを申し出た側の着手と、相手の承諾が行き違っても、
    /// 双方の記録が、結果の指す手数で終わる（その後の手を承認すると、証跡が検証に通らなくなる）
    void movesCrossingAnAgreementAreNotCommitted()
    {
        Pair pair;
        pair.negotiateAndStart();
        Side& black = pair.black();
        Side& white = pair.white();
        pair.play(black, "7g7f");
        // 後手が引き分けを申し出て、答えを待たずに指す
        QCOMPARE(white.match->offer(Cause::AgreedDraw), Match::OfferResult::Sent);
        const std::vector<Frame> offer = white.match->takeOutgoing();
        pair.clock.ms += 1000;
        QCOMPARE(white.match->playMove(UsiMove::fromString(u"3c3d")), Match::MoveError::None);
        const std::vector<Frame> move = white.match->takeOutgoing();
        // 先手は、着手が届く前に承諾する
        for (const Frame& f : offer) black.match->receive(f);
        QVERIFY(black.match->hasPendingOffer());
        black.match->answerOffer(true);
        QVERIFY(black.match->agreedResult().has_value());
        const std::vector<Frame> echo = black.match->takeOutgoing();
        // 行き違った着手が届く
        for (const Frame& f : move) black.match->receive(f);
        QCOMPARE(black.match->lastState().ply, quint32(1));
        for (const Frame& f : black.match->takeOutgoing()) QVERIFY(f.type != FrameType::StateAccept);
        // 後手は承諾を受けて一致し、双方の確定がそろう
        for (const Frame& f : echo) white.match->receive(f);
        pair.pump();
        QCOMPARE(black.match->settlement(), Settlement::MutuallySigned);
        QCOMPARE(white.match->settlement(), Settlement::MutuallySigned);
        for (Side* s : {&black, &white}) {
            const EvidenceReport report = EvidenceVerifier::verify(s->match->evidence().encode());
            QVERIFY2(report.isValid(), qPrintable(report.error));
            QCOMPARE(report.category, EvidenceReport::Category::MutuallySigned);
            QCOMPARE(report.lastMutualPly, 1);
            QCOMPARE(report.result->cause, Cause::AgreedDraw);
        }
    }

    /// 回線が切れている間に相手の申し出へ答えても、結果を一致させない（承諾を相手へ送れないため）。
    /// つなぎ直した後に相手が申し出を送り直すので、そのとき答える
    void offerAnsweredWhileDisconnectedIsAskedAgain()
    {
        Pair pair;
        pair.negotiateAndStart();
        Side& black = pair.black();
        Side& white = pair.white();
        pair.play(black, "7g7f");
        QCOMPARE(white.match->offer(Cause::AgreedDraw), Match::OfferResult::Sent);
        pair.pump();
        QVERIFY(black.match->hasPendingOffer());
        pair.connected = false;
        black.match->connectionLost();
        white.match->connectionLost();
        black.match->answerOffer(true);
        QVERIFY(!black.match->agreedResult().has_value());
        QCOMPARE(black.match->phase(), Match::Phase::Reconnecting);
        // つなぎ直すと、後手の申し出がもう一度届く
        black.events.clear();
        pair.clock.ms += 3000;
        pair.connected = true;
        black.match->connectionEstablished();
        white.match->connectionEstablished();
        pair.pump();
        QCOMPARE(black.match->phase(), Match::Phase::Playing);
        QVERIFY(pair.hasEvent(black, MatchEvent::Type::OfferReceived));
        QVERIFY(black.match->hasPendingOffer());
        black.match->answerOffer(true);
        pair.pump();
        QCOMPARE(black.match->settlement(), Settlement::MutuallySigned);
        QCOMPARE(white.match->settlement(), Settlement::MutuallySigned);
    }

    void duplicateProposalIsNotAppliedTwice()
    {
        Pair pair;
        pair.negotiateAndStart();
        Side& black = pair.black();
        Side& white = pair.white();
        pair.clock.ms += 1000;
        black.match->playMove(UsiMove::fromString(u"7g7f"));
        std::vector<Frame> frames = black.match->takeOutgoing();
        QCOMPARE(frames.size(), std::size_t(1));
        white.match->receive(frames.front());
        white.match->receive(frames.front());   // 再送
        std::vector<MatchEvent> events = white.match->takeEvents();
        int commits = 0;
        for (const MatchEvent& e : events) commits += e.type == MatchEvent::Type::MoveCommitted ? 1 : 0;
        QCOMPARE(commits, 1);
        QCOMPARE(white.match->phase(), Match::Phase::Playing);
    }

    void storageFailureStopsBeforeSending()
    {
        Pair pair;
        pair.negotiateAndStart();
        pair.black().failPersist = true;
        pair.clock.ms += 1000;
        QCOMPARE(pair.black().match->playMove(UsiMove::fromString(u"7g7f")), Match::MoveError::Finished);
        QCOMPARE(pair.black().match->settlement(), Settlement::Error);
        QCOMPARE(*pair.black().match->endingError(), ErrorCode::StorageError);
    }

    /// 合意した結果の棋譜の終局の行は、ローカル対局・通信対局と同じ表（MatchEnding）で決める（監査 2026-11-12 R1）。
    /// 最大手数は持将棋と書き、行は終局した局面の手番側にそろえる
    void endingLineFollowsMatchEnding_data()
    {
        QTest::addColumn<int>("cause");
        QTest::addColumn<int>("outcome");
        QTest::addColumn<int>("ply");
        QTest::addColumn<int>("terminal");
        QTest::addColumn<int>("side");
        const auto row = [](const char* name, Cause cause, Outcome outcome, int ply, TerminalType terminal, Turn side) {
            QTest::newRow(name) << int(cause) << int(outcome) << ply << int(terminal) << int(side);
        };
        row("resign", Cause::Resign, Outcome::WhiteWins, 10, TerminalType::Resign, Turn::Black);
        row("checkmate", Cause::Checkmate, Outcome::BlackWins, 11, TerminalType::Checkmate, Turn::White);
        row("timeout", Cause::Timeout, Outcome::WhiteWins, 20, TerminalType::Timeout, Turn::Black);
        row("repetition", Cause::Repetition, Outcome::Draw, 30, TerminalType::Repetition, Turn::Black);
        // 王手を続けた後手の反則負けは、手番の先手の反則勝ちの行にする
        row("perpetual-check", Cause::PerpetualCheck, Outcome::BlackWins, 40, TerminalType::IllegalWin, Turn::Black);
        row("agreed-draw", Cause::AgreedDraw, Outcome::Draw, 5, TerminalType::Draw, Turn::White);
        row("max-ply", Cause::MaxPly, Outcome::Draw, 512, TerminalType::Impasse, Turn::Black);
        row("agreed-abort", Cause::AgreedAbort, Outcome::Aborted, 7, TerminalType::Interrupt, Turn::White);
    }

    void endingLineFollowsMatchEnding()
    {
        QFETCH(int, cause);
        QFETCH(int, outcome);
        QFETCH(int, ply);
        QFETCH(int, terminal);
        QFETCH(int, side);
        FinalResult result;
        result.ply = static_cast<quint32>(ply);
        result.cause = static_cast<Cause>(cause);
        result.outcome = static_cast<Outcome>(outcome);
        const RecordEntry::Ending ending =
            MatchEnding::endingOf(GameRules::endInfo(result), GameRules::sideToMove(result.ply));
        QCOMPARE(int(ending.terminal), terminal);
        QCOMPARE(int(ending.side), side);
    }

    void illegalMoveKeepsTheTurn()
    {
        Pair pair;
        pair.negotiateAndStart();
        QCOMPARE(pair.black().match->playMove(UsiMove::fromString(u"7g7e")), Match::MoveError::Illegal);
        QCOMPARE(pair.white().match->playMove(UsiMove::fromString(u"3c3d")), Match::MoveError::NotMyTurn);
        pair.play(pair.black(), "7g7f");
    }
};

QTEST_GUILESS_MAIN(TestP2pMatch)
#include "tst_p2p_match.moc"
