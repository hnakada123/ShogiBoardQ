#ifndef P2PWINDOW_H
#define P2PWINDOW_H

/// @file p2pwindow.h
/// @brief P2P対局のウィンドウ（接続・招待・対局中・記録・相手・設定。実装仕様 11章）

#include <QDialog>
#include <QPointer>

#include "p2plobbyservice.h"

class P2pConnectionPanel;
class P2pGameCoordinator;
class P2pGamePanel;
class P2pHistoryPanel;
class P2pFriendPresence;
class P2pInvitePanel;
class P2pPeersPanel;
class P2pSettingsPanel;
class P2pStatusStrip;
class QMessageBox;
class QTabWidget;

/**
 * @brief P2P対局のウィンドウ（モードレス）
 *
 * タブの上に接続の状態の帯（状態・招待の経路・「接続」「切断」）を置き、タブは使う順に並べる: 「接続」（相手のいる
 * 場所・相手とのつなぎ方・協力ノード。P2pConnectionPanel）→「招待」（名乗る名前と対局の条件・招待を作る・招待を
 * 受ける。P2pInvitePanel）→「対局中」（P2pGamePanel）。続けて「対局記録」「相手」「設定」。
 * 招待・友だちへの申し込みでつながらなかった知らせ（P2pLobbyService::connectAttemptFailed）は、ここで1つに受け、
 * 招待の欄の下か、知らせの箱に出す。
 *
 * 通常の画面では鍵の用語を主な操作に使わず、「招待を受ける」のような行動の分かる語にする。対局の相手は招待で決める
 * （不特定の相手との公開募集は持たない）。自分や相手のアドレス（IP）は出さない（例外は、受け取った招待文字列を確かめた
 * ときの私設アドレスの到達候補と、利用者が入れた協力ノードのアドレスだけ。protocol/ipc.md 4章）。
 * 相手や通信プロセスから来た文字列（表示名・PeerId）は PlainText で出す。ロビーと司令塔は表示のために読むだけで、
 * 接続・招待・対局の操作は意図のシグナルで知らせる（受けるのは P2pGameUiController。投了と合意中断はメニューと
 * 同じ QAction を通る）。
 */
class P2pWindow : public QDialog
{
    Q_OBJECT
public:
    P2pWindow(P2pLobbyService* lobby, P2pGameCoordinator* game, QWidget* parent = nullptr);

    /// 名乗る表示名・持ち時間・先後の希望・自分の対局者（「招待」タブの欄の値）
    QString displayName() const;
    /// 自分の手を人間が指すか、登録エンジンが指すか（「対局者」の欄）
    P2pGameCoordinator::PlayerSetup playerSetup() const;
    P2p::TimeControlTerms terms() const;
    P2p::SidePreference sidePreference() const;
    /// 相手とのつなぎ方（「接続」タブの値）
    QString networkName() const;
    QStringList bootstrap() const;
    bool relayOnly() const;
    bool hideUnknownNames() const;

    /// 対局中のタブを出す
    void showGameTab();
    /// 友だちの在席（非所有）。「友だちに在席を知らせる」の設定を渡し、「相手」タブに在席を出す
    void setFriendPresence(P2pFriendPresence* presence);
    /// 表示名が使えるか確かめる（使えなければ「招待」タブを出して知らせ、false）
    bool confirmDisplayName();

    /// 本体と前後を入れ替えられる、独立したウィンドウとして出す（対局中も開いたまま、本体の盤と行き来して使う）
    void setVisible(bool visible) override;

signals:
    /// 接続（通信プロセスの起動）・切断
    void startRequested();
    void stopRequested();
    /// プレイヤー鍵を作り直す
    void recreateIdentityRequested();
    /// 招待を作る（表示名・持ち時間・先後の希望はウィンドウの欄の値）
    void createInviteRequested();
    /// 受け取った招待文字列・合言葉を確かめる・その招待で接続する（名乗る表示名はウィンドウの欄の値。attempt は、
    /// 確かめた結果の種類）
    void inspectInviteRequested(const QString& invite);
    void connectInviteRequested(const QString& invite, P2pLobbyService::ConnectAttempt attempt, bool allowPrivate);
    /// 友だち（プレイヤーID）に対局を申し込む
    void applyToFriendRequested(const QByteArray& playerKey);
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
    /// 「接続」タブの設定を変えた（保存して、状態の帯と招待の説明を合わせる）
    void onConnectionSettingsChanged();
    void showConnectionTab();
    /// 招待・友だちへの申し込みでつながらなかった（招待の欄の下か、知らせの箱に出す）
    void onConnectAttemptFailed(const P2pLobbyService::ConnectFailure& failure);
    /// 「相手」タブの「友だちに在席を知らせる」を切り替えた（プレイヤーごとに保存し、在席の部品に伝える）
    void onSharingToggled(bool sharing);
    void onCopyPlayerId();
    void saveSettings();
    void onTabChanged(int index);
    /// 文が増えて今の高さに収まらなくなったら、収まる高さまでウィンドウを広げる（画面の高さまで）。折り返すラベルの
    /// 高さは幅で決まり、ウィンドウの最小の高さには入らないので、Qt は自動では広げず、足りない分だけ文が欠ける
    void growToFit();

private:
    /// ロビー（通信プロセスの窓口）と司令塔の知らせを、表示の出し直しにつなぐ
    void connectSources();
    /// 招待がどの経路で届くか（接続中の表示）
    QString routeText() const;
    void loadSettings();

    QPointer<P2pLobbyService> m_lobby;
    QPointer<P2pGameCoordinator> m_game;
    /// この PC の何人目のプレイヤーか（1〜。2人目からは題名と上の案内に出し、表示名と対局者の選択も別に覚える）
    int m_player = 1;
    P2pStatusStrip* m_strip = nullptr;
    QTabWidget* m_tabs = nullptr;
    QWidget* m_gameTab = nullptr;

    // 接続・招待・対局中・設定
    P2pConnectionPanel* m_connection = nullptr;
    P2pInvitePanel* m_invite = nullptr;
    P2pGamePanel* m_gamePanel = nullptr;
    P2pSettingsPanel* m_settings = nullptr;
    QPointer<QMessageBox> m_connectFailedBox;   ///< つながらなかった知らせ（モードレス。閉じたら消える）

    // 記録・相手
    P2pHistoryPanel* m_history = nullptr;
    P2pPeersPanel* m_peers = nullptr;
    QPointer<P2pFriendPresence> m_presence;      ///< 非所有
};

#endif // P2PWINDOW_H
