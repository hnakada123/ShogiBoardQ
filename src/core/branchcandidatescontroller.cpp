#include "branchcandidatescontroller.h"
#include "kifuvariationengine.h"
#include "kifubranchlistmodel.h"
#include "recordpane.h"

#include <QMetaType>
#include <QModelIndex>
#include <QDebug>

//====================== コンストラクタ ======================
BranchCandidatesController::BranchCandidatesController(KifuVariationEngine* ve,
                                                       KifuBranchListModel* model,
                                                       QObject* parent)
    : QObject(parent)
    , m_ve(ve)
    , m_model(model)
{
    m_planMode = true;     // ★Plan方式を常時有効
    m_planMetas.clear();

    qDebug().nospace()
        << "[WIRE] BranchCandidatesController created ve=" << static_cast<void*>(ve)
        << " model=" << static_cast<void*>(model);
}

//====================== クリック処理 ======================
void BranchCandidatesController::activateCandidate(int rowIndex)
{
    // Plan 方式のみ
    if (rowIndex < 0 || rowIndex >= m_planMetas.size()) {
        qDebug() << "[BRANCH-CTL] activateCandidate: out of range idx=" << rowIndex
                 << " metas=" << m_planMetas.size();
        return;
    }
    const auto& m = m_planMetas[rowIndex];
    qDebug() << "[BRANCH-CTL] planActivated row=" << m.targetRow
             << " ply=" << m.targetPly
             << " label=" << m.label
             << " line=" << m.lineName;
    emit planActivated(m.targetRow, m.targetPly);
}

//====================== （互換）旧方式はノーオペ ======================
void BranchCandidatesController::refreshCandidatesForPly(int /*ply*/,
                                                         bool /*includeMainline*/,
                                                         const QString& /*prevSfen*/,
                                                         const QSet<int>& /*restrictVarIds*/)
{
    if (!m_model) return;
    m_planMode = true;            // 念のため Plan モードを維持
    m_planMetas.clear();

    m_model->clearBranchCandidates();
    m_model->setHasBackToMainRow(false);
    qDebug() << "[BRANCH-CTL] refreshCandidatesForPly: legacy API (no-op in Plan mode)";
}

//====================== Plan をそのまま表示 ======================
void BranchCandidatesController::refreshCandidatesFromPlan(
    int ply1,
    const QVector<BranchCandidateDisplayItem>& items,
    const QString& /*baseLabel*/)
{
    m_planMode = true;
    m_planMetas.clear();

    if (m_model) {
        m_model->clearBranchCandidates();
        m_model->setHasBackToMainRow(false); // Plan は「戻る」不要
    }

    // モデルに「指し手ラベル」だけ流す
    QList<KifDisplayItem> rows;
    rows.reserve(items.size());
    for (const auto& it : items) {
        rows.push_back(KifDisplayItem(it.label)); // 例: "▲２六歩(27)"
    }
    if (m_model) {
        m_model->setBranchCandidatesFromKif(rows);
    }

    // クリック時のジャンプ先（行/手）をメタに揃えて保持
    m_planMetas.reserve(items.size());
    for (const auto& it : items) {
        PlanMeta meta;
        meta.targetRow  = it.row;
        meta.targetPly  = ply1;       // 「その手」で切替える
        meta.label      = it.label;
        meta.lineName   = it.lineName;
        m_planMetas.push_back(meta);
    }

    qDebug() << "[BRANCH-CTL] set plan items =" << items.size()
             << " ply=" << ply1;
}

void BranchCandidatesController::attachRecordPane(RecordPane* pane)
{
    if (!pane) return;

    // ここで必要なメタタイプ登録を Controller 側に寄せる
    qRegisterMetaType<KifDisplayItem>("KifDisplayItem");
    qRegisterMetaType<QList<KifDisplayItem>>("QList<KifDisplayItem>");
    qRegisterMetaType<QStringList>("QStringList");

    QObject::connect(pane, &RecordPane::branchActivated,
                     this, &BranchCandidatesController::onRecordPaneBranchActivated,
                     Qt::UniqueConnection);
}

void BranchCandidatesController::onRecordPaneBranchActivated(const QModelIndex& index)
{
    if (!index.isValid()) return;
    activateCandidate(index.row());
}
