#ifndef P2PLOBBYSERVICE_H
#define P2PLOBBYSERVICE_H

/// @file p2plobbyservice.h
/// @brief P2P対局の通信プロセスの窓口（起動・停止・招待・接続の振り分け。実装仕様 3章・10.3節）

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

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
 * 自分や相手のアドレス（IP）は通信プロセスだけが扱い、ここには届かない。
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
    };
    Q_ENUM(StartFailure)

    /// 招待文字列を読んだ結果（通信プロセスの解釈。対局・UI側は文字列を読まない）
    using InviteInfo = PeerIpc::InspectedInvite;

    /// profile はこのアプリが使うプレイヤーの保存先（非所有。この部品より長く生きること）
    P2pLobbyService(const Deps& deps, P2pPlayerProfile* profile, QObject* parent = nullptr);
    ~P2pLobbyService() override;

    P2pPlayerProfile* profile() const { return m_profile.data(); }

    /// 自動化 API を有効にして起動したときは使えなくする（P2P対局の部品を動かさない。実装仕様 10.2節）
    void disableForAutomation() { m_automationDisabled = true; }
    bool isDisabledForAutomation() const { return m_automationDisabled; }

    /// 使用期限を過ぎたビルドか（期限のあるネットワークで使えない）
    static bool isBuildExpired();
    /// 使用期限（UNIX秒。起点と日数は通信プロセスと同じ値を CMake が p2pbuildinfo.h に書く）
    static qint64 buildExpiresAt();

    /// 通信プロセスを起動してネットワークに参加する（保存先を開いていなければ開く。プレイヤー鍵は読み込み済みであること）
    StartFailure begin(const QString& networkName, const QStringList& bootstrap, bool relayOnly);
    /// 招待を無効にし、対局中なら中断を知らせて、通信プロセスを止める（保存先も閉じる）
    void end();

    Status status() const { return m_status; }
    PeerProcess::Failure peerFailure() const { return m_peerFailure; }
    QString networkName() const { return m_networkName; }
    bool isLocalProfile() const { return m_localProfile; }
    QString peerIdText() const { return m_peerIdText; }
    int connectedPeers() const { return m_connectedPeers; }
    QString reachability() const { return m_reachability; }
    /// 協力ノードに取れた中継の予約の数（0 なら、中継経由では相手から届かない）
    int relayReservations() const { return m_relayReservations; }
    bool relayOnly() const { return m_relayOnly; }
    QJsonObject sandbox() const { return m_peer ? m_peer->sandbox() : QJsonObject(); }

    // --- 招待 ---
    void createInvite(const QString& displayName, const P2p::TimeControlTerms& terms, P2p::SidePreference side);
    void cancelInvites();
    void inspectInvite(const QString& invite);
    void connectInvite(const QString& invite, bool allowPrivate, const QString& myDisplayName);

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
    /// 作った招待文字列（記録しない。利用者に渡してもらう）
    void inviteCreated(const QString& invite, quint64 expiresAt, int addressCount);
    void inviteInspected(const P2pLobbyService::InviteInfo& info);
    /// 招待文字列の確認・招待での接続を、通信プロセスが断った
    void inviteRejected(PeerIpc::InviteRejection rejection);
    /// 招待の接続が開けなかった
    void connectFailed(PeerIpc::CloseReason reason);
    /// 通信プロセスが使えなくなった
    void peerFailed(PeerProcess::Failure failure, const QString& detail);

private slots:
    void onPeerReady();
    void onPeerReplied(quint64 requestId, const QString& method, const QJsonObject& result);
    void onPeerRequestFailed(quint64 requestId, const QString& method, const QString& code);
    void onPeerNotified(const QString& name, const QJsonObject& params);
    void onPeerFailed(PeerProcess::Failure failure, const QString& detail);
    void onPeerStopped();
    void onActivityRevoked(QObject* owner);
    void onGameReleased();

private:
    /// 動作状態を「P2P接続中」にする（対局中は司令塔が持つ）
    void holdConnectionActivity();
    void handleConnectionOpened(const PeerIpc::ConnectionOpened& opened);
    void rejectConnection(quint64 connection, P2p::ErrorCode code);

    Deps m_deps;
    QPointer<P2pPlayerProfile> m_profile;     ///< 非所有
    PeerProcess* m_peer = nullptr;            ///< 子オブジェクト（使う間だけ）
    ActivityLease m_lease;
    bool m_automationDisabled = false;

    Status m_status = Status::Off;
    PeerProcess::Failure m_peerFailure = PeerProcess::Failure::None;
    QString m_networkName;
    QStringList m_bootstrap;
    bool m_relayOnly = false;
    bool m_localProfile = true;
    QString m_peerIdText;
    QByteArray m_peerIdBytes;
    QByteArray m_networkId;
    int m_connectedPeers = 0;
    QString m_reachability;
    int m_relayReservations = 0;

    // 招待（招待した側の条件。文字列は持たない）
    QString m_inviteDisplayName;
    P2p::TimeControlTerms m_inviteTerms;
    P2p::SidePreference m_inviteSide = P2p::SidePreference::None;
    bool m_inviteActive = false;
    // 招待で接続を開く要求（要求番号 → 交渉の条件）と、開こうとしている接続
    QHash<quint64, P2pGameCoordinator::NegotiationRequest> m_connectRequests;
    QHash<quint64, P2pGameCoordinator::NegotiationRequest> m_pendingConnections;
};

#endif // P2PLOBBYSERVICE_H
