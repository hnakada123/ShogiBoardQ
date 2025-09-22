#include "branchcandidatescontroller.h"
#include "kifuvariationengine.h"
#include "kifubranchlistmodel.h"
#include <QDebug>

//====================== コンストラクタ ======================
BranchCandidatesController::BranchCandidatesController(KifuVariationEngine* ve,
                                                       KifuBranchListModel* model,
                                                       QObject* parent)
    : QObject(parent), m_ve(ve), m_model(model)
{
}

//====================== クリック処理 ======================
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

// BranchCandidatesController.cpp
void BranchCandidatesController::refreshCandidatesForPly(int ply,
                                                         bool includeMainline,
                                                         const QString& prevSfen,
                                                         const QSet<int>& restrictVarIds)
{
    if (!m_ve || !m_model) return;

    auto cands = m_ve->branchCandidatesForPly(ply, includeMainline, prevSfen);

    // ★変更点：ホワイトリストがある場合は “ID完全一致” のみ採用（mainline特例は廃止）
    if (!restrictVarIds.isEmpty()) {
        QList<BranchCandidate> filtered;
        filtered.reserve(cands.size());
        for (const auto& c : cands) {
            if (restrictVarIds.contains(c.variationId)) {
                filtered.push_back(c);
            }
        }
        cands.swap(filtered);
    }

    // ↓ 以降は従来どおりモデル更新
    QList<KifDisplayItem> rows;
    rows.reserve(cands.size());
    m_varIds.clear();
    m_varIds.reserve(cands.size() + 1);

    for (const auto& c : cands) {
        rows.push_back(KifDisplayItem{ c.label, QString() });
        m_varIds.push_back(c.variationId);
    }

    m_model->setBranchCandidatesFromKif(rows);
    m_model->setHasBackToMainRow(!cands.isEmpty());  // 候補があれば「本譜へ戻る」を出す
    m_varIds.push_back(-1);                          // 末尾ダミー
}
