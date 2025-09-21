#ifndef BRANCHCANDIDATESCONTROLLER_H
#define BRANCHCANDIDATESCONTROLLER_H

#include <QObject>
#include <QVector>
#include <QModelIndex>
#include "kifdisplayitem.h"

class KifuVariationEngine;
class KifuBranchListModel;
struct KifDisplayItem;

class BranchCandidatesController : public QObject {
    Q_OBJECT
public:
    explicit BranchCandidatesController(KifuVariationEngine* ve,
                                        KifuBranchListModel* model,
                                        QObject* parent = nullptr);

    // ★ 追加：あと差し用
    void setEngine(KifuVariationEngine* ve) { m_ve = ve; }

public slots:
    // 分岐候補ビューでのクリック（RecordPane 等から QModelIndex を受け取る）
    void activateCandidate(const QModelIndex& index);

    void refreshForPly(int ply) { refreshCandidatesForPly(ply, /*includeMainline=*/false); } // ← 委譲

    void refreshCandidatesForPly(int ply, bool includeMainline);                    // 既存
    void refreshCandidatesForPly(int ply, bool includeMainline, const QString& prevSfen); // ★追加

signals:
    // MainWindow 側で display/rebuild を行うための通知
    void applyLineRequested(const QList<KifDisplayItem>& disp, const QStringList& usi);
    void backToMainRequested();

private:
    KifuVariationEngine*  m_ve   = nullptr;
    KifuBranchListModel*  m_model = nullptr;
    QVector<int>          m_varIds;   // 行 → variationId の対応
};

#endif // BRANCHCANDIDATESCONTROLLER_H
