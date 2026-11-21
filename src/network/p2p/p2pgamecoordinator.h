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
#include "usimatchhandler.h"
#include "usimove.h"

class EngineSlots;
class GameBoard;
class MatchRecordSink;
class P2pEnginePlayer;
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
 *
 * 自分の手は、人間が盤で指すか、登録エンジンが指す（PlayerSetup）。エンジンで指すことは交渉で相手に伝え、双方が署名する
 * 開始合意の Rules に入る。エンジンは交渉を始めたときに枠 E1 で起動し、初期化（readyok）が済むまで開始署名を送らない。
 * エンジンに送る局面と持ち時間は P2pEnginePlayer が双方の署名した手順から作り、エンジンの指し手・投了・入玉宣言・
 * 異常を対局にどう反映するか（相手へ送る・投了・申し出・中断）はこの司令塔が決める。
 */
class P2pGameCoordinator : public QObject
{
    Q_OBJECT
public:
    struct Deps {
        GameBoard* gameBoard = nullptr;
        ShogiClock* clock = nullptr;
        AppActivityState* activity = nullptr;
        EngineSlots* engines = nullptr;   ///< エンジンで指すとき、エンジンを置く枠（無ければエンジンでは指せない）
    };

    /// 自分の対局者（人間が盤で指すか、登録エンジンが指すか）
    struct PlayerSetup {
        P2p::PlayerKind kind = P2p::PlayerKind::Human;
        QString enginePath;
        QString engineName;   ///< 登録名

        bool isEngine() const { return kind == P2p::PlayerKind::Engine; }
    };

    /// 自分が名乗る表示名と、対局の条件（「招待」タブの欄の値。持ち時間と先後は、招待した側・友だちの申し込みを
    /// 受けた側の条件）
    struct Conditions {
        QString displayName;
        P2p::TimeControlTerms terms;
        P2p::SidePreference side = P2p::SidePreference::None;
        PlayerSetup player;

        /// 交渉を始められるか（表示名と持ち時間が規則に合う）
        bool isValid() const;
        /// 申し込む側として使う条件（持ち時間と先後は、相手の HELLO の応答で初めて知るので、決めない）
        Conditions asApplicant() const
        {
            Conditions c;
            c.displayName = displayName;
            c.player = player;
            return c;
        }
    };

    /// 交渉を始めるのに要るもの
    struct NegotiationRequest {
        quint64 connection = 0;
        QString opponentPeerId;          ///< 文字列表記（再接続に使う）
        QByteArray opponentPeerIdBytes;  ///< バイナリ（開始合意の PeerId のハッシュ）
        quint8 role = P2p::kRoleApplicant;
        /// 自分の条件（申込側は表示名と対局者だけを使う。asApplicant）
        Conditions mine;
        bool viaRelay = false;
        /// 友だちとの交渉（友だちの在席の知らせで始めた申し込み）で、相手が名乗るべきプレイヤーID。招待では空
        QByteArray friendPlayerKey;
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

    /// 交渉を始める（申込側は HELLO を送る。エンジンで指すなら、エンジンを起動する）
    bool startNegotiation(const NegotiationRequest& request);
    /// 通信プロセスからの知らせ
    void onConnectionOpened(quint64 connection, const QString& peerId, bool viaRelay);
    void onFrame(quint64 connection, const QByteArray& frame);
    void onConnectionClosed(quint64 connection);
    void onRouteChanged(quint64 connection, bool viaRelay);

    // --- 交渉（条件確認画面） ---
    const P2p::Negotiation* negotiation() const { return m_negotiation.get(); }
    /// 開始を押す（エンジンの初期化が済むまでは何もしない）
    void confirmStart();
    void cancelNegotiation();
    /// エンジンの初期化（usi〜readyok）を待っているか（済むまで開始を押せない）
    bool isEngineStarting() const;

    // --- 対局（P2pGameChannel） ---
    const P2p::Match* match() const { return m_match.get(); }
    GameChannel::MoveResult playMove(const UsiMove& move);
    /// 投了できるか（自分の手番。エンジンに指させている対局でも、人が投了できる）
    bool canResign() const;
    void resign();
    /// 自分のエンジンが手番で思考中なら、すぐ指させる
    void forceImmediateMove();
    void offerDraw();
    void offerAbort();
    void answerOffer(bool accept);
    /// 対局をやめる（ABORT。結果未確定）
    void leaveGame();
    bool isInProgress() const;
    bool isHumanToMove() const;
    Turn sideToMove() const;

    // --- 画面に出す値 ---
    /// 自分の対局者（今の交渉・対局のもの）
    const PlayerSetup& player() const { return m_player; }
    /// 自分の手をエンジンに指させているか
    bool playsWithEngine() const { return m_player.isEngine(); }
    /// 相手の対局者の種別（交渉で分かる前は nullopt）
    std::optional<P2p::PlayerKind> opponentKind() const;
    QString opponentDisplayName() const { return m_opponentDisplayName; }
    QByteArray opponentPlayerKey() const;
    bool viaRelay() const { return m_viaRelay; }
    /// 今の交渉が、友だちの申し込み（友だち一覧から始めた・友だちから届いた）か
    bool isFriendNegotiation() const { return m_phase == Phase::Negotiating && m_friendNegotiation; }
    /// 今の交渉・対局で、自分が招待した側（友だちの申し込みを受けた側）か
    bool isHost() const { return m_role == P2p::kRoleHost; }
    /// 最後に保存した証跡の場所
    QString lastEvidencePath() const { return m_lastEvidencePath; }
    /// 条件の確認と開始署名の期限までの残り（ミリ秒。交渉中でなければ 0）
    qint64 confirmationRemainingMs() const
    {
        return m_phase == Phase::Negotiating ? qMax<qint64>(0, m_confirmationDeadline - now()) : 0;
    }

    /// 自分の手を指すエンジン（エンジンで指す交渉を始めるまで nullptr）
    const P2pEnginePlayer* enginePlayer() const { return m_engine; }

signals:
    /// 交渉の段階が変わった（条件確認画面を出す・更新する。エンジンの初期化が済んだときも）
    void negotiationChanged();
    /// 交渉が終わった（started なら対局が始まった。そうでなければ code が理由）
    void negotiationEnded(bool started, std::optional<P2p::ErrorCode> code, bool byPeer);
    /// 招待で接続した側で、相手の応答（HELLO）が1つも届かないうちに接続が閉じた。招待した側は、招待のトークンが
    /// 合わない接続を何も返さずに閉じるので、合言葉・招待文字列が違う・期限が過ぎた・もう使われた・取り消された
    /// ときに起こる（negotiationEnded の後に出す。negotiationEnded の理由は自分の取りやめと同じで、知らせが出ない）
    void inviteRefused();
    /// 対局が始まった（棋譜を作り、P2P対局中にした後）
    void gameStarted();
    /// エンジンで指す交渉を始める（エンジンを起動する前。前の対局の通信ログを消す）
    void engineStarting();
    /// 自分のエンジンで指す対局が始まった（gameStarted の後。side は自分の手番、engineName は登録名。
    /// 棋譜の対局者は表示名なので、評価値グラフの系列名はこれで知らせる。CsaGameCoordinator と同じ形）
    void matchEngineStarted(Turn side, const QString& engineName);
    /// 自分のエンジンが指した手を相手へ送った（score は自分から見た評価値。グラフの ply 手目に足す）
    void engineMovePlayed(Turn side, int ply, const UsiScore& score);
    /// エンジンについての知らせ（起動の失敗・異常終了・指せない手・入玉宣言）。交渉の取りやめ・投了・中断は済ませてから出す
    void engineNotice(const QString& message);
    /// 対局の状態（手番・確認待ち・再接続・経路）が変わった
    void statusChanged();
    /// 相手が合意引き分け・合意中断を申し出た
    void offerReceived(P2p::Cause cause);
    /// 自分の申し出（合意引き分け・合意中断）を相手へ送り、答えを待っている（待っている間に押し直したときも出す。
    /// cause は相手が尋ねられている申し出）
    void offerPending(P2p::Cause cause);
    /// 自分の申し出を送れなかった（開始の手続き中・再接続中・指した手の確認待ち）
    void offerNotSent();
    /// 自分の提案（申し出・時間切れの主張）を相手が断った
    void proposalDeclined();
    /// 結果が一致した（棋譜を確定し、P2P対局中を手放した後）
    void resultAgreed(const P2p::FinalResult& result);
    /// 対局が終わった（証跡を保存した後）
    void gameFinished(P2p::Settlement ending, std::optional<P2p::ErrorCode> error, const QString& evidencePath);

private slots:
    /// 既知の相手の記録が変わった（交渉の相手をブロックした・友だちから外したら、交渉をやめる）
    void onKnownPeersChanged();
    void onTick();
    void onEngineInitialized();
    void onEngineError(const QString& message);
    void onEngineMoveReady(const UsiMatchMove& move);
    void onEngineResigned();
    void onEngineDeclaredWin();

private:
    qint64 now() const { return m_elapsed.elapsed(); }
    /// 合意引き分け・合意中断を申し出て、申し出た側に結果（答えを待っている・送れなかった）を知らせる
    void offer(P2p::Cause cause);
    void flushNegotiation();
    /// 相手の HELLO を受け入れたら、自分の手を指すエンジンを起動する（エンジンで指す交渉だけ。1回だけ）
    void launchEngine();
    void flushMatch();
    void sendFrames(const std::vector<P2p::Frame>& frames);
    void endNegotiation(bool started);
    void startMatch();
    void handleMatchEvent(const P2p::MatchEvent& event);
    void applyCommittedMove(const P2p::MatchEvent& event);
    void syncClock();
    /// 自分の手を相手へ送った時点で、自分の時計を署名した残り時間で止める（盤は承認を受けてから動かす）
    void onMoveSent();
    /// 自分の手を受け付ける時点か（対局中で、自分の手番で、確認待ちの手がなく、棋譜から終局が決まっていない。
    /// 人間・エンジンで共通）
    bool isMyMoveExpected() const;
    Turn mySide() const;
    /// 自分の手番ならエンジンに指し手を頼む（同じ局面では1回だけ）
    void requestEngineMove();
    /// 終局をエンジンに知らせて枠を空ける（result が無ければ勝ち負けのない終局）
    void finishEngine(const P2p::FinalResult* result);
    void releaseEngine();
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
    P2pEnginePlayer* m_engine = nullptr;   ///< 自分の手を指すエンジン（枠 E1。エンジンで指すときに作る）
    bool m_engineLaunched = false;          ///< 今の交渉でエンジンを起動したか
    PlayerSetup m_player;                  ///< 今の交渉・対局の自分の対局者

    Phase m_phase = Phase::Idle;
    QElapsedTimer m_elapsed;
    QTimer* m_ticker = nullptr;
    quint64 m_connection = 0;
    QString m_opponentPeerId;
    QString m_opponentDisplayName;
    QString m_myDisplayName;
    quint8 m_role = P2p::kRoleApplicant;
    bool m_viaRelay = false;
    bool m_friendNegotiation = false;   ///< 今の交渉が友だちの申し込みか
    qint64 m_confirmationDeadline = 0;  ///< 条件の確認と開始署名の期限（交渉を始めてから60秒）
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
