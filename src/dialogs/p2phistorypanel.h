#ifndef P2PHISTORYPANEL_H
#define P2PHISTORYPANEL_H

/// @file p2phistorypanel.h
/// @brief P2P対局の記録の一覧（結果と署名の状態を分けて出す。証跡の検証・書き出し・削除。実装仕様 9章・11章）

#include <QDateTime>
#include <QHash>
#include <QString>
#include <QWidget>

#include <optional>

#include "p2pevidenceverifier.h"

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
    /// 証跡を検証した結果。大きさと更新時刻が前に検証したときと同じなら、検証し直さない
    struct VerifiedFile {
        qint64 size = -1;
        QDateTime modified;
        std::optional<P2p::EvidenceReport> report;   ///< 読めなければ nullopt
    };

    QString selectedPath() const;
    QByteArray selectedGameId() const;
    /// 証跡を読んで検証した結果（変わっていなければ前の結果）。seen に今の一覧の分を集める
    std::optional<P2p::EvidenceReport> verifiedReport(const QString& path, QHash<QString, VerifiedFile>& seen) const;

    P2pRecordStore* m_store = nullptr;        ///< 非所有
    P2pKnownPeers* m_knownPeers = nullptr;    ///< 非所有
    bool m_hideUnknownNames = false;
    QTableWidget* m_table = nullptr;
    QPushButton* m_verify = nullptr;
    QPushButton* m_export = nullptr;
    QPushButton* m_remove = nullptr;
    /// 一覧に出している証跡の検証の結果（作り直すたびに、今の一覧の分だけにする）
    QHash<QString, VerifiedFile> m_verified;
};

#endif // P2PHISTORYPANEL_H
