#ifndef P2PWINDOW_H
#define P2PWINDOW_H

/// @file p2pwindow.h
/// @brief P2P対局のウィンドウ（対戦相手を探す・招待・対局中・記録・相手・設定。実装仕様 11章）

#include <QDialog>
#include <QPointer>

#include "p2plobbyservice.h"

class P2pGameCoordinator;
class P2pHistoryPanel;
class P2pPeersPanel;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QTabWidget;
class QTableWidget;

/**
 * @brief P2P対局のウィンドウ（モードレス）
 *
 * 通常の画面では DHT や鍵の用語を主な操作に使わず、「対戦相手を探す」「招待で接続」のような行動の分かる語にする。
 * 相手や通信プロセスから来た文字列（表示名・PeerId）は PlainText で出す。接続と切断はプレイヤー鍵の準備が要るので、
 * 意図のシグナルで知らせる（受けるのは P2pGameUiController）。
 */
class P2pWindow : public QDialog
{
    Q_OBJECT
public:
    P2pWindow(P2pLobbyService* lobby, P2pGameCoordinator* game, QWidget* parent = nullptr);

    /// 名乗る表示名・持ち時間・先後の希望（ウィンドウの欄の値）
    QString displayName() const;
    P2p::TimeControlTerms terms() const;
    P2p::SidePreference sidePreference() const;
    QString networkName() const;
    QStringList bootstrap() const;
    bool relayOnly() const;
    bool hideUnknownNames() const;

    /// 対局中のタブを出す
    void showGameTab();

signals:
    /// 接続（通信プロセスの起動）・切断
    void startRequested();
    void stopRequested();
    /// プレイヤー鍵を作り直す
    void recreateIdentityRequested();

public slots:
    /// 接続・募集・対局の状態を出し直す（通信プロセスの知らせのたびに呼ばれるので、記録の表は作り直さない）
    void refresh();
    /// 対局記録と相手の表を作り直す（記録は証跡を読んで検証するので、対局が終わったとき・タブを開いたときだけ）
    void reloadRecords();

private slots:
    void onConnectClicked();
    void onPublishClicked();
    void onApplyClicked();
    void onRefreshOffers();
    void onOffersChanged();
    void onCreateInvite();
    void onInviteCreated(const QString& invite, quint64 expiresAt, int addressCount);
    void onCopyInvite();
    void onInspectInvite();
    void onInviteInspected(const P2pLobbyService::InviteInfo& info);
    void onConnectInvite();
    void onRequestFailed(const QString& method, const QString& code);
    void onConnectFailed(const QString& reason);
    void onCopyPlayerId();
    void onResign();
    void onOfferDraw();
    void onOfferAbort();
    void onLeaveGame();
    void saveSettings();
    void updateGameStatus();
    void onTabChanged(int index);

private:
    QWidget* createPlayTab();
    QWidget* createInviteTab();
    QWidget* createGameTab();
    QWidget* createSettingsTab();
    void loadSettings();
    bool checkDisplayName();

    QPointer<P2pLobbyService> m_lobby;
    QPointer<P2pGameCoordinator> m_game;
    QTabWidget* m_tabs = nullptr;
    QWidget* m_gameTab = nullptr;

    // 対戦相手を探す
    QLabel* m_status = nullptr;
    QComboBox* m_network = nullptr;
    QPushButton* m_connect = nullptr;
    QLabel* m_playerId = nullptr;
    QLabel* m_networkInfo = nullptr;
    QLineEdit* m_displayName = nullptr;
    QSpinBox* m_mainMinutes = nullptr;
    QSpinBox* m_byoyomi = nullptr;
    QComboBox* m_side = nullptr;
    QTableWidget* m_offers = nullptr;
    QPushButton* m_publish = nullptr;
    QPushButton* m_apply = nullptr;
    QPushButton* m_refreshOffers = nullptr;

    // 招待
    QPushButton* m_createInvite = nullptr;
    QPlainTextEdit* m_inviteOut = nullptr;
    QPushButton* m_copyInvite = nullptr;
    QLabel* m_inviteOutInfo = nullptr;
    QPlainTextEdit* m_inviteIn = nullptr;
    QPushButton* m_inspectInvite = nullptr;
    QLabel* m_inviteInInfo = nullptr;
    QCheckBox* m_allowPrivate = nullptr;
    QPushButton* m_connectInvite = nullptr;

    // 対局中
    QLabel* m_gameStatus = nullptr;
    QPushButton* m_resign = nullptr;
    QPushButton* m_offerDraw = nullptr;
    QPushButton* m_offerAbort = nullptr;
    QPushButton* m_leave = nullptr;

    // 記録・相手
    P2pHistoryPanel* m_history = nullptr;
    P2pPeersPanel* m_peers = nullptr;

    // 設定
    QPlainTextEdit* m_bootstrap = nullptr;
    QCheckBox* m_relayOnly = nullptr;
    QCheckBox* m_hideUnknown = nullptr;
    QLabel* m_identityInfo = nullptr;
    QLabel* m_buildInfo = nullptr;
};

#endif // P2PWINDOW_H
