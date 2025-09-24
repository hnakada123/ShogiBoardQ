#ifndef BRANCHCANDIDATESCANDIDATESCONTROLLER_H
#define BRANCHCANDIDATESCANDIDATESCONTROLLER_H

#include <QObject>
#include <QVector>
#include <QSet>
#include <QString>
#include "kifdisplayitem.h"
#include "branchdisplayplan.h"   // ★ 共通型（BranchCandidateDisplayItem など）

class KifuVariationEngine;
class KifuBranchListModel;

class BranchCandidatesController : public QObject
{
    Q_OBJECT
public:
    explicit BranchCandidatesController(KifuVariationEngine* ve,
                                        KifuBranchListModel* model,
                                        QObject* parent = nullptr);

    // クリック（候補の行）→ Plan メタに従って行/手へジャンプ
    Q_INVOKABLE void activateCandidate(int rowIndex);

    // ★Plan 方式：MainWindow 側で作った Plan をそのまま流し込む
    void refreshCandidatesFromPlan(int ply1,
                                   const QVector<BranchCandidateDisplayItem>& items,
                                   const QString& baseLabel);

    // （互換）旧ロジックの名残り。Plan専用化により実質ノーオペにしておく。
    void refreshCandidatesForPly(int /*ply*/,
                                 bool /*includeMainline*/,
                                 const QString& /*prevSfen*/,
                                 const QSet<int>& /*restrictVarIds*/);

    // ==== MainWindow から invokeMethod で呼ばれるハイライト系API（厳密に int,int のシグネチャ） ====
    Q_INVOKABLE void clearHighlight();

    Q_INVOKABLE bool hasSetActiveVidPly() const { return true; }
    Q_INVOKABLE bool setActive(int variationId, int ply);            // 互換名
    Q_INVOKABLE bool setActiveVidPly(int variationId, int ply) {     // 実体は setActive に集約
        return setActive(variationId, ply);
    }

    Q_INVOKABLE bool hasSetActiveVariation() const { return true; }
    Q_INVOKABLE void setActiveVariation(int variationId);

    Q_INVOKABLE bool hasSetActivePly() const { return true; }
    Q_INVOKABLE void setActivePly(int ply);

signals:
    // 旧来イベント（必要なら残す）
    void applyLineRequested(const QList<KifDisplayItem>& disp,
                            const QStringList& usiStrs);
    void backToMainRequested();

    // ★Plan専用：候補クリック時に MainWindow 側で行/手へジャンプ
    void planActivated(int row, int ply1);

    // ★ハイライト変更通知（モデルやビュー側が拾って再描画する用途）
    void highlightChanged(int variationId, int ply);

private:
    struct PlanMeta {
        int     targetRow  = -1;
        int     targetPly  =  0;   // 1-based
        QString label;
        QString lineName;          // "Main" or "VarN"
    };

    void applyHighlight_(); // 内部一元処理

    KifuVariationEngine*         m_ve    = nullptr;  // 参照だけ残す（現状Planでは未使用）
    KifuBranchListModel*         m_model = nullptr;
    bool                         m_planMode = true; // 常にPlanモード
    QVector<PlanMeta>            m_planMetas;       // 表示に対応するクリック時メタ

    // ★ハイライトの現在値（vid/ply）。invokeMethod 経由で更新される
    int m_activeVid = -1;
    int m_activePly = -1;
};

#endif // BRANCHCANDIDATESCANDIDATESCONTROLLER_H
