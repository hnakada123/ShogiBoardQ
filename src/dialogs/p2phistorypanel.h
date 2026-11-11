#ifndef P2PHISTORYPANEL_H
#define P2PHISTORYPANEL_H

/// @file p2phistorypanel.h
/// @brief P2P対局の記録の一覧（結果と署名の状態を分けて出す。証跡の検証・書き出し・削除。実装仕様 9章・11章）

#include <QWidget>

class P2pKnownPeers;
class P2pRecordStore;
class QPushButton;
class QTableWidget;

class P2pHistoryPanel : public QWidget
{
    Q_OBJECT
public:
    P2pHistoryPanel(P2pRecordStore* store, P2pKnownPeers* knownPeers, QWidget* parent = nullptr);

    void setHideUnknownNames(bool hide) { m_hideUnknownNames = hide; reload(); }

public slots:
    void reload();

private slots:
    void onVerify();
    void onExport();
    void onRemove();
    void onSelectionChanged();

private:
    QString selectedPath() const;
    QByteArray selectedGameId() const;

    P2pRecordStore* m_store = nullptr;        ///< 非所有
    P2pKnownPeers* m_knownPeers = nullptr;    ///< 非所有
    bool m_hideUnknownNames = false;
    QTableWidget* m_table = nullptr;
    QPushButton* m_verify = nullptr;
    QPushButton* m_export = nullptr;
    QPushButton* m_remove = nullptr;
};

#endif // P2PHISTORYPANEL_H
