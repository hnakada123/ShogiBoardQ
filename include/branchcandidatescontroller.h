#ifndef BRANCHCANDIDATESCANDIDATESCONTROLLER_H
#define BRANCHCANDIDATESCANDIDATESCONTROLLER_H

#include <QObject>
#include <QVector>
#include <QSet>
#include <QString>
#include "kifdisplayitem.h"
#include "branchdisplayplan.h"   // ★ 共通型（BranchCandidateDisplayItem など）
#include "kifuvariationengine.h"

class KifuBranchListModel;
class RecordPane;

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

    // RecordPane のシグナルを自身に配線（MainWindow から配線コードを追い出す）
    void attachRecordPane(RecordPane* pane);

public slots:
    // RecordPane 側の「分岐セルが選ばれた」→ 自身の activateCandidate() へ
    void onRecordPaneBranchActivated(const QModelIndex& index);

signals:
    // 旧来イベント（必要なら残す）
    void applyLineRequested(const QList<KifDisplayItem>& disp,
                            const QStringList& usiStrs);
    void backToMainRequested();

    // ★Plan専用：候補クリック時に MainWindow 側で行/手へジャンプ
    void planActivated(int row, int ply1);

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
