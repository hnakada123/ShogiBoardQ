/// @file analysisresulthandler.cpp
/// @brief 解析結果ハンドラクラスの実装

#include "analysisresulthandler.h"

#include "analysiscoordinator.h"
#include "kifuanalysislistmodel.h"
#include "kifurecordlistmodel.h"
#include "kifudisplay.h"
#include "kifuanalysisresultsdisplay.h"
#include "sfenutils.h"

#include <limits>
#include <QString>
#include <QStringList>

#include "logcategories.h"

namespace {
QString sanitizeUsiPv(const QString& rawPv, bool isBook)
{
    if (isBook) {
        return QString();
    }

    QString usiPv = rawPv;
    const qsizetype parenPos = usiPv.indexOf(QLatin1Char('('));
    if (parenPos > 0) {
        usiPv = usiPv.left(parenPos).trimmed();
    }
    // 詰んだ局面で返る resign / win は指し手ではないため、候補手や読み筋の盤面表示に使わない
    QStringList moves;
    const QStringList tokens = usiPv.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    for (const QString& token : tokens) {
        if (token == QLatin1String("resign") || token == QLatin1String("win")) break;
        moves.append(token);
    }
    return moves.join(QLatin1Char(' '));
}

QString resolveMoveLabel(const AnalysisResultHandler::Refs& refs, int ply)
{
    if (refs.recordModel && ply >= 0 && refs.recordModel->rowCount() > ply) {
        if (KifuDisplay* disp = refs.recordModel->item(ply)) {
            const QString label = disp->currentMove();
            if (!label.isEmpty()) {
                return label;
            }
        }
    }
    return QStringLiteral("ply %1").arg(ply);
}

QString resolveLastUsiMove(const AnalysisResultHandler::Refs& refs, int ply)
{
    if (refs.usiMoves && ply > 0 && ply <= refs.usiMoves->size()) {
        return refs.usiMoves->at(ply - 1);
    }
    if (refs.recordModel && ply > 0 && ply < refs.recordModel->rowCount()) {
        if (KifuDisplay* moveDisp = refs.recordModel->item(ply)) {
            return moveDisp->usiMove;
        }
    }
    return QString();
}

int evaluateForDisplay(bool isGoteTurn,
                       int scoreCp,
                       int mate,
                       bool isBook,
                       int prevEvalCp,
                       QString* outEvalStr)
{
    if (!outEvalStr) {
        return prevEvalCp;
    }

    if (isBook) {
        *outEvalStr = QStringLiteral("-");
        return prevEvalCp;
    }
    if (mate != 0) {
        const int adjustedMate = isGoteTurn ? -mate : mate;
        *outEvalStr = QStringLiteral("mate %1").arg(adjustedMate);
        return prevEvalCp;
    }
    if (scoreCp != std::numeric_limits<int>::min()) {
        const int adjustedScore = isGoteTurn ? -scoreCp : scoreCp;
        *outEvalStr = QString::number(adjustedScore);
        return adjustedScore;
    }

    *outEvalStr = QStringLiteral("-");
    return prevEvalCp;
}

} // namespace

void AnalysisResultHandler::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void AnalysisResultHandler::reset()
{
    m_prevEvalCp = 0;
    m_hasPreviousEval = false;
    m_pendingPly = -1;
    m_pendingScoreCp = 0;
    m_pendingMate = 0;
    m_pendingMateText.clear();
    m_pendingPv.clear();
    m_pendingPvKanji.clear();
    m_lastCommittedPly = -1;
    m_lastCommittedScoreCp = 0;
    m_lastCommittedMate.clear();
}

void AnalysisResultHandler::updatePending(int ply, int scoreCp, int mate, const QString& pv, const QString& rawMate)
{
    // 結果を一時保存（bestmove時に確定）
    // 最新のinfo行で更新し続ける
    // 注意: m_pendingPvKanjiはupdatePendingPvKanjiで設定されるため、ここではクリアしない
    //       plyが変わった場合でも、ThinkingInfoUpdatedが先に呼ばれて設定済み
    if (m_pendingPly != ply) {
        m_pendingScoreCp = std::numeric_limits<int>::min();
        m_pendingMate = 0;
        m_pendingMateText.clear();
        m_pendingPv.clear();
    }
    m_pendingPly = ply;
    if (scoreCp != std::numeric_limits<int>::min() || mate != 0 || !rawMate.isEmpty()) {
        m_pendingScoreCp = scoreCp;
        m_pendingMate = mate;
        m_pendingMateText = rawMate.isEmpty() && mate != 0 ? QString::number(mate) : rawMate;
    }
    if (!pv.isEmpty()) m_pendingPv = pv;

    qCDebug(lcAnalysis).noquote() << "updatePending: ply=" << ply << "pv=" << pv.left(30) << "pvKanji=" << m_pendingPvKanji.left(30);
}

void AnalysisResultHandler::updatePendingPvKanji(const QString& pvKanjiStr)
{
    // ThinkingInfoPresenterからの漢字PVを保存（思考タブと同じ内容を棋譜解析結果に使用）
    m_pendingPvKanji = pvKanjiStr;
    qCDebug(lcAnalysis).noquote() << "saved pvKanjiStr=" << pvKanjiStr.left(50);
}

void AnalysisResultHandler::commitPendingResult()
{
    if (!m_refs.analysisModel) {
        qCDebug(lcAnalysis).noquote() << "commitPendingResult: analysisModel is null";
        return;
    }

    // m_pendingPlyが-1の場合は定跡（infoなしでbestmoveが来た）
    // m_coordから現在のplyを取得
    int ply = m_pendingPly;
    bool isBook = false;
    if (ply < 0 && m_refs.coord) {
        ply = m_refs.coord->currentPly();
        isBook = true;
        qCDebug(lcAnalysis).noquote() << "book move detected, ply=" << ply;
    }

    if (ply < 0) {
        qCDebug(lcAnalysis).noquote() << "commitPendingResult skipped: ply=" << ply;
        return;
    }

    const int scoreCp = m_pendingScoreCp;
    const int mate = m_pendingMate;
    QString mateText = m_pendingMateText;
    const QString usiPv = sanitizeUsiPv(m_pendingPv, isBook);

    // 漢字PVがあればそれを使用、なければUSI形式PV、定跡なら「定跡」
    QString pv;
    if (isBook) {
        pv = tr("（定跡）");
    } else if (!m_pendingPvKanji.isEmpty()) {
        pv = m_pendingPvKanji;
    } else {
        pv = m_pendingPv;
    }

    // 一時保存をリセット
    m_pendingPly = -1;
    m_pendingScoreCp = 0;
    m_pendingMate = 0;
    m_pendingMateText.clear();
    m_pendingPv.clear();
    m_pendingPvKanji.clear();

    const QString moveLabel = resolveMoveLabel(m_refs, ply);

    const QString position = m_refs.sfenHistory && ply < m_refs.sfenHistory->size()
        ? SfenUtils::normalizePositionLikeSfen(m_refs.sfenHistory->at(ply)) : QString();
    const bool isGoteTurn = position.isEmpty() ? ply % 2 == 1
        : position.section(QLatin1Char(' '), 1, 1) == QStringLiteral("w");
    QString evalStr;
    const int curVal = !mateText.isEmpty() ? m_prevEvalCp : evaluateForDisplay(isGoteTurn,
                                          scoreCp,
                                          mate,
                                          isBook,
                                          m_prevEvalCp,
                                          &evalStr);
    if (!mateText.isEmpty() && !isBook) {
        if (isGoteTurn) {
            if (mateText == QStringLiteral("+")) mateText = QStringLiteral("-");
            else if (mateText == QStringLiteral("-")) mateText = QStringLiteral("+");
            else if (mateText.toLongLong() == 0) mateText = QStringLiteral("+0");
            else mateText = QString::number(-mateText.toLongLong());
        }
        evalStr = QStringLiteral("mate %1").arg(mateText);
    }
    bool hasNumericEval = false;
    evalStr.toInt(&hasNumericEval);
    const QString diff = hasNumericEval && m_hasPreviousEval
        ? QString::number(static_cast<qint64>(curVal) - m_prevEvalCp) : QStringLiteral("-");
    m_hasPreviousEval = hasNumericEval;
    if (hasNumericEval) m_prevEvalCp = curVal;

    qCDebug(lcAnalysis).noquote() << "commitPendingResult: ply=" << ply << "moveLabel=" << moveLabel << "evalStr=" << evalStr << "pv=" << pv.left(30);

    // KifuAnalysisResultsDisplay は (Move, Eval, Diff, PV) の4引数
    KifuAnalysisResultsDisplay* resultItem = new KifuAnalysisResultsDisplay(
        moveLabel,
        evalStr,
        diff,
        pv
        );

    resultItem->setUsiPv(usiPv);
    if (m_refs.sfenHistory && ply > 0 && ply <= m_refs.sfenHistory->size())
        resultItem->beforeSfen = SfenUtils::normalizePositionLikeSfen(m_refs.sfenHistory->at(ply - 1));

    // 局面SFENを設定
    if (m_refs.sfenHistory && ply >= 0 && ply < m_refs.sfenHistory->size()) {
        const QString sfen = SfenUtils::normalizePositionLikeSfen(m_refs.sfenHistory->at(ply));
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

    // GUI更新用に結果を保存（次のonPositionPreparedでシグナルを発行）
    m_lastCommittedPly = ply;
    m_lastCommittedScoreCp = hasNumericEval || !mateText.isEmpty()
        ? curVal : std::numeric_limits<int>::min();
    m_lastCommittedMate = isBook ? QString() : mateText;
}
