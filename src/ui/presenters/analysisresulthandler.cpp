/// @file analysisresulthandler.cpp
/// @brief 解析結果ハンドラクラスの実装

#include "analysisresulthandler.h"
#include "evaluationseries.h"

#include "kifuanalysislistmodel.h"
#include "kifuanalysisresultsdisplay.h"
#include "sfenutils.h"

#include <QString>
#include <QStringList>

#include "logcategories.h"
#include "analysisresultspanel.h"
#include "pvboarddialog.h"

namespace {
QString resolveMoveLabel(const AnalysisResultHandler::Refs& refs, int ply)
{
    if (ply == 0) return QObject::tr("=== 開始局面 ===");
    if (refs.line && ply > 0 && ply <= refs.line->moveCount()) {
        const QString label = refs.line->displayText(ply);
        if (!label.isEmpty()) return label;
    }
    return QStringLiteral("ply %1").arg(ply);
}

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
    const QString usiPv = result.book ? QString() : result.usiPv.join(QLatin1Char(' '));
    const QString pv = result.book ? tr("（定跡）") : result.pvKanji;

    const QString moveLabel = resolveMoveLabel(m_refs, ply);

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

    qCDebug(lcAnalysis).noquote() << "append: ply=" << ply << "moveLabel=" << moveLabel << "pv=" << pv.left(30);

    auto* resultItem = new KifuAnalysisResultsDisplay(moveLabel, blackScore, difference, pv);

    resultItem->setUsiPv(usiPv);
    if (m_refs.line && ply > 0 && ply <= m_refs.line->moveCount() + 1)
        resultItem->beforeSfen = SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply - 1));

    // 局面SFENを設定
    if (m_refs.line && ply >= 0 && ply <= m_refs.line->moveCount()) {
        const QString sfen = SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply));
        resultItem->setSfen(sfen);
    }

    const QString lastMove = resolveLastUsiMove(m_refs, ply);
    if (!lastMove.isEmpty()) {
        resultItem->setLastUsiMove(lastMove);
        qCDebug(lcAnalysis).noquote() << "setLastUsiMove: ply=" << ply << "lastMove=" << lastMove;
    }

    // 候補手を設定（前の行の読み筋の最初の指し手）
    int prevRow = m_refs.analysisModel->rowCount() - 1;  // 今追加しようとしている行の1つ前
    if (prevRow >= 0) {
        KifuAnalysisResultsDisplay* prevItem = m_refs.analysisModel->item(prevRow);
        if (prevItem) {
            if (!resultItem->beforeSfen.isEmpty() && resultItem->beforeSfen == prevItem->sfen()) {
                resultItem->candidateUsi = prevItem->usiPv().simplified().section(QLatin1Char(' '), 0, 0);
                resultItem->candidateSfen = prevItem->sfen();
            }
            if (!resultItem->candidateUsi.isEmpty()) {
                QString candidate = KifuPresentation::move(resultItem->candidateSfen, resultItem->candidateUsi,
                    {KifuPresentation::Notation::Japanese, false});
                const QString previous = prevItem->lastUsiMove();
                if (previous.size() >= 4 && previous.mid(2, 2) == resultItem->candidateUsi.mid(2, 2))
                    candidate.replace(1, 2, QStringLiteral("同　"));
                resultItem->setCandidateMove(candidate);
            }
        }
    }

    m_refs.analysisModel->appendItem(resultItem);

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

    KifuAnalysisResultsDisplay* item = m_refs.analysisModel->item(row);
    if (!item) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: item is null";
        return;
    }

    const QString kanjiPv = item->principalVariation();
    if (kanjiPv.isEmpty()) {
        qCDebug(lcAnalysis).noquote() << "showPvBoardDialog: kanjiPv is empty";
        return;
    }

    QString baseSfen = item->sfen();
    // 各結果は対象局面を保持している。表示行番号を手数として再解釈しない。
    if (baseSfen.isEmpty()) return;

    QStringList usiMoves;
    const QString usiPvStr = item->usiPv();
    if (!usiPvStr.isEmpty()) {
        usiMoves = usiPvStr.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    }

    QWidget* parentWidget = nullptr;
    if (m_refs.presenter && m_refs.presenter->container()) {
        parentWidget = m_refs.presenter->container();
    }

    auto* dlg = new PvBoardDialog(baseSfen, usiMoves, parentWidget);
    dlg->setKanjiPv(kanjiPv);

    const QString blackName = m_refs.blackPlayerName.isEmpty() ? tr("先手") : m_refs.blackPlayerName;
    const QString whiteName = m_refs.whitePlayerName.isEmpty() ? tr("後手") : m_refs.whitePlayerName;
    dlg->setPlayerNames(blackName, whiteName);
    dlg->setFlipMode(m_refs.boardFlipped);

    QString lastMove = item->lastUsiMove();
    if (!lastMove.isEmpty()) {
        dlg->setLastMove(lastMove);
    }

    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->show();
}
