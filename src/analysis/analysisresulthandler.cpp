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
const QString kSenteMark = QStringLiteral("▲"); // clazy:exclude=non-pod-global-static
const QString kGoteMark = QStringLiteral("△"); // clazy:exclude=non-pod-global-static
const QString kDoWithSpace = QStringLiteral("同　"); // clazy:exclude=non-pod-global-static
const QString kDoWithoutSpace = QStringLiteral("同"); // clazy:exclude=non-pod-global-static

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
    return usiPv;
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
            return AnalysisResultHandler::extractUsiMoveFromKanji(moveDisp->currentMove());
        }
    }
    return QString();
}

int evaluateForDisplay(int ply,
                       int scoreCp,
                       int mate,
                       bool isBook,
                       int prevEvalCp,
                       QString* outEvalStr)
{
    if (!outEvalStr) {
        return prevEvalCp;
    }

    const bool isGoteTurn = (ply % 2 == 1);
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

    *outEvalStr = QStringLiteral("0");
    return 0;
}

QString extractDestination(const QString& moveText)
{
    qsizetype markPos = moveText.indexOf(kSenteMark);
    if (markPos < 0) {
        markPos = moveText.indexOf(kGoteMark);
    }
    if (markPos < 0) {
        return QString();
    }

    const QString afterMark = moveText.mid(markPos + 1);
    if (afterMark.startsWith(kDoWithoutSpace) || afterMark.size() < 2) {
        return QString();
    }
    return afterMark.left(2);
}

QString extractCandidateFirstMove(const QString& pvText)
{
    if (pvText.startsWith(kSenteMark) || pvText.startsWith(kGoteMark)) {
        const qsizetype sentePos = pvText.indexOf(kSenteMark, 1);
        const qsizetype gotePos = pvText.indexOf(kGoteMark, 1);
        qsizetype nextMark = -1;
        if (sentePos > 0 && gotePos > 0) {
            nextMark = qMin(sentePos, gotePos);
        } else if (sentePos > 0) {
            nextMark = sentePos;
        } else if (gotePos > 0) {
            nextMark = gotePos;
        }
        if (nextMark > 0) {
            return pvText.left(nextMark);
        }
    }
    return pvText;
}

QString normalizeDoNotation(QString moveText)
{
    if (moveText.contains(kDoWithoutSpace) && !moveText.contains(kDoWithSpace)) {
        moveText.replace(kDoWithoutSpace, kDoWithSpace);
    }
    return moveText;
}

QString buildCandidateMoveFromPrevious(const KifuAnalysisResultsDisplay* prevItem)
{
    if (!prevItem) {
        return QString();
    }

    const QString prevPv = prevItem->principalVariation();
    if (prevPv.isEmpty() || prevPv == AnalysisResultHandler::tr("（定跡）")) {
        return QString();
    }

    QString candidateMove = extractCandidateFirstMove(prevPv);
    if (candidateMove.isEmpty()) {
        return QString();
    }

    const QString prevDestination = extractDestination(prevItem->currentMove());
    const QString candDestination = extractDestination(candidateMove);
    if (!prevDestination.isEmpty() &&
        !candDestination.isEmpty() &&
        prevDestination == candDestination) {
        qsizetype markPos = candidateMove.indexOf(kSenteMark);
        if (markPos < 0) {
            markPos = candidateMove.indexOf(kGoteMark);
        }
        if (markPos >= 0 && candidateMove.size() >= markPos + 3) {
            const QString prefix = candidateMove.left(markPos + 1);
            const QString suffix = candidateMove.mid(markPos + 3);
            candidateMove = prefix + kDoWithSpace + suffix;
        }
    }

    return normalizeDoNotation(candidateMove);
}
} // namespace

void AnalysisResultHandler::setRefs(const Refs& refs)
{
    m_refs = refs;
}

void AnalysisResultHandler::reset()
{
    m_prevEvalCp = 0;
    m_pendingPly = -1;
    m_pendingScoreCp = 0;
    m_pendingMate = 0;
    m_pendingPv.clear();
    m_pendingPvKanji.clear();
    m_lastCommittedPly = -1;
    m_lastCommittedScoreCp = 0;
}

void AnalysisResultHandler::updatePending(int ply, int scoreCp, int mate, const QString& pv)
{
    // 結果を一時保存（bestmove時に確定）
    // 最新のinfo行で更新し続ける
    // 注意: m_pendingPvKanjiはupdatePendingPvKanjiで設定されるため、ここではクリアしない
    //       plyが変わった場合でも、ThinkingInfoUpdatedが先に呼ばれて設定済み
    m_pendingPly = ply;
    m_pendingScoreCp = scoreCp;
    m_pendingMate = mate;
    m_pendingPv = pv;

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
    m_pendingPv.clear();
    m_pendingPvKanji.clear();

    const QString moveLabel = resolveMoveLabel(m_refs, ply);

    QString evalStr;
    const int curVal = evaluateForDisplay(ply,
                                          scoreCp,
                                          mate,
                                          isBook,
                                          m_prevEvalCp,
                                          &evalStr);
    const QString diff = isBook ? QStringLiteral("-") : QString::number(curVal - m_prevEvalCp);
    m_prevEvalCp = curVal;

    qCDebug(lcAnalysis).noquote() << "commitPendingResult: ply=" << ply << "moveLabel=" << moveLabel << "evalStr=" << evalStr << "pv=" << pv.left(30);

    // KifuAnalysisResultsDisplay は (Move, Eval, Diff, PV) の4引数
    KifuAnalysisResultsDisplay* resultItem = new KifuAnalysisResultsDisplay(
        moveLabel,
        evalStr,
        diff,
        pv
        );

    resultItem->setUsiPv(usiPv);

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
            const QString candidateMove = buildCandidateMoveFromPrevious(prevItem);
            if (!candidateMove.isEmpty()) {
                resultItem->setCandidateMove(candidateMove);
                qCDebug(lcAnalysis).noquote() << "setCandidateMove:" << candidateMove;
            }
        }
    }

    m_refs.analysisModel->appendItem(resultItem);

    // GUI更新用に結果を保存（次のonPositionPreparedでシグナルを発行）
    m_lastCommittedPly = ply;
    m_lastCommittedScoreCp = curVal;
}
