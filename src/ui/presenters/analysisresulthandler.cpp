/// @file analysisresulthandler.cpp
/// @brief 解析結果ハンドラクラスの実装

#include "analysisresulthandler.h"
#include "evaluationseries.h"

#include "kifuanalysislistmodel.h"
#include "kifuanalysisrow.h"
#include "kifupresentation.h"
#include "sfenutils.h"

#include <QString>
#include <QStringList>

#include "logcategories.h"
#include "analysisresultspanel.h"
#include "pvboarddialog.h"

namespace {
QString resolveLastUsiMove(const AnalysisResultHandler::Refs& refs, int ply)
{
    return refs.line ? refs.line->usiMove(ply).toString() : QString();
}

} // namespace

AnalysisResultHandler::AnalysisResultHandler(const Refs& refs)
    : m_refs(refs)
{
}

UsiScore AnalysisResultHandler::append(const PlyAnalysis& result)
{
    const int ply = result.ply;
    if (!m_refs.analysisModel || ply < 0) return {};

    const UsiScore score = result.book ? UsiScore{} : result.score;

    const QString position = m_refs.line && ply <= m_refs.line->moveCount()
        ? SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply)) : QString();
    const bool isGoteTurn = position.isEmpty() ? ply % 2 == 1
        : position.section(QLatin1Char(' '), 1, 1) == QStringLiteral("w");
    // エンジンは手番側から見た評価値を出す（先手から見た値への直しは評価値グラフの点と同じ規則）
    const UsiScore blackScore = EvaluationSeries::blackView(isGoteTurn ? Turn::White : Turn::Black, score);
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
        resultRow.beforeSfen = SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply - 1));
    if (m_refs.line && ply >= 0 && ply <= m_refs.line->moveCount())
        resultRow.sfen = SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply));
    resultRow.lastUsiMove = resolveLastUsiMove(m_refs, ply);

    // 候補手（前の行の読み筋の最初の指し手）
    if (const KifuAnalysisRow* previous = m_refs.analysisModel->row(m_refs.analysisModel->rowCount() - 1)) {
        if (!resultRow.beforeSfen.isEmpty() && resultRow.beforeSfen == previous->sfen) {
            if (const std::optional<UsiMove> first = previous->pv.firstMove()) resultRow.candidateUsi = first->toString();
            resultRow.candidateSfen = previous->sfen;
        }
        if (!resultRow.candidateUsi.isEmpty()) {
            QString candidate = KifuPresentation::move(resultRow.candidateSfen, resultRow.candidateUsi,
                                                       {KifuPresentation::Notation::Japanese, false});
            const QString& previousMove = previous->lastUsiMove;
            if (previousMove.size() >= 4 && previousMove.mid(2, 2) == resultRow.candidateUsi.mid(2, 2))
                candidate.replace(1, 2, QStringLiteral("同　"));
            resultRow.candidateMove = candidate;
        }
    }

    m_refs.analysisModel->appendRow(resultRow);

    return blackScore;
}

// ============================================================
// AnalysisResultHandler のPVダイアログ・棋譜変換処理
// ============================================================

void AnalysisResultHandler::showPvBoardDialog(int row)
{
    qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: row=" << row;

    if (!m_refs.analysisModel) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: analysisModel is null";
        return;
    }

    if (row < 0 || row >= m_refs.analysisModel->rowCount()) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: row out of range";
        return;
    }

    const KifuAnalysisRow* item = m_refs.analysisModel->row(row);
    if (!item) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: item is null";
        return;
    }

    // 読み筋の無い行（定跡）は開かない。各結果は解析した局面を持つので、表の行番号を手数として読み直さない
    if (item->book || !item->pv.isDisplayable() || item->pv.baseSfen().isEmpty()) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: no pv";
        return;
    }

    auto* dlg = new PvBoardDialog(item->pv, m_refs.presenter);

    const QString blackName = m_refs.blackPlayerName.isEmpty() ? tr("先手") : m_refs.blackPlayerName;
    const QString whiteName = m_refs.whitePlayerName.isEmpty() ? tr("後手") : m_refs.whitePlayerName;
    dlg->setPlayerNames(blackName, whiteName);
    dlg->setFlipMode(m_refs.boardFlipped);

    const QString lastMove = item->lastUsiMove;
    if (!lastMove.isEmpty()) {
        dlg->setLastMove(lastMove);
    }

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}
