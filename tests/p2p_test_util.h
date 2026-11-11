#ifndef P2P_TEST_UTIL_H
#define P2P_TEST_UTIL_H

/// @file p2p_test_util.h
/// @brief P2P対局の試験で使う、2人分の鍵と開始合意・署名付きの手順を作る補助

#include <QByteArray>
#include <QFile>
#include <QStringList>

#include <utility>
#include <vector>

#include "p2pevidence.h"
#include "p2pgamerules.h"
#include "p2pprotocol.h"
#include "p2psignature.h"

namespace P2pTest {

/// 1人分の鍵（プレイヤー鍵と一局用鍵）
struct Player {
    P2p::SigningKey playerKey = P2p::SigningKey::generate();
    P2p::SigningKey sessionKey = P2p::SigningKey::generate();
};

/// 2人の開始合意（先手 black・後手 white）
inline P2p::StartAgreement makeAgreement(const Player& black, const Player& white, const P2p::TimeControlTerms& terms,
                                         quint64 startBefore = 4'000'000'000ULL)
{
    P2p::StartAgreement a;
    a.networkId = P2p::networkId("sbq-local-1");
    a.blackNonce = P2p::Crypto::randomBytes(32);
    a.whiteNonce = P2p::Crypto::randomBytes(32);
    a.blackPlayerKey = black.playerKey.publicKey();
    a.whitePlayerKey = white.playerKey.publicKey();
    a.blackSessionKey = black.sessionKey.publicKey();
    a.whiteSessionKey = white.sessionKey.publicKey();
    a.blackPeerIdHash = P2p::Cbor::sha256("peer-black");
    a.whitePeerIdHash = P2p::Cbor::sha256("peer-white");
    a.rulesHash = P2p::Rules::v1Hash();
    a.timeControlHash = terms.hash();
    a.networkPolicyHash = P2p::NetworkPolicy::v1().hash();
    a.startBefore = startBefore;
    return a;
}

/// 両方の一局用鍵で承認した状態
inline P2p::SignedState approveBoth(const P2p::GameState& state, const Player& black, const Player& white)
{
    const P2p::CborValue approval = P2p::stateApproval(state.gameId, state.ply, state.hash());
    return P2p::SignedState{state, black.sessionKey.sign(approval), white.sessionKey.sign(approval)};
}

/// 開始局面から moves を順に指した、両署名付きの手順（消費時間は1手 spentMs）
inline std::vector<P2p::SignedState> playMoves(const QByteArray& gameId, const P2p::TimeControlTerms& terms,
                                               const QStringList& moves, const Player& black, const Player& white,
                                               quint64 spentMs = 1000)
{
    std::vector<P2p::SignedState> states;
    P2p::GameState state = P2p::GameRules::initialState(gameId, terms);
    states.push_back(approveBoth(state, black, white));
    for (const QString& text : moves) {
        const std::optional<P2p::GameState> next =
            P2p::GameRules::nextState(state, UsiMove::fromString(text), spentMs, terms);
        if (!next) return {};
        state = *next;
        states.push_back(approveBoth(state, black, white));
    }
    return states;
}

/// 証跡（開始署名つき。結果は呼ぶ側が入れる）
inline P2p::Evidence makeEvidence(const Player& black, const Player& white, const P2p::TimeControlTerms& terms,
                                  const QStringList& moves)
{
    P2p::Evidence e;
    e.agreement = makeAgreement(black, white, terms);
    e.timeControl = terms;
    e.blackStartSignature = black.playerKey.sign(e.agreement.toCbor());
    e.whiteStartSignature = white.playerKey.sign(e.agreement.toCbor());
    e.states = playMoves(e.gameId(), terms, moves, black, white);
    return e;
}

/// 証跡の最後の状態を指す結果
inline P2p::FinalResult resultFor(const P2p::Evidence& e, P2p::Outcome outcome, P2p::Cause cause)
{
    std::vector<P2p::GameState> states;
    for (const P2p::SignedState& s : e.states) states.push_back(s.state);
    P2p::FinalResult r;
    r.gameId = e.gameId();
    r.finalStateHash = states.back().hash();
    r.transcriptHash = P2p::transcriptHash(r.gameId, states);
    r.ply = states.back().ply;
    r.outcome = outcome;
    r.cause = cause;
    return r;
}

/// 千日手になる手順（飛車と飛車を往復させ、開始局面が4回目に現れる）
inline QStringList repetitionMoves()
{
    QStringList moves;
    for (int i = 0; i < 3; ++i) moves << QStringLiteral("2h3h") << QStringLiteral("8b7b") << QStringLiteral("3h2h") << QStringLiteral("7b8b");
    return moves;
}

/// protocol/testvectors/<name> の行（# と空行を除く）
inline QList<QByteArray> readVectorLines(const QString& name)
{
    QFile file(QStringLiteral(SOURCE_DIR "/protocol/testvectors/") + name);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QList<QByteArray> lines;
    for (QByteArray line : file.readAll().split('\n')) {
        line = line.trimmed();
        if (line.isEmpty() || line.startsWith('#')) continue;
        lines.append(line);
    }
    return lines;
}

} // namespace P2pTest

#endif // P2P_TEST_UTIL_H
