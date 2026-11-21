#ifndef P2PINVITEPANEL_H
#define P2PINVITEPANEL_H

/// @file p2pinvitepanel.h
/// @brief P2P対局のウィンドウの「招待」タブ（名乗る名前と対局の条件・招待する・招待を受ける）

#include <QByteArray>
#include <QPointer>
#include <QWidget>

#include <optional>

#include "p2plobbyservice.h"

class EngineSelector;
struct EngineEntry;
class P2pGameCoordinator;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;

/**
 * @brief 「招待」タブ
 *
 * 上に、名乗る名前と対局の条件（表示名・対局者・持ち時間・先後）。その下に「招待する」（招待を作る・合言葉と招待文字列を
 * 渡す）と「招待を受ける」（合言葉・招待文字列を確かめて接続する）を左右に並べる。ロビーと司令塔は表示のために読むだけで、
 * 招待の操作は意図のシグナルで知らせる（受けるのは P2pGameUiController）。
 */
class P2pInvitePanel : public QWidget
{
    Q_OBJECT
public:
    /// player はこの PC の何人目のプレイヤーか（表示名と対局者の選択を、プレイヤーごとに覚える）
    P2pInvitePanel(P2pLobbyService* lobby, P2pGameCoordinator* game, int player, QWidget* parent = nullptr);

    /// 名乗る表示名・持ち時間・先後の希望・自分の対局者（人間か、登録エンジンか）
    QString displayName() const;
    P2pGameCoordinator::PlayerSetup playerSetup() const;
    P2p::TimeControlTerms terms() const;
    P2p::SidePreference sidePreference() const;

    /// 表示名が使えるか
    bool hasValidDisplayName() const;
    /// 表示名が使えるか確かめる（使えなければ知らせて、表示名の欄に入力を移して false）
    bool checkDisplayName();

    void loadSettings();
    void saveSettings();
    /// 登録エンジンの一覧を読み直す（選んでいたエンジンが削除されていれば「人間」に戻す。別のエンジンに替えない）
    void reloadEngines();
    /// 今の設定で作る招待に何が入るか（「接続」タブの設定から決まる文。注意の欄に出す）
    void setInviteContentsText(const QString& text);
    /// 接続・交渉の状態に合わせて、押せる操作を出し直す
    void refresh();
    /// 招待の欄で確かめた・使った文字列で、つなげなかった理由を、欄の下に出す
    void showConnectResult(const QString& text);

public slots:
    /// 招待での接続の途中の文（「接続しています…」）を消す（交渉が終わった・つなげなかった）
    void clearConnectProgress();

signals:
    /// 招待を作る（表示名・持ち時間・先後の希望・対局者はこのタブの欄の値）
    void createInviteRequested();
    /// 受け取った招待文字列・合言葉を確かめる・その招待で接続する（attempt は、確かめた結果の種類）
    void inspectInviteRequested(const QString& invite);
    void connectInviteRequested(const QString& invite, P2pLobbyService::ConnectAttempt attempt, bool allowPrivate);
    /// 文が替わって、要る高さが変わったかもしれない（ウィンドウが収まる高さまで広げる）
    void contentResized();

private slots:
    void onCreateInvite();
    void onInviteCreated(const QString& invite, const QString& code, quint64 expiresAt, int addressCount);
    void onCopyInvite();
    void onCopyCode();
    /// 期限の過ぎた招待文字列・合言葉がクリップボードに残っていれば消す
    void clearCopiedInvite();
    void onInspectInvite();
    void onInviteInspected(const P2pLobbyService::InviteInfo& info);
    void onInspectRejected(PeerIpc::InviteRejection rejection);
    void onConnectInvite();
    void onEngineSettingsRequested(const EngineEntry& engine);

private:
    QWidget* createConditionBox();
    QWidget* createInviteOutBox();
    QWidget* createInviteInBox();
    /// 招待文字列・合言葉をクリップボードに入れる（履歴に残さない印を付け、期限が過ぎたら消す）
    void copyInviteText(const QString& text);
    /// 「招待を受ける」の欄の下の文
    void setInviteInText(const QString& text);

    QPointer<P2pLobbyService> m_lobby;
    QPointer<P2pGameCoordinator> m_game;
    int m_player = 1;
    QByteArray m_engineNameKey;   ///< 選んだエンジンの登録名を保存するキー（プレイヤーごと。EngineSelector に渡す）

    // 名乗る名前と対局の条件
    QLineEdit* m_displayName = nullptr;
    QComboBox* m_playerKind = nullptr;            ///< 「対局者」（先頭が「人間」、その後に登録エンジン）
    QPushButton* m_engineSettings = nullptr;
    EngineSelector* m_engineSelector = nullptr;
    QSpinBox* m_mainMinutes = nullptr;
    QSpinBox* m_byoyomi = nullptr;
    QComboBox* m_side = nullptr;

    // 招待する
    QPushButton* m_createInvite = nullptr;
    QWidget* m_codeRow = nullptr;                ///< 合言葉の行（合言葉があるときだけ出す）
    QLabel* m_codeOut = nullptr;
    QPushButton* m_copyCode = nullptr;
    QLabel* m_noCode = nullptr;                  ///< 合言葉の付かない招待の案内（招待文字列を渡してもらう）
    QPlainTextEdit* m_inviteOut = nullptr;
    QPushButton* m_copyInvite = nullptr;
    QLabel* m_inviteOutInfo = nullptr;
    QLabel* m_inviteNotes = nullptr;
    quint64 m_inviteExpiresAt = 0;               ///< 作った招待の期限（UNIX 秒）
    QString m_copiedInvite;                      ///< コピーした招待文字列・合言葉（期限が過ぎたらクリップボードから消す）

    // 招待を受ける
    QPlainTextEdit* m_inviteIn = nullptr;
    QPushButton* m_inspectInvite = nullptr;
    QLabel* m_inviteInInfo = nullptr;
    QCheckBox* m_allowPrivate = nullptr;
    QPushButton* m_connectInvite = nullptr;
    bool m_inspectingCode = false;                     ///< 確かめている文字列が合言葉（断られた理由の文を選ぶ）
    std::optional<PeerIpc::InviteKind> m_inspectedKind;   ///< 確かめた結果の種類（接続するときの試みの種類）
    bool m_connecting = false;                         ///< 招待で接続している途中（欄の下に「接続しています…」）
};

#endif // P2PINVITEPANEL_H
