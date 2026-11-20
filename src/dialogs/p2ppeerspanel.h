#ifndef P2PPEERSPANEL_H
#define P2PPEERSPANEL_H

/// @file p2ppeerspanel.h
/// @brief 既知の相手の管理（名前・確認コード・確認済み・ブロック・削除・友だち。実装仕様 3.4節・3.3.3節・11章）

#include <QWidget>

#include <QPointer>

class P2pFriendPresence;
class P2pIdentityStore;
class P2pKnownPeers;
class QCheckBox;
class QLabel;
class QPushButton;
class QTableWidget;

class P2pPeersPanel : public QWidget
{
    Q_OBJECT
public:
    P2pPeersPanel(P2pKnownPeers* knownPeers, P2pIdentityStore* identity, QWidget* parent = nullptr);

    /// 友だちの在席（非所有）。渡すと、友だちの在席を表に出し、表示している間だけ在席を調べさせる
    void setPresence(P2pFriendPresence* presence);
    /// 「友だちに在席を知らせる」のチェックを、保存した設定に合わせる（知らせは出さない）
    void setSharing(bool sharing);

signals:
    /// 選んだ友だちに対局を申し込む
    void applyRequested(const QByteArray& playerKey);
    /// 「友だちに在席を知らせる」を切り替えた（保存と反映は受け手が行う）
    void sharingToggled(bool sharing);

public slots:
    void reload();

protected:
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private slots:
    void onRename();
    void onShowCode();
    void onToggleVerified();
    void onToggleBlocked();
    void onToggleFriend();
    void onApply();
    void onRemove();
    void onSelectionChanged();

private:
    QByteArray selectedKey() const;

    P2pKnownPeers* m_knownPeers = nullptr;     ///< 非所有
    P2pIdentityStore* m_identity = nullptr;    ///< 非所有
    QTableWidget* m_table = nullptr;
    QPushButton* m_rename = nullptr;
    QPushButton* m_code = nullptr;
    QPushButton* m_verified = nullptr;
    QPushButton* m_blocked = nullptr;
    QPushButton* m_friend = nullptr;
    QPushButton* m_apply = nullptr;
    QPushButton* m_remove = nullptr;
    QCheckBox* m_share = nullptr;
    QLabel* m_friendNote = nullptr;
    QPointer<P2pFriendPresence> m_presence;    ///< 非所有
};

#endif // P2PPEERSPANEL_H
