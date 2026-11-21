#ifndef P2PLOBBYSERVICE_H
#define P2PLOBBYSERVICE_H

/// @file p2plobbyservice.h
/// @brief P2P対局の通信プロセスの窓口（起動・停止・招待・接続の振り分け。実装仕様 3章・10.3節）

#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTimer>

#include <functional>
#include <memory>
#include <optional>

#include "appactivitystate.h"
#include "p2pframes.h"
#include "p2pgamecoordinator.h"
#include "p2pplayerprofile.h"
#include "p2ptransport.h"
#include "peerprocess.h"

/**
 * @brief 「P2P対局」の通信プロセスの窓口
 *
 * 通信プロセスを動かし、招待を扱い、開いた接続を司令塔へ振り分ける。保存先（対局の記録・既知の相手・プレイヤー鍵）は
 * プレイヤーの保存先（P2pPlayerProfile）のものを使う。
 * 通信プロセスは「P2P対局」を始めてから終えるまでだけ動かす。使う間は動作状態を「P2P接続中」にする
 * （対局の間は司令塔が「P2P対局中」にする）。対局の相手は招待文字列で決める（不特定の相手との公開募集は持たない）。
 * 自分や相手のアドレス（IP）は通信プロセスだけが扱い、ここには届かない（例外は、招待文字列を確かめた結果の私設アドレスの
 * 到達候補 InviteInfo::privateAddresses だけ。protocol/ipc.md 4章）。
 */
class P2pLobbyService : public QObject, public P2pTransport
{
    Q_OBJECT
public:
    struct Deps {
        AppActivityState* activity = nullptr;
        P2pGameCoordinator* game = nullptr;
    };

    enum class Status { Off, Starting, Online, Failed };
    Q_ENUM(Status)

    /// 使い始められない理由
    enum class StartFailure {
        None,
        Locked,              ///< ほかのアプリが p2p/ を使っている
        NetworkNotAllowed,   ///< このビルドが参加できないネットワーク
        BuildExpired,        ///< 使用期限を過ぎたビルド（期限のあるネットワーク）
        NoIdentity,          ///< プレイヤー鍵を読み込んでいない
        AutomationEnabled,   ///< 自動化 API を有効にして起動した
        Busy,                ///< ほかの動作（対局・検討など）の最中
    };
    Q_ENUM(StartFailure)

    /// 招待文字列を読んだ結果（通信プロセスの解釈。対局・UI側は文字列を読まない）
    using InviteInfo = PeerIpc::InspectedInvite;

    /// 招待・友だちへの申し込みでつなぐ試み（つながらなかったときの理由の文は、試みで変わる）
    enum class ConnectAttempt {
        InviteString,  ///< 招待文字列
        LanCode,       ///< 同じ LAN の合言葉（接続先が入っている）
        NodeCode,      ///< 協力ノードを使う合言葉
        Friend,        ///< 友だちの在席の知らせ（友だちへの申し込み）
    };
    Q_ENUM(ConnectAttempt)

    /// つなぐ試みが、交渉を始める前に終わった
    struct ConnectFailure {
        enum class Stage {
            Rejected,   ///< 通信プロセスが要求を断った（読めない・期限切れ・自分の招待など。rejection）
            NotOpened,  ///< 接続が開かなかった（reason）
            Refused,    ///< 開いたが、相手が何も返さずに閉じた（招待のトークンが合わない・友だちが今は受けられない）
        };
        ConnectAttempt attempt = ConnectAttempt::InviteString;
        Stage stage = Stage::Rejected;
        PeerIpc::InviteRejection rejection = PeerIpc::InviteRejection::Unreadable;
        PeerIpc::CloseReason reason = PeerIpc::CloseReason::Closed;
    };

    /// profile はこのアプリが使うプレイヤーの保存先（非所有。この部品より長く生きること）
    P2pLobbyService(const Deps& deps, P2pPlayerProfile* profile, QObject* parent = nullptr);
    ~P2pLobbyService() override;

    P2pPlayerProfile* profile() const { return m_profile.data(); }

    /// 自動化 API を有効にして起動したときは使えなくする（P2P対局の部品を動かさない。実装仕様 10.2節）
    void disableForAutomation() { m_automationDisabled = true; }

    /// 通信プロセスを起動してネットワークに参加する（保存先を開いていなければ開く。プレイヤー鍵は読み込み済みであること）
    StartFailure begin(const QString& networkName, const QStringList& bootstrap, bool relayOnly);
    /// 招待を無効にし、対局中なら中断を知らせて、通信プロセスを止める（保存先も閉じる）
    void end();
    /// アプリの終了のときだけ使う。end() の後、通信プロセスが中断の知らせと shutdown を受けて終わるのを msecs まで待つ
    void waitForExit(int msecs);
    /// 接続を始められるか（ほかの動作（対局・検討など）の最中でない）。変わると statusChanged を出す
    bool canBegin() const;

    Status status() const { return m_status; }
    bool isLocalProfile() const { return m_localProfile; }
    int connectedPeers() const { return m_connectedPeers; }
    PeerIpc::Reachability reachability() const { return m_reachability; }
    /// 協力ノードに取れた中継の予約の数（0 なら、中継経由では相手から届かない）
    int relayReservations() const { return m_relayReservations; }
    /// つながっている、合言葉の待ち合わせに対応した協力ノードの数（0 なら、協力ノードを使う合言葉と友だち一覧は使えない）
    int meetNodes() const { return m_meetNodes; }
    bool relayOnly() const { return m_relayOnly; }
    /// 通信プロセスに掛かっている権限の制限（動かしていない間は、何も掛かっていない値）
    PeerIpc::Sandbox sandbox() const { return m_peer ? m_peer->sandbox() : PeerIpc::Sandbox(); }

    // --- 招待（player は自分の対局者。人間が指すか、登録エンジンが指すか） ---
    void createInvite(const QString& displayName, const P2p::TimeControlTerms& terms, P2p::SidePreference side,
                      const P2pGameCoordinator::PlayerSetup& player = {});
    void inspectInvite(const QString& invite);
    /// 招待文字列・合言葉で接続する（attempt は、確かめた結果の種類。つながらなかったときの知らせに付けて返す）
    void connectInvite(const QString& invite, ConnectAttempt attempt, bool allowPrivate, const QString& myDisplayName,
                       const P2pGameCoordinator::PlayerSetup& player = {});

    // --- 友だち一覧（在席の知らせ。中身の暗号化と復号は P2pFriendPresence が行い、ここは通信プロセスへ運ぶだけ） ---
    /// 在席の知らせを作らせる（応答は presenceCreated）。送れなければ 0
    quint64 createPresence();
    /// 作らせた在席の知らせをすべて無効にする
    void cancelPresence();
    /// 番号と暗号化したものを、協力ノードに預ける（応答は meetStored）・引き取る（応答は meetFetched）。送れなければ 0
    quint64 meetPut(const QByteArray& slot, const QByteArray& blob, int ttlSeconds);
    quint64 meetGet(const QByteArray& slot);
    /// 友だちの在席の知らせ（復号したもの）の相手に、対局を申し込む。request は自分の表示名・対局者と、
    /// 相手が名乗るべきプレイヤーID（friendPlayerKey）。始められなければ false
    bool connectPresence(const QByteArray& presence, const P2pGameCoordinator::NegotiationRequest& request);
    /// 友だちの申し込み（在席の知らせのトークンで開いた接続）を受けるかを決める部品。知らせの番号から、交渉の条件
    /// （相手が名乗るべきプレイヤーID・自分の表示名・持ち時間・先後・対局者）を返す。受けなければ nullopt
    using FriendHandler = std::function<std::optional<P2pGameCoordinator::NegotiationRequest>(quint64 presenceId)>;
    void setFriendHandler(FriendHandler handler) { m_friendHandler = std::move(handler); }

    // --- P2pTransport ---
    QByteArray localPeerId() const override { return m_peerIdBytes; }
    QByteArray networkId() const override { return m_networkId; }
    bool sendFrame(quint64 connection, const QByteArray& frame) override;
    void closeConnection(quint64 connection) override;
    void setGamePeer(const QString& peerId) override;
    void reconnectGamePeer(const QString& peerId) override;

signals:
    void statusChanged();
    void networkStatusChanged();
    /// 作った招待文字列と合言葉（記録しない。利用者に渡してもらう）。合言葉は、同じ LAN の相手と直接つなぐ
    /// ときだけ付く（付かなければ空）
    void inviteCreated(const QString& invite, const QString& code, quint64 expiresAt, int addressCount);
    void inviteInspected(const P2pLobbyService::InviteInfo& info);
    /// 招待文字列・合言葉の確認（inspectInvite）を、通信プロセスが断った
    void inspectRejected(PeerIpc::InviteRejection rejection);
    /// 招待・友だちへの申し込みでつなぐ試みが、交渉を始める前に終わった（断られた・開かなかった・相手が閉じた）
    void connectAttemptFailed(const P2pLobbyService::ConnectFailure& failure);
    /// 通信プロセスが使えなくなった
    void peerFailed(PeerProcess::Failure failure, const QString& detail);
    /// 友だち一覧の要求の応答（requestId は createPresence・meetPut・meetGet が返した番号）
    void presenceCreated(quint64 requestId, const PeerIpc::CreatedPresence& presence);
    void meetStored(quint64 requestId, int stored);
    void meetFetched(quint64 requestId, const QList<QByteArray>& blobs);
    /// 友だち一覧の要求（createPresence・meetPut・meetGet）を、通信プロセスが断った
    void presenceRequestFailed(quint64 requestId);

private slots:
    void onPeerReady();
    void onPeerReplied(quint64 requestId, const PeerIpc::Reply& reply);
    void onPeerRequestFailed(quint64 requestId, PeerIpc::Method method, const QString& errorCode);
    void onPeerNotified(const PeerIpc::Notification& notification);
    void onPeerFailed(PeerProcess::Failure failure, const QString& detail);
    void onPeerStopped();
    void onActivityRevoked(QObject* owner);
    void onGameReleased();
    /// 自分がつないだ接続で始めた交渉で、相手が何も返さずに閉じた
    void onInviteRefused();

private:
    /// 接続を始めた・開こうとしている、つなぐ試み
    struct PendingConnect {
        P2pGameCoordinator::NegotiationRequest request;
        ConnectAttempt attempt = ConnectAttempt::InviteString;
    };

    /// 動作状態を「P2P接続中」にする（対局中は司令塔が持つ）
    void holdConnectionActivity();
    // 応答（種類ごと。足りない種類があればコンパイルできない）
    void handleReply(quint64 requestId, const PeerIpc::Hello& hello);
    void handleReply(quint64 requestId, const PeerIpc::StartResult& started);
    void handleReply(quint64 requestId, const PeerIpc::CreatedInvite& created);
    void handleReply(quint64 requestId, const PeerIpc::InspectedInvite& inspected);
    void handleReply(quint64 requestId, const PeerIpc::ConnectionStarted& connecting);
    void handleReply(quint64 requestId, const PeerIpc::Done& done);
    void handleReply(quint64 requestId, const PeerIpc::CreatedPresence& presence);
    void handleReply(quint64 requestId, const PeerIpc::MeetStored& stored);
    void handleReply(quint64 requestId, const PeerIpc::MeetFetched& fetched);
    // 通知（種類ごと）
    void handleNotification(const PeerIpc::NetworkStatus& status);
    void handleNotification(const PeerIpc::ConnectionOpened& opened);
    void handleNotification(const PeerIpc::ConnectionRoute& route);
    void handleNotification(const PeerIpc::FrameReceived& received);
    void handleNotification(const PeerIpc::ConnectionClosed& closed);
    void rejectConnection(quint64 connection, P2p::ErrorCode code);
    /// 自分から接続を始める（友だちへの申し込みと招待で共通。送れなければ false）
    bool startConnect(const PeerIpc::Request& ipcRequest, const PendingConnect& pending);
    void failAttempt(ConnectAttempt attempt, ConnectFailure::Stage stage, PeerIpc::InviteRejection rejection,
                     PeerIpc::CloseReason reason);

    Deps m_deps;
    QPointer<P2pPlayerProfile> m_profile;     ///< 非所有
    PeerProcess* m_peer = nullptr;            ///< 子オブジェクト（使う間だけ）
    /// 接続の状態の知らせ（networkStatusChanged）をまとめて出す。通信プロセスが知らせを続けて送っても、画面を
    /// そのたびに作り直さない
    QTimer* m_networkStatusTimer = nullptr;
    ActivityLease m_lease;
    bool m_automationDisabled = false;

    Status m_status = Status::Off;
    QString m_networkName;
    QStringList m_bootstrap;
    bool m_relayOnly = false;
    bool m_localProfile = true;
    QByteArray m_peerIdBytes;
    QByteArray m_networkId;
    int m_connectedPeers = 0;
    PeerIpc::Reachability m_reachability = PeerIpc::Reachability::Unknown;
    int m_relayReservations = 0;
    int m_meetNodes = 0;
    FriendHandler m_friendHandler;

    // 招待（招待した側の条件。文字列は持たない）
    QString m_inviteDisplayName;
    P2p::TimeControlTerms m_inviteTerms;
    P2p::SidePreference m_inviteSide = P2p::SidePreference::None;
    P2pGameCoordinator::PlayerSetup m_invitePlayer;
    bool m_inviteActive = false;
    // 自分から接続を開く要求（要求番号 → 試み）と、開こうとしている接続（接続の番号 → 試み）
    QHash<quint64, PendingConnect> m_connectRequests;
    QHash<quint64, PendingConnect> m_pendingConnections;
    /// 自分がつないだ接続で、司令塔が交渉している試み（相手が何も返さずに閉じたときに、どの試みかを知らせる）
    std::optional<ConnectAttempt> m_negotiatingAttempt;
};

#endif // P2PLOBBYSERVICE_H
