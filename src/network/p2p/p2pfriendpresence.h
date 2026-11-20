#ifndef P2PFRIENDPRESENCE_H
#define P2PFRIENDPRESENCE_H

/// @file p2pfriendpresence.h
/// @brief 友だち一覧の在席（預ける・調べる・友だちの申し込みを受ける。protocol/README.md 3.3節・実装仕様 3.3.3節）

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QString>

#include <functional>
#include <optional>

#include "p2pfriend.h"
#include "p2pgamecoordinator.h"
#include "peeripc.h"

class P2pLobbyService;
class QTimer;

/**
 * @brief 友だちの在席
 *
 * 友だち（既知の相手のうち、友だちにした相手）ごとに、自分のプレイヤー鍵と相手のプレイヤーID から2人だけの秘密を
 * 作る。「友だちに在席を知らせる」が有効で接続している間、通信プロセスが作った在席の知らせ（接続先と申し込み用の
 * トークン）を、友だちごとの鍵で暗号化して協力ノードに預ける。友だちの一覧を表示している間は、友だちが預けた知らせを
 * 引き取って復号し、読めれば「オンライン」とする。友だちの側も自分を友だちにしていなければ、どちらも起きない。
 *
 * 知らせの中身（接続先）は読まない（通信プロセスが作ったバイト列を、暗号化して運ぶだけ）。協力ノードには、番号の
 * 意味も中身も分からない（運営者には、どの接続元どうしが同じ番号を預けて引き取ったかは見える）。
 */
class P2pFriendPresence : public QObject
{
    Q_OBJECT
public:
    /// 申し込みを受けるときの自分の条件（「接続と条件」の欄の値）
    struct HostSetup {
        QString displayName;
        P2p::TimeControlTerms terms;
        P2p::SidePreference side = P2p::SidePreference::None;
        P2pGameCoordinator::PlayerSetup player;
    };
    /// 今の条件を返す部品（表示名が使えないなど、申し込みを受けられなければ nullopt）
    using HostSetupProvider = std::function<std::optional<HostSetup>()>;

    /// 申し込みを始めた結果
    enum class ApplyResult {
        Started,       ///< 相手へ接続を始めた（つながれば、双方に条件確認画面が開く）
        NotOnline,     ///< 相手の在席の知らせが無い（オンラインでない・期限が過ぎた）
        Busy,          ///< 自分が交渉中・対局中
        Unavailable,   ///< 接続していない・合言葉に対応した協力ノードが無い
    };
    Q_ENUM(ApplyResult)

    /// 預け直す・調べ直す間隔と、預けておく時間（秒）
    static constexpr int kDepositIntervalSeconds = 75;
    static constexpr int kWatchIntervalSeconds = 60;
    static constexpr int kDepositTtlSeconds = 180;

    /// lobby・game は非所有（この部品より長く生きること）
    P2pFriendPresence(P2pLobbyService* lobby, P2pGameCoordinator* game, QObject* parent = nullptr);
    ~P2pFriendPresence() override;

    void setHostSetupProvider(HostSetupProvider provider) { m_hostSetup = std::move(provider); }

    /// 「友だちに在席を知らせる」（無効にすると、作った知らせを無効にする）
    void setSharing(bool sharing);
    bool isSharing() const { return m_sharing; }
    /// 友だちの在席を調べる（友だちの一覧を表示している間だけ）
    void setWatching(bool watching);
    /// 友だちの在席を、今すぐ調べ直す（一覧を表示し直したとき。調べていない間は何もしない）
    void refreshNow() { startWatchRound(); }

    /// 使えるか（接続していて、合言葉の待ち合わせに対応した協力ノードにつながっている）
    bool isAvailable() const;
    /// 友だちが在席を知らせているか（最後に調べたときに、期限内の知らせを読めた）
    bool isOnline(const QByteArray& playerKey) const;

    /// 友だちの記録が変わった（追加・外した・ブロック・削除）。外した友だちへの知らせを無効にして、預け直す
    void reloadFriends();
    /// 友だちに対局を申し込む（myDisplayName・player は「接続と条件」の値）
    ApplyResult apply(const QByteArray& playerKey, const QString& myDisplayName,
                      const P2pGameCoordinator::PlayerSetup& player);

signals:
    /// 友だちの在席・使えるかどうかが変わった
    void presenceChanged();

private slots:
    void onLobbyChanged();
    void startDepositRound();
    void startWatchRound();
    void step();
    void expire();
    void onPresenceCreated(quint64 requestId, const PeerIpc::CreatedPresence& presence);
    void onMeetStored(quint64 requestId, int stored);
    void onMeetFetched(quint64 requestId, const QList<QByteArray>& blobs);
    void onRequestFailed(quint64 requestId);

private:
    struct Online {
        P2p::Friend::Presence presence;
    };
    struct PendingGet {
        QByteArray playerKey;
        QList<quint64> epochs;   ///< これから試す期間（先頭が、今尋ねている期間）
    };
    struct Created {
        QByteArray playerKey;
        quint64 expiresAt = 0;
    };

    /// 友だちとの2人だけの秘密（作れなければ空）
    QByteArray pairSecretFor(const QByteArray& playerKey);
    QByteArray myPlayerKey() const;
    bool canDeposit() const;
    bool canWatch() const;
    void fetch(const PendingGet& get);
    void setOnline(const QByteArray& playerKey, const std::optional<P2p::Friend::Presence>& presence);
    void clearState();
    /// 友だちの申し込み（在席の知らせの番号）を受けるときの交渉の条件。受けなければ nullopt
    std::optional<P2pGameCoordinator::NegotiationRequest> acceptApplication(quint64 presenceId);

    QPointer<P2pLobbyService> m_lobby;
    QPointer<P2pGameCoordinator> m_game;
    HostSetupProvider m_hostSetup;
    QTimer* m_depositTimer = nullptr;
    QTimer* m_watchTimer = nullptr;
    QTimer* m_stepTimer = nullptr;
    QTimer* m_expireTimer = nullptr;
    QTimer* m_retryTimer = nullptr;   ///< 接続先がまだ無かった知らせを、少し後で作り直す
    bool m_sharing = false;
    bool m_watching = false;
    bool m_wasAvailable = false;

    QHash<QByteArray, QByteArray> m_pairSecrets;      ///< プレイヤーID → 2人だけの秘密（メモリにだけ持つ）
    QList<QByteArray> m_depositQueue;                  ///< この回に知らせを預ける友だち
    QList<QByteArray> m_watchQueue;                    ///< この回に在席を調べる友だち
    QHash<quint64, QByteArray> m_pendingCreates;       ///< createPresence の要求 → 友だち
    QHash<quint64, PendingGet> m_pendingGets;          ///< meetGet の要求 → 友だちと期間
    QList<quint64> m_pendingPuts;                      ///< 結果を待っている meetPut の要求
    QHash<quint64, Created> m_created;                 ///< 在席の知らせの番号 → 預けた友だちと期限
    QHash<QByteArray, Online> m_online;                ///< 在席を知らせている友だち
};

#endif // P2PFRIENDPRESENCE_H
