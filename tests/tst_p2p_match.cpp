/// @file tst_p2p_match.cpp
/// @brief 開始までの交渉と対局の状態機械の試験（2人分を通信プロセスなしでつなぐ。実装仕様 13.1節）

#include <QtTest>

#include <deque>

#include "p2p_test_util.h"
#include "p2pevidenceverifier.h"
#include "p2pmatch.h"
#include "p2pnegotiation.h"

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

    Pair(SidePreference applicantSide = SidePreference::None, SidePreference hostSide = SidePreference::None)
    {
        applicant.peerId = fakePeerId('a');
        host.peerId = fakePeerId('h');
        applicant.negotiation = std::make_unique<Negotiation>(config(kRoleApplicant, applicantSide), &applicant.playerKey);
        host.negotiation = std::make_unique<Negotiation>(config(kRoleHost, hostSide), &host.playerKey);
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
        c.offerId = zeroHash();
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
                const Frame f = toHost.front();
                toHost.pop_front();
                deliver(host, f);
            }
            if (!toApplicant.empty()) {
                const Frame f = toApplicant.front();
                toApplicant.pop_front();
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
    void startMatches()
    {
        for (Side* s : {&applicant, &host}) {
            Negotiation& n = *s->negotiation;
            QCOMPARE(n.stage(), Negotiation::Stage::Started);
            MatchSetup setup;
            setup.agreement = *n.agreement();
            setup.terms = n.terms();
            setup.iAmBlack = n.iAmBlack();
            setup.blackStartSignature = n.blackStartSignature();
            setup.whiteStartSignature = n.whiteStartSignature();
            setup.myConnectionRandom = n.myConnectionRandom();
            setup.opponentConnectionRandom = n.opponentConnectionRandom();
            setup.nextRequestId = n.nextRequestId();
            Side* side = s;
            s->match = std::make_unique<Match>(
                setup, n.takeSessionKey(), &s->playerKey, [this]() { return clock.ms; },
                [side](const CborValue& record) {
                    if (side->failPersist) return false;
                    side->journal.push_back(record);
                    return true;
                });
        }
        applicant.match->begin();
        host.match->begin();
        pump();
    }

    void negotiateAndStart()
    {
        applicant.negotiation->begin();
        pump();
        QCOMPARE(applicant.negotiation->stage(), Negotiation::Stage::Confirming);
        QCOMPARE(host.negotiation->stage(), Negotiation::Stage::Confirming);
        host.negotiation->confirm();
        applicant.negotiation->confirm();
        pump();
        startMatches();
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

} // namespace

class TestP2pMatch : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QVERIFY(Crypto::initialize()); }

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
        QVERIFY(pair.host.negotiation->applicantSigned());
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
            QCOMPARE(s->match->ending(), MatchEnding::MutuallySigned);
            const EvidenceReport report = EvidenceVerifier::verify(s->match->evidence().encode());
            QVERIFY2(report.isValid(), qPrintable(report.error));
            QCOMPARE(report.category, EvidenceReport::Category::MutuallySigned);
            QCOMPARE(report.result->outcome, Outcome::WhiteWins);
            QCOMPARE(report.result->cause, Cause::Resign);
        }
        QCOMPARE(pair.black().match->evidence().encode(), pair.white().match->evidence().encode());
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
            QCOMPARE(s->match->ending(), MatchEnding::MutuallySigned);
            QCOMPARE(s->match->agreedResult()->cause, Cause::Repetition);
        }
    }

    void drawOfferNeedsTheOtherUser()
    {
        Pair pair;
        pair.negotiateAndStart();
        pair.play(pair.black(), "7g7f");
        QVERIFY(pair.black().match->offer(Cause::AgreedDraw));
        QVERIFY(!pair.black().match->offer(Cause::AgreedDraw));   // 同じ手数で1回まで
        pair.pump();
        QVERIFY(pair.white().match->hasPendingOffer());
        pair.white().match->answerOffer(false);
        pair.pump();
        QVERIFY(pair.hasEvent(pair.black(), MatchEvent::Type::ProposalDeclined));
        QCOMPARE(pair.black().match->phase(), Match::Phase::Playing);
        // 手を進めてから、後手が中断を申し出て先手が受ける
        pair.play(pair.white(), "3c3d");
        QVERIFY(pair.white().match->offer(Cause::AgreedAbort));
        pair.pump();
        pair.black().match->answerOffer(true);
        pair.pump();
        QCOMPARE(pair.black().match->agreedResult()->outcome, Outcome::Aborted);
        QCOMPARE(pair.white().match->ending(), MatchEnding::MutuallySigned);
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
        QCOMPARE(white.match->ending(), MatchEnding::Error);
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
            QCOMPARE(s->match->ending(), MatchEnding::MutuallySigned);
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
    }

    void reconnectTimeoutLeavesUnconfirmedRecord()
    {
        Pair pair;
        pair.negotiateAndStart();
        pair.play(pair.black(), "7g7f");
        pair.black().match->connectionLost();
        pair.clock.ms += 121'000;
        pair.black().match->tick();
        QCOMPARE(pair.black().match->ending(), MatchEnding::Disconnected);
        const EvidenceReport report = EvidenceVerifier::verify(pair.black().match->evidence().encode());
        QVERIFY2(report.isValid(), qPrintable(report.error));
        QCOMPARE(report.category, EvidenceReport::Category::Unconfirmed);
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
        QCOMPARE(pair.black().match->ending(), MatchEnding::Error);
        QCOMPARE(*pair.black().match->endingError(), ErrorCode::StorageError);
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
