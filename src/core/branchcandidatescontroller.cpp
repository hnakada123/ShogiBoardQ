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

void BranchCandidatesController::refreshCandidatesForPly(int ply, bool includeMainline)
{
    if (!m_ve || !m_model) {
        qDebug() << "[BRANCH-CTL] missing ve/model; abort";
        return;
    }

    // 候補を取得
    auto cs = m_ve->branchCandidatesForPly(ply, includeMainline);

    // ★ 修正: 「includeMainline=true かつ 候補数=1」⇒ 本譜だけ = 分岐なし → 非表示
    if (includeMainline && cs.size() <= 1) {
        m_model->clearBranchCandidates();
        m_model->setHasBackToMainRow(false);
        qDebug() << "[BRANCH-CTL] only mainline (size<=1). hide at ply=" << ply;
        return;
    }

    // ここから「分岐あり」だけモデル反映
    QList<KifDisplayItem> items;
    items.reserve(cs.size());
    for (const auto& c : cs) {
        // c.label を使って表示ラベル作成（従来どおり）
        items.push_back(KifDisplayItem{ c.label, QString() });
    }

    m_model->setBranchCandidatesFromKif(items);
    // 「戻る」を付けるのは “本譜+分岐” を並べた場合のみ
    m_model->setHasBackToMainRow(includeMainline && cs.size() > 1);
}
