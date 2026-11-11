#ifndef P2PPEERSPANEL_H
#define P2PPEERSPANEL_H

/// @file p2ppeerspanel.h
/// @brief 既知の相手の管理（名前・確認コード・確認済み・ブロック・削除。実装仕様 3.4節・11章）

#include <QWidget>

class P2pIdentityStore;
class P2pKnownPeers;
class QPushButton;
class QTableWidget;

class P2pPeersPanel : public QWidget
{
    Q_OBJECT
public:
    P2pPeersPanel(P2pKnownPeers* knownPeers, P2pIdentityStore* identity, QWidget* parent = nullptr);

public slots:
    void reload();

private slots:
    void onRename();
    void onShowCode();
    void onToggleVerified();
    void onToggleBlocked();
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
    QPushButton* m_remove = nullptr;
};

#endif // P2PPEERSPANEL_H
