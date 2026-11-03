/// @file kifuanalysisrowpresenter.cpp
/// @brief 棋譜解析の1局面の結果を結果の表の1行にする処理の実装

#include "kifuanalysisrowpresenter.h"

#include "kifmovetext.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisrow.h"
#include "logcategories.h"
#include "usiposition.h"

#include <QString>

KifuAnalysisRowPresenter::KifuAnalysisRowPresenter(const Refs& refs)
    : m_refs(refs)
{
}

UsiScore KifuAnalysisRowPresenter::append(const PlyAnalysis& result)
{
    const int ply = result.ply;
    if (!m_refs.analysisModel || ply < 0) return {};

    // 先手から見た値は解析した局面の手番で直してある（PlyAnalysisCollector。CLI の analyze-kifu と同じ値）
    const UsiScore blackScore = result.book ? UsiScore{} : result.blackScore;
    std::optional<int> difference;
    if (blackScore.isCp()) {
        if (m_previousCp) difference = blackScore.value - *m_previousCp;
        m_previousCp = blackScore.value;
    } else {
        m_previousCp.reset();
    }

    qCDebug(lcAnalysis).noquote() << "append: ply=" << ply << "book=" << result.book << "pv=" << result.pv.usiText().left(30);

    KifuAnalysisRow resultRow;
    resultRow.ply = ply;
    if (m_refs.line) resultRow.move = m_refs.line->displayText(ply);
    resultRow.branch = m_refs.branchPlies.contains(ply);
    resultRow.book = result.book;
    resultRow.evaluation = blackScore;
    resultRow.evaluationDifference = difference;
    if (!result.book) resultRow.pv = result.pv;
    resultRow.depth = result.book ? -1 : result.depth;
    if (m_refs.line && ply > 0 && ply <= m_refs.line->moveCount() + 1)
        resultRow.beforeSfen = UsiPosition::sfenOf(m_refs.line->sfenAt(ply - 1));
    if (m_refs.line && ply >= 0 && ply <= m_refs.line->moveCount())
        resultRow.sfen = UsiPosition::sfenOf(m_refs.line->sfenAt(ply));
    if (m_refs.line) resultRow.lastUsiMove = m_refs.line->usiMove(ply).toString();

    // 候補手（前の行の読み筋の最初の指し手。前の行の局面に至った手と同じマスへの手は「同　」）
    const KifuAnalysisRow* previous = m_refs.analysisModel->row(m_refs.analysisModel->rowCount() - 1);
    if (previous && !resultRow.beforeSfen.isEmpty() && resultRow.beforeSfen == previous->sfen
        && !previous->pv.steps().isEmpty()) {
        const PvText::Step& first = previous->pv.steps().constFirst();
        resultRow.candidateUsi = first.move.toString();
        resultRow.candidateSfen = previous->sfen;
        resultRow.candidateMove = KifMoveText::format(first.mover, first.moving, first.move,
                                                      UsiMove::fromString(previous->lastUsiMove).to);
    }

    m_refs.analysisModel->appendRow(resultRow);

    return blackScore;
}
