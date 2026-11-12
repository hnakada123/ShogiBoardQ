#ifndef P2PMATCH_H
#define P2PMATCH_H

/// @file p2pmatch.h
/// @brief 開始後の対局の状態機械（State0・着手の合意・時計の観測・再接続・結果の提案と署名。実装仕様 6〜8章）

#include <QByteArray>
#include <QString>

#include <functional>
#include <optional>
#include <vector>

#include "p2pevidence.h"
#include "p2pframes.h"
#include "p2pgamerules.h"
#include "p2pmatchtimer.h"
#include "p2psettlement.h"
#include "p2psignature.h"

namespace P2p {

/// 対局を始めるのに要るもの（交渉の結果）
struct MatchSetup {
    StartAgreement agreement;
    TimeControlTerms terms;
    bool iAmBlack = true;
    QByteArray blackStartSignature;
    QByteArray whiteStartSignature;
    QByteArray myConnectionRandom;
    QByteArray opponentConnectionRandom;
    /// この接続で次に送るフレームの requestId（交渉から続けて増やす）
    quint64 nextRequestId = 1;
    /// 相手が名乗った表示名（ジャーナルの開始の件に書く。証跡には入れない）
    QString opponentDisplayName;
};

/// 対局からの知らせ
struct MatchEvent {
    enum class Type {
        Ready,             ///< State0 に両方が署名し、READY を交わした（先手の初手の計時が始まる）
        MoveCommitted,     ///< 両方の署名がそろった着手（盤・棋譜・時計へ反映する）
        OfferReceived,     ///< 相手が合意引き分け・合意中断を申し出た（利用者に尋ねる）
        ProposalDeclined,  ///< 自分の提案（申し出・時間切れの主張）を相手が断った
        ResultAgreed,      ///< 結果が一致した（以後、最終結果への署名を交わす）
        Reconnecting,      ///< 接続が切れた・着手の確認が遅れた（再接続を始める）
        Resumed,           ///< 再接続して状態を照合した
        Finished,          ///< 対局が終わった（証跡を保存する）
    };
    Type type = Type::MoveCommitted;
    QByteArray moveUsi;    ///< MoveCommitted
    quint64 spentMs = 0;   ///< MoveCommitted: その手の消費時間（ミリ秒。指した側の申告）
    std::optional<FinalResult> result;   ///< OfferReceived・ResultAgreed・Finished（結果が一致していれば）
};

/**
 * @brief 開始後の1局
 *
 * フレームを順に渡し（receive）、利用者の操作（playMove・resign・offer）を渡すと、送るフレームと知らせができる。
 * 時刻は単調増加時計の関数で受け取り、tick() で期限を確かめる。ソケット・タイマー・UI に依存しない。
 * 送る前の提案と ACK の前の受理証明は persist に書いてから送る（書けなければ中断する）。
 */
class Match
{
public:
    using MonotonicClock = std::function<qint64()>;
    /// ジャーナルへの記録（書けなければ false）
    using Persist = std::function<bool(const CborValue& record)>;

    enum class Phase { Opening, Playing, Reconnecting, Finishing, Finished };
    enum class MoveError { None, NotMyTurn, Pending, Illegal, TimeUp, Finished };

    Match(MatchSetup setup, SigningKey sessionKey, const SigningKey* playerKey, MonotonicClock clock, Persist persist);

    /// 開始する（開始合意と両方の開始署名をジャーナルに書いてから。先手は State0 を提案する）
    void begin();
    void receive(const Frame& frame);
    /// 時間に関わる確認（再送・復旧・切断の上限・時間切れ・署名の待ち）。定期的に呼ぶ
    void tick();

    /// 手番の利用者が指す
    MoveError playMove(const UsiMove& move);
    /// 投了（自分の手番に限る）
    bool resign();
    /// 合意引き分け・合意中断を申し出る（同じ手数でそれぞれ1回まで）
    bool offer(Cause cause);
    /// 相手の申し出に答える
    void answerOffer(bool accept);
    /// 対局をやめる（ABORT を送り、結果未確定で終える）
    void abort(ErrorCode reason = ErrorCode::Cancelled);

    /// 接続が切れた（Reconnecting にする。計時は続ける）
    void connectionLost();
    /// 新しい接続ができた（再接続の HELLO を送る）
    void connectionEstablished();

    std::vector<Frame> takeOutgoing();
    std::vector<MatchEvent> takeEvents();

    Phase phase() const { return m_phase; }
    bool iAmBlack() const { return m_setup.iAmBlack; }
    QByteArray gameId() const { return m_gameId; }
    const StartAgreement& agreement() const { return m_setup.agreement; }
    Settlement settlement() const { return m_settlement; }
    std::optional<ErrorCode> endingError() const { return m_endingError; }
    /// 合意した結果
    const std::optional<FinalResult>& agreedResult() const { return m_agreedResult; }
    /// 自分の手番か（最新の両署名付きの状態の手番）
    bool isMyTurn() const;
    /// 確認待ちの自分の手があるか
    bool hasPendingMove() const { return m_pending.has_value(); }
    /// 両方の署名がそろった最新の状態。開始局面（State0）の承認がそろう前は、手数 0 の空の状態を返す
    /// （開始の手続きの途中で対局が終わったとき・相手が最初に壊れた提案を送ったときにも呼ばれる。空の列の末尾を読まない）
    const GameState& lastState() const;
    const std::vector<SignedState>& confirmedStates() const { return m_confirmed; }
    /// 1局の切断時間の合計（ミリ秒）
    qint64 reconnectTotalMs() const;
    /// 再接続の待ちの残り（ミリ秒。Reconnecting でなければ -1）
    qint64 reconnectRemainingMs() const;
    /// 相手の申し出に答える前か
    bool hasPendingOffer() const { return m_pendingOffer.has_value(); }
    /// 相手が同じ手数の異なる状態に署名した証明をジャーナルに書いたか（ジャーナルを残す）
    bool hasConflictProof() const { return m_conflictProof; }
    /// 証跡（いつでも作れる。結果がまだなければ空の結果）
    Evidence evidence() const;

private:
    // --- 共通 ---
    void send(FrameType type, const CborValue& payload);
    void emitEvent(MatchEvent event) { m_events.push_back(std::move(event)); }
    bool persist(const CborValue& record);
    void finish(Settlement ending, std::optional<ErrorCode> error);
    void failWithError(ErrorCode code, quint32 relatedPly);
    quint8 mySide() const { return m_setup.iAmBlack ? kSideBlack : kSideWhite; }
    QByteArray opponentSessionKey() const { return m_setup.agreement.sessionKey(!m_setup.iAmBlack); }
    QByteArray opponentPlayerKey() const { return m_setup.agreement.playerKey(!m_setup.iAmBlack); }
    SignedState signedByMe(const GameState& state) const;
    bool verifyApproval(const GameState& state, const QByteArray& signature, bool black) const;
    qint64 now() const { return m_clock ? m_clock() : 0; }

    // --- State0 と着手（p2pmatch.cpp） ---
    void handleStatePropose(const StateProposePayload& payload);
    void handleStateAccept(const StateAcceptPayload& payload);
    void commit(const SignedState& state);
    void checkTerminal();

    // --- 制御・結果・再接続（p2pmatch_control.cpp） ---
    void sendControl(ControlKind kind, const CborValue& body);
    void handleControl(const ControlPayload& payload);
    void handleReady(const CborValue& body);
    void handleResultPropose(const CborValue& body, const QByteArray& bodyHash);
    void handleResultSignature(const ResultSignaturePayload& payload);
    void handleReconnectHello(const ReconnectHello& hello);
    void handleResume(const CborValue& body);
    void handleSyncRequest(const SyncRequestPayload& request);
    void handleSyncChunk(const SyncChunkPayload& chunk);
    void finishResume();
    FinalResult resultFor(Outcome outcome, Cause cause) const;
    bool proposeResult(Outcome outcome, Cause cause);
    void agree(const FinalResult& result);
    bool resultIsCurrent(const FinalResult& result) const;
    void resendPending();

    MatchSetup m_setup;
    SigningKey m_sessionKey;
    const SigningKey* m_playerKey = nullptr;   ///< 非所有
    MonotonicClock m_clock;
    Persist m_persist;
    QByteArray m_gameId;

    Phase m_phase = Phase::Opening;
    Settlement m_settlement = Settlement::None;
    std::optional<ErrorCode> m_endingError;
    bool m_conflictProof = false;
    std::vector<Frame> m_outgoing;
    std::vector<MatchEvent> m_events;
    quint64 m_requestId = 1;

    // 状態
    std::vector<SignedState> m_confirmed;
    std::optional<SignedState> m_pending;     ///< 相手の承認を待つ自分の状態
    QByteArray m_pendingFrame;                ///< その STATE_PROPOSE（再送は同じバイト列）
    bool m_pendingSent = false;
    qint64 m_pendingSentAt = 0;               ///< 最後に送った時刻（再送の間隔）

    // 接続と制御
    QByteArray m_connectionId;
    QByteArray m_myRandom;
    QByteArray m_opponentRandom;
    quint64 m_controlOut = 0;
    quint64 m_controlIn = 0;
    quint64 m_controlCount = 0;
    bool m_resumeSent = false;
    bool m_resumeReceived = false;
    bool m_syncing = false;
    bool m_readyDone = false;

    // 時計
    qint64 m_startedAt = 0;          ///< 対局の開始（1局の実時間の上限）
    qint64 m_openingDeadline = 0;
    MatchTimer m_timer;              ///< 自分の消費時間・相手の手番の観測・切断の時間
    quint32 m_claimedTimeoutPly = 0xFFFFFFFFu;
    bool m_myTimeoutDeclared = false;

    // 結果
    std::optional<FinalResult> m_myProposal;
    QByteArray m_myProposalHash;
    std::optional<FinalResult> m_pendingOffer;   ///< 相手の申し出（利用者の答え待ち）
    QByteArray m_pendingOfferHash;
    quint32 m_offeredDrawPly = 0xFFFFFFFFu;
    quint32 m_offeredAbortPly = 0xFFFFFFFFu;
    std::optional<FinalResult> m_agreedResult;
    QByteArray m_myResultSignature;
    QByteArray m_opponentResultSignature;
    qint64 m_resultAgreedAt = 0;
};

} // namespace P2p

#endif // P2PMATCH_H
