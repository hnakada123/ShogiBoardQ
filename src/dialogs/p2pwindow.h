#ifndef P2PWINDOW_H
#define P2PWINDOW_H

/// @file p2pwindow.h
/// @brief P2P対局のウィンドウ（接続と条件・招待・対局中・記録・相手・設定。実装仕様 11章）

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

/**
 * @brief P2P対局のウィンドウ（モードレス）
 *
 * 通常の画面では鍵の用語を主な操作に使わず、「招待で接続」のような行動の分かる語にする。対局の相手は招待で決める
 * （不特定の相手との公開募集は持たない）。自分や相手のアドレス（IP）は出さない（例外は、受け取った招待文字列を確かめた
 * ときの私設アドレスの到達候補だけ。同じ LAN の相手につなぐ許可を利用者に求める。protocol/ipc.md 4章）。
 * 相手や通信プロセスから来た文字列（表示名・PeerId）は PlainText で出す。ロビーと司令塔は表示のために読むだけで、
 * 接続・招待・対局の操作は意図のシグナルで知らせる（受けるのは P2pGameUiController。投了と合意中断はメニューと
 * 同じ QAction を通る）。
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
    /// 招待を作る（表示名・持ち時間・先後の希望はウィンドウの欄の値）
    void createInviteRequested();
    /// 受け取った招待文字列を確かめる・その招待で接続する（名乗る表示名はウィンドウの欄の値）
    void inspectInviteRequested(const QString& invite);
    void connectInviteRequested(const QString& invite, bool allowPrivate);
    /// 対局中の操作（投了・合意引き分けの申し出・合意中断の申し出・対局をやめる）
    void resignRequested();
    void offerDrawRequested();
    void offerAbortRequested();
    void leaveGameRequested();

public slots:
    /// 接続・招待・対局の状態を出し直す（通信プロセスの知らせのたびに呼ばれるので、記録の表は作り直さない）
    void refresh();
    /// 対局記録と相手の表を作り直す（記録は証跡を読んで検証するので、対局が終わったとき・タブを開いたときだけ）
    void reloadRecords();

private slots:
    void onConnectClicked();
    void onCreateInvite();
    void onInviteCreated(const QString& invite, quint64 expiresAt, int addressCount);
    void onCopyInvite();
    void onInspectInvite();
    void onInviteInspected(const P2pLobbyService::InviteInfo& info);
    void onConnectInvite();
    void onInviteRejected(PeerIpc::InviteRejection rejection);
    void onConnectFailed(PeerIpc::CloseReason reason);
    void onCopyPlayerId();
    void saveSettings();
    void updateGameStatus();
    void onTabChanged(int index);

private:
    QWidget* createConnectionTab();
    /// 招待がどの経路で届くか（接続中の表示）
    QString routeText() const;
    QWidget* createInviteTab();
    QWidget* createGameTab();
    QWidget* createSettingsTab();
    void loadSettings();
    bool checkDisplayName();
    /// 文が増えて今の高さに収まらなくなったら、収まる高さまでウィンドウを広げる（画面の高さまで）。折り返すラベルの
    /// 高さは幅で決まり、ウィンドウの最小の高さには入らないので、Qt は自動では広げず、足りない分だけ文が欠ける
    void growToFit();

    QPointer<P2pLobbyService> m_lobby;
    QPointer<P2pGameCoordinator> m_game;
    /// この PC の何人目のプレイヤーか（1〜。2人目からは題名と上の案内に出し、表示名も別に覚える）
    int m_player = 1;
    QTabWidget* m_tabs = nullptr;
    QWidget* m_gameTab = nullptr;

    // 接続と条件
    QLabel* m_status = nullptr;
    QComboBox* m_network = nullptr;
    QPushButton* m_connect = nullptr;
    QLabel* m_playerId = nullptr;
    QLabel* m_routeInfo = nullptr;
    QLineEdit* m_displayName = nullptr;
    QSpinBox* m_mainMinutes = nullptr;
    QSpinBox* m_byoyomi = nullptr;
    QComboBox* m_side = nullptr;

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
