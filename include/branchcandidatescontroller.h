#ifndef BRANCHCANDIDATESCONTROLLER_H
#define BRANCHCANDIDATESCONTROLLER_H

#include <QObject>
#include <QVector>
#include <QModelIndex>
#include <QSet>
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

    // あと差し用
    void setEngine(KifuVariationEngine* ve) { m_ve = ve; }

public slots:
    // 分岐候補ビューでのクリック
    void activateCandidate(const QModelIndex& index);

    // 互換ラッパ（空の文脈・空セットで呼ぶ）
    void refreshForPly(int ply) {
        refreshCandidatesForPly(ply, /*includeMainline=*/false, QString(), QSet<int>());
    }

    // ★この1本に統一（restrictVarIds は省略可）
    void refreshCandidatesForPly(int ply,
                                 bool includeMainline,
                                 const QString& prevSfen,
                                 const QSet<int>& restrictVarIds = QSet<int>());

signals:
    void applyLineRequested(const QList<KifDisplayItem>& disp, const QStringList& usi);
    void backToMainRequested();

private:
    KifuVariationEngine*  m_ve    = nullptr;
    KifuBranchListModel*  m_model = nullptr;
    QVector<int>          m_varIds;  // 行→variationId の対応
};

#endif // BRANCHCANDIDATESCONTROLLER_H
