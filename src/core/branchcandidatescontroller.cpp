#include "branchcandidatescontroller.h"

#include "kifuvariationengine.h"
#include "kifubranchlistmodel.h"

//====================== コンストラクタ ======================
BranchCandidatesController::BranchCandidatesController(KifuVariationEngine* ve,
                                                       KifuBranchListModel* model,
                                                       QObject* parent)
    : QObject(parent), m_ve(ve), m_model(model)
{
    // 何もなし（必要ならここでシグナル配線）
}

void BranchCandidatesController::activateCandidate(const QModelIndex& index)
{
    qDebug().nospace() << "[BRANCH-CTL] activateCandidate row=" << index.row()
    << " valid=" << index.isValid();

    if (!index.isValid() || !m_model) {
        qWarning() << "[BRANCH-CTL] invalid index or model null";
        return;
    }

    if (m_model->isBackToMainRow(index.row())) {
        qDebug() << "[BRANCH-CTL] backToMain clicked";
        emit backToMainRequested();
        return;
    }

    const int r = index.row();
    if (r < 0 || r >= m_varIds.size() || !m_ve) {
        qWarning().nospace() << "[BRANCH-CTL] row out-of-range or ve null; r=" << r
                             << " size=" << m_varIds.size();
        return;
    }

    const int variationId = m_varIds.at(r);
    qDebug().nospace() << "[BRANCH-CTL] resolve variationId=" << variationId;

    const ResolvedLine line = m_ve->resolveAfterWins(variationId);
    qDebug().nospace() << "[BRANCH-CTL] resolved: disp=" << line.disp.size()
                       << " usi=" << line.usi.size();

    QStringList usiList;
    usiList.reserve(line.usi.size());
    for (const auto& u : line.usi) usiList.push_back(u);

    qDebug() << "[BRANCH-CTL] emit applyLineRequested";
    emit applyLineRequested(line.disp, usiList);
}

// 本体（文脈 prevSfen つき）
void BranchCandidatesController::refreshCandidatesForPly(int ply,
                                                         bool includeMainline,
                                                         const QString& prevSfen)
{
    if (!m_ve || !m_model) {
        qDebug() << "[BRANCH-CTL] missing ve/model; abort";
        return;
    }

    qDebug().noquote()
        << "[BRANCH-CTL] refresh ply=" << ply
        << " includeMainline=" << includeMainline
        << " prevSfen=" << (prevSfen.isEmpty() ? "<EMPTY>" : prevSfen);

    // エンジンへ（★ 文脈 SFEN を渡す）
    auto cs = m_ve->branchCandidatesForPly(ply, includeMainline, prevSfen);

    // 本譜のみしか無いなら非表示
    if (includeMainline && cs.size() <= 1) {
        m_model->clearBranchCandidates();
        m_model->setHasBackToMainRow(false);
        qDebug() << "[BRANCH-CTL] only mainline (size<=1). hide at ply=" << ply;
        return;
    }

    // 候補をモデルに反映
    QList<KifDisplayItem> items;
    items.reserve(cs.size());
    int i = 0;
    for (const auto& c : cs) {
        items.push_back(KifDisplayItem{ c.label, QString() });
        qDebug().noquote()
            << "  [BRANCH-CTL] item[" << i++ << "]=" << c.label;
    }

    m_model->setBranchCandidatesFromKif(items);
    m_model->setHasBackToMainRow(includeMainline && cs.size() > 1);
}

// 互換ラッパ（prevSfen を渡し忘れた経路の早期検知用）
void BranchCandidatesController::refreshCandidatesForPly(int ply, bool includeMainline)
{
    qWarning() << "[BRANCH-CTL] WARN: called without prevSfen; context filter disabled";
    refreshCandidatesForPly(ply, includeMainline, QString{});
}
