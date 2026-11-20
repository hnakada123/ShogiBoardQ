#ifndef P2PWINDOW_H
#define P2PWINDOW_H

/// @file p2pwindow.h
/// @brief P2P対局のウィンドウ（接続と条件・招待・対局中・記録・相手・設定。実装仕様 11章）

#include <QDialog>
#include <QPointer>

#include "p2plobbyservice.h"

class EngineSelector;
struct EngineEntry;
class P2pGameCoordinator;
class P2pHistoryPanel;
class P2pPeersPanel;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QMessageBox;
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

    /// 名乗る表示名・持ち時間・先後の希望・自分の対局者（ウィンドウの欄の値）
    QString displayName() const;
    /// 自分の手を人間が指すか、登録エンジンが指すか（「対局者」の欄）
    P2pGameCoordinator::PlayerSetup playerSetup() const;
    P2p::TimeControlTerms terms() const;
    P2p::SidePreference sidePreference() const;
    QString networkName() const;
    QStringList bootstrap() const;
    bool relayOnly() const;
    bool hideUnknownNames() const;

    /// 対局中のタブを出す
    void showGameTab();

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
    /// 受け取った招待文字列・合言葉を確かめる・その招待で接続する（名乗る表示名はウィンドウの欄の値）
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
    void onInviteCreated(const QString& invite, const QString& code, quint64 expiresAt, int addressCount);
    void onCopyInvite();
    void onCopyCode();
    /// 期限の過ぎた招待文字列・合言葉がクリップボードに残っていれば消す
    void clearCopiedInvite();
    void onInspectInvite();
    void onInviteInspected(const P2pLobbyService::InviteInfo& info);
    void onConnectInvite();
    void onInviteRejected(PeerIpc::InviteRejection rejection);
    void onConnectFailed(PeerIpc::CloseReason reason);
    /// 招待で接続したが、相手が招待を受け付けなかった（合言葉・招待文字列が違う・期限切れ・使用済み）
    void onInviteRefused();
    /// 招待での接続の途中の文（「接続しています…」）を消す（交渉が終わった・対局が始まった）
    void clearConnectProgress();
    void onCopyPlayerId();
    void onEngineSettingsRequested(const EngineEntry& engine);
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
    /// 登録エンジンの一覧を読み直す（選んでいたエンジンが削除されていれば「人間」に戻す。別のエンジンに替えない）
    void reloadEngines();
    bool checkDisplayName();
    /// 招待文字列・合言葉をクリップボードに入れる（履歴に残さない印を付け、期限が過ぎたら消す）
    void copyInviteText(const QString& text);
    /// 文が増えて今の高さに収まらなくなったら、収まる高さまでウィンドウを広げる（画面の高さまで）。折り返すラベルの
    /// 高さは幅で決まり、ウィンドウの最小の高さには入らないので、Qt は自動では広げず、足りない分だけ文が欠ける
    void growToFit();

    QPointer<P2pLobbyService> m_lobby;
    QPointer<P2pGameCoordinator> m_game;
    /// この PC の何人目のプレイヤーか（1〜。2人目からは題名と上の案内に出し、表示名と対局者の選択も別に覚える）
    int m_player = 1;
    QByteArray m_engineNameKey;   ///< 選んだエンジンの登録名を保存するキー（プレイヤーごと。EngineSelector に渡す）
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
    QComboBox* m_playerKind = nullptr;            ///< 「対局者」（先頭が「人間」、その後に登録エンジン）
    QPushButton* m_engineSettings = nullptr;
    EngineSelector* m_engineSelector = nullptr;

    // 招待
    QPushButton* m_createInvite = nullptr;
    QWidget* m_codeRow = nullptr;                ///< 合言葉の行（合言葉があるときだけ出す）
    QLabel* m_codeOut = nullptr;
    QPushButton* m_copyCode = nullptr;
    QPlainTextEdit* m_inviteOut = nullptr;
    QPushButton* m_copyInvite = nullptr;
    QLabel* m_inviteOutInfo = nullptr;
    QPlainTextEdit* m_inviteIn = nullptr;
    QPushButton* m_inspectInvite = nullptr;
    QLabel* m_inviteInInfo = nullptr;
    QPointer<QMessageBox> m_connectFailedBox;   ///< 招待の接続に失敗した知らせ（モードレス。閉じたら消える）
    bool m_connectingByInvite = false;           ///< 招待で接続している途中（欄の下に「接続しています…」を出している）
    quint64 m_inviteExpiresAt = 0;               ///< 作った招待の期限（UNIX 秒）
    QString m_copiedInvite;                      ///< コピーした招待文字列・合言葉（期限が過ぎたらクリップボードから消す）
    bool m_inviteInIsCode = false;               ///< 確かめた・接続に使った文字列が合言葉だった（断られた理由の文を選ぶ）
    bool m_inviteInHasAddress = false;           ///< 確かめた合言葉に接続先が入っていた（同じ LAN の合言葉。つながらないときの文を選ぶ）
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
