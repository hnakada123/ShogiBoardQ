#ifndef P2PLOBBYSERVICE_H
#define P2PLOBBYSERVICE_H

/// @file p2plobbyservice.h
/// @brief P2P対局の通信プロセスの窓口（起動・停止・募集・招待・接続の振り分け。実装仕様 3章・10.3節）

#include <QDateTime>
#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>
#include <optional>

#include "appactivitystate.h"
#include "p2pframes.h"
#include "p2pgamecoordinator.h"
#include "p2ptransport.h"
#include "peerprocess.h"

class P2pIdentityStore;
class P2pKnownPeers;
class P2pRecordStore;

/**
 * @brief 「P2P対局」を使う間の部品の持ち主
 *
 * 通信プロセス・保存先（ロック）・プレイヤー鍵・既知の相手を持ち、募集と招待を扱い、開いた接続を司令塔へ振り分ける。
 * 通信プロセスは「P2P対局」を始めてから終えるまでだけ動かす。使う間は動作状態を「P2P接続中」にする
 * （対局の間は司令塔が「P2P対局中」にする）。通信プロセスから届いた募集は形・上限・表示名の規則を確かめ直し、
 * 認証済みの情報として扱わない。
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

    /// 募集1件（通信プロセスが確かめ、ここで形・規則を確かめ直したもの。認証済みではない）
    struct Offer {
        QByteArray offerId;
        QString peerId;
        QString displayName;
        P2p::TimeControlTerms terms;
        P2p::SidePreference side = P2p::SidePreference::None;
        quint64 expiresAt = 0;
    };

    /// 招待文字列を読んだ結果（通信プロセスの解釈。対局・UI側は文字列を読まない）
    struct InviteInfo {
        QString peerId;
        quint64 expiresAt = 0;
        int addressCount = 0;
        int usableAddressCount = 0;
        QStringList privateAddresses;
    };

    /// 実験版のビルドの使用期限（日。実装仕様 3.1節）
    static constexpr qint64 kBuildLifetimeDays = 180;
    /// 再申込みの拒否の時間（実装仕様 3.3節）
    static constexpr qint64 kReapplyBlockSeconds = 600;

    P2pLobbyService(const Deps& deps, const QString& dataRoot, QObject* parent = nullptr);
    ~P2pLobbyService() override;

    P2pRecordStore* records() const { return m_store.get(); }
    P2pKnownPeers* knownPeers() const { return m_knownPeers.get(); }
    P2pIdentityStore* identity() const { return m_identity; }

    /// 自動化 API を有効にして起動したときは使えなくする（P2P対局の部品を動かさない。実装仕様 10.2節）
    void disableForAutomation() { m_automationDisabled = true; }
    bool isDisabledForAutomation() const { return m_automationDisabled; }

    /// 使用期限を過ぎたビルドか（期限のあるネットワークで使えない）
    static bool isBuildExpired();
    /// 使用期限（UNIX秒）
    static qint64 buildExpiresAt();

    /// p2p/ のロックを取り、残ったジャーナルを証跡に直す（使い始める前に1回）
    StartFailure prepare();
    /// 通信プロセスを起動してネットワークに参加する（プレイヤー鍵は読み込み済みであること）
    StartFailure begin(const QString& networkName, const QStringList& bootstrap, bool relayOnly);
    /// 募集を取り下げ、対局中なら中断を知らせて、通信プロセスを止める
    void end();

    Status status() const { return m_status; }
    PeerProcess::Failure peerFailure() const { return m_peerFailure; }
    QString networkName() const { return m_networkName; }
    bool isLocalProfile() const { return m_localProfile; }
    QString peerIdText() const { return m_peerIdText; }
    QStringList listenAddresses() const { return m_listenAddrs; }
    int connectedPeers() const { return m_connectedPeers; }
    QString reachability() const { return m_reachability; }
    bool clockSkewSuspected() const { return m_clockSkew; }
    QJsonObject sandbox() const { return m_peer ? m_peer->sandbox() : QJsonObject(); }

    // --- 募集 ---
    void publishOffer(const QString& displayName, const P2p::TimeControlTerms& terms, P2p::SidePreference side);
    bool hasOffer() const { return !m_offerId.isEmpty(); }
    QList<Offer> offers() const { return m_offers.values(); }
    void refreshOffers();
    /// 募集に申し込む
    bool applyToOffer(const QByteArray& offerId, const QString& myDisplayName, P2p::SidePreference mySide);

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
    void withdrawOffer() override;

signals:
    void statusChanged();
    void networkStatusChanged();
    void offersChanged();
    /// 作った招待文字列（記録しない。利用者に渡してもらう）
    void inviteCreated(const QString& invite, quint64 expiresAt, int addressCount);
    void inviteInspected(const P2pLobbyService::InviteInfo& info);
    /// 要求が失敗した（method と通信プロセスのエラーの code）
    void requestFailed(const QString& method, const QString& code);
    /// 申込み・招待の接続が開けなかった
    void connectFailed(const QString& reason);
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
    void onNegotiationAbandoned(const QString& peerId, const QByteArray& verifiedPlayerKey);
    void onApplicantVerified(const QString& peerId);

private:
    /// 動作状態を「P2P接続中」にする（対局中は司令塔が持つ）
    void holdConnectionActivity();
    void handleConnectionOpened(const QJsonObject& params);
    void handleOffersChanged(const QJsonObject& params);
    void handleNetworkStatus(const QJsonObject& params);
    void rejectConnection(quint64 connection, P2p::ErrorCode code);
    bool isReapplyBlocked(const QString& peerId) const;
    quint64 connectionOf(const QJsonObject& params) const;

    /// 申込み・招待で開こうとしている接続（開いたら司令塔へ渡す）
    struct PendingConnection {
        P2pGameCoordinator::NegotiationRequest request;
    };

    Deps m_deps;
    QString m_dataRoot;
    std::unique_ptr<P2pRecordStore> m_store;
    std::unique_ptr<P2pKnownPeers> m_knownPeers;
    P2pIdentityStore* m_identity = nullptr;   ///< 子オブジェクト
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
    QStringList m_listenAddrs;
    int m_connectedPeers = 0;
    QString m_reachability;
    bool m_clockSkew = false;

    // 募集
    QByteArray m_offerId;
    QString m_offerDisplayName;
    P2p::TimeControlTerms m_offerTerms;
    P2p::SidePreference m_offerSide = P2p::SidePreference::None;
    QHash<QByteArray, Offer> m_offers;
    // 招待（招待した側の条件。文字列は持たない）
    QString m_inviteDisplayName;
    P2p::TimeControlTerms m_inviteTerms;
    P2p::SidePreference m_inviteSide = P2p::SidePreference::None;
    bool m_inviteActive = false;
    // 申込み・招待の接続を開く要求（要求番号 → 交渉の条件）と、開いている接続
    QHash<quint64, P2pGameCoordinator::NegotiationRequest> m_connectRequests;
    QHash<quint64, P2pGameCoordinator::NegotiationRequest> m_pendingConnections;
    /// 再申込みを拒否する相手（PeerId → 期限の UNIX秒）と、拒否の回数
    QHash<QString, qint64> m_reapplyBlockedUntil;
    QHash<QString, int> m_reapplyCount;
};

#endif // P2PLOBBYSERVICE_H
