#ifndef P2PGAMECOORDINATOR_H
#define P2PGAMECOORDINATOR_H

/// @file p2pgamecoordinator.h
/// @brief P2P対局の司令塔（交渉・着手の合意・再接続・終局を、盤・時計・棋譜・動作状態・証跡へつなぐ。実装仕様 10.2節）

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>

#include <memory>
#include <optional>

#include "appactivitystate.h"
#include "gamechannel.h"
#include "p2pmatch.h"
#include "p2pnegotiation.h"
#include "usimove.h"

class GameBoard;
class MatchRecordSink;
class P2pJournal;
class P2pKnownPeers;
class P2pRecordStore;
class P2pTransport;
class ShogiClock;
class QTimer;

/**
 * @brief 1つの交渉と1局を受け持つ司令塔
 *
 * 将棋の合法性と対局の進行、署名するかどうかはここ（と P2p::Match）が決める。通信プロセスから届いたものは
 * 外枠からすべて確かめ直す。両方の署名のない状態を盤・棋譜に入れない。棋譜を作ってから「P2P対局中」にし、
 * 終局では棋譜を確定してから手放す。相手の結果への署名の待ちは、棋譜の閲覧を止めない。
 */
class P2pGameCoordinator : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        GameBoard* gameBoard = nullptr;
        ShogiClock* clock = nullptr;
        AppActivityState* activity = nullptr;
    };

    /// 交渉を始めるのに要るもの
    struct NegotiationRequest {
        quint64 connection = 0;
        QString opponentPeerId;          ///< 文字列表記（再接続に使う）
        QByteArray opponentPeerIdBytes;  ///< バイナリ（開始合意の PeerId のハッシュ）
        quint8 role = P2p::kRoleApplicant;
        QString myDisplayName;
        P2p::SidePreference mySide = P2p::SidePreference::None;
        QByteArray offerId;              ///< 招待ではゼロ32バイト
        P2p::TimeControlTerms hostTerms;
        std::optional<P2p::TimeControlTerms> expectedTerms;
        std::optional<QString> expectedDisplayName;
        std::optional<P2p::SidePreference> expectedHostSide;
        bool viaRelay = false;
    };

    enum class Phase { Idle, Negotiating, Playing, Finished };
    Q_ENUM(Phase)

    explicit P2pGameCoordinator(const Deps& deps, QObject* parent = nullptr);
    ~P2pGameCoordinator() override;

    void setRecordSink(MatchRecordSink* sink) { m_sink = sink; }
    /// P2P対局を使う間の部品（通信の窓口・保存先・既知の相手・プレイヤー鍵）。どれも非所有
    void setEnvironment(P2pTransport* transport, P2pRecordStore* store, P2pKnownPeers* knownPeers,
                        const P2p::SigningKey* playerKey);

    Phase phase() const { return m_phase; }
    /// 交渉か対局の途中か（新しい申込みを受けない）
    bool isBusy() const { return m_phase == Phase::Negotiating || m_phase == Phase::Playing; }
    bool ownsConnection(quint64 connection) const { return connection != 0 && connection == m_connection; }
    /// 今の交渉・対局の接続（無ければ 0）
    quint64 currentConnection() const { return m_connection; }

    /// 交渉を始める（申込側は HELLO を送る）
    bool startNegotiation(const NegotiationRequest& request);
    /// 通信プロセスからの知らせ
    void onConnectionOpened(quint64 connection, const QString& peerId, bool viaRelay);
    void onFrame(quint64 connection, const QByteArray& frame);
    void onConnectionClosed(quint64 connection);
    void onRouteChanged(quint64 connection, bool viaRelay);

    // --- 交渉（条件確認画面） ---
    const P2p::Negotiation* negotiation() const { return m_negotiation.get(); }
    void confirmStart();
    void cancelNegotiation();

    // --- 対局（P2pGameChannel） ---
    const P2p::Match* match() const { return m_match.get(); }
    GameChannel::MoveResult playMove(const UsiMove& move);
    bool canResign() const;
    void resign();
    void offerDraw();
    void offerAbort();
    void answerOffer(bool accept);
    /// 対局をやめる（ABORT。結果未確定）
    void leaveGame();
    bool isInProgress() const;
    bool isHumanToMove() const;
    Turn sideToMove() const;

    // --- 画面に出す値 ---
    QString opponentDisplayName() const { return m_opponentDisplayName; }
    QByteArray opponentPlayerKey() const;
    bool viaRelay() const { return m_viaRelay; }
    /// 最後に保存した証跡の場所
    QString lastEvidencePath() const { return m_lastEvidencePath; }
    /// 仮予約の残り（ミリ秒。交渉中でなければ 0）
    qint64 reservationRemainingMs() const
    {
        return m_phase == Phase::Negotiating ? qMax<qint64>(0, m_reservationDeadline - now()) : 0;
    }
    /// 自分の役割（申込側か募集側か）
    quint8 role() const { return m_role; }

signals:
    /// 交渉の段階が変わった（条件確認画面を出す・更新する）
    void negotiationChanged();
    /// 交渉が終わった（started なら対局が始まった。そうでなければ code が理由）
    void negotiationEnded(bool started, std::optional<P2p::ErrorCode> code, bool byPeer);
    /// 申込側の開始署名を確かめた（募集側は募集を取り下げる）
    void applicantVerified(const QString& peerId);
    /// 対局が始まった（棋譜を作り、P2P対局中にした後）
    void gameStarted();
    /// 対局の状態（手番・確認待ち・再接続・経路）が変わった
    void statusChanged();
    /// 相手が合意引き分け・合意中断を申し出た
    void offerReceived(P2p::Cause cause);
    void proposalDeclined();
    /// 結果が一致した（棋譜を確定し、P2P対局中を手放した後）
    void resultAgreed(const P2p::FinalResult& result);
    /// 対局が終わった（証跡を保存した後）
    void gameFinished(P2p::MatchEnding ending, std::optional<P2p::ErrorCode> error, const QString& evidencePath);
    /// 交渉に失敗した相手（再申込みの拒否の対象。開始署名で確かめたプレイヤーID があれば添える）
    void negotiationAbandoned(const QString& peerId, const QByteArray& verifiedPlayerKey);

private slots:
    void onTick();

private:
    qint64 now() const { return m_elapsed.elapsed(); }
    void flushNegotiation();
    void flushMatch();
    void sendFrames(const std::vector<P2p::Frame>& frames);
    void endNegotiation(bool started);
    void startMatch();
    void handleMatchEvent(const P2p::MatchEvent& event);
    void applyCommittedMove(const P2p::MatchEvent& event);
    void syncClock();
    void writeEndingRecord(const P2p::FinalResult* result);
    void releaseGame();
    void saveEvidence();
    void resetMatch();
    /// 計時の連続性を失った（スリープ・時計の飛び）なら true（実装仕様 7.3節）
    bool clockDiscontinuity();

    Deps m_deps;
    MatchRecordSink* m_sink = nullptr;
    P2pTransport* m_transport = nullptr;
    P2pRecordStore* m_store = nullptr;
    P2pKnownPeers* m_knownPeers = nullptr;
    const P2p::SigningKey* m_playerKey = nullptr;

    Phase m_phase = Phase::Idle;
    QElapsedTimer m_elapsed;
    QTimer* m_ticker = nullptr;
    quint64 m_connection = 0;
    QString m_opponentPeerId;
    QString m_opponentDisplayName;
    QString m_myDisplayName;
    quint8 m_role = P2p::kRoleApplicant;
    bool m_viaRelay = false;
    qint64 m_reservationDeadline = 0;   ///< 仮予約の期限（条件の確認と開始署名まで60秒）
    qint64 m_lastTickMono = -1;         ///< 前の確認の単調増加時計（スリープの検出）
    qint64 m_lastTickWallMs = 0;        ///< 前の確認の OS の時刻
    qint64 m_nextReconnectAt = 0;

    std::unique_ptr<P2p::Negotiation> m_negotiation;
    std::unique_ptr<P2p::Match> m_match;
    std::unique_ptr<P2pJournal> m_journal;
    ActivityLease m_lease;              ///< P2P対局中（棋譜を作ってから受け取り、終局で棋譜を確定してから手放す）
    bool m_recordFinished = false;
    BoardSquare m_previousTo;           ///< 直前の手の移動先（棋譜の「同」）
    QString m_lastEvidencePath;
};

#endif // P2PGAMECOORDINATOR_H
