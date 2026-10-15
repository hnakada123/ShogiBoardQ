/// @file analysisresulthandler.cpp
/// @brief 解析結果ハンドラクラスの実装

#include "analysisresulthandler.h"

#include "analysiscoordinator.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisresultsdisplay.h"
#include "sfenutils.h"

#include <limits>
#include <QString>
#include <QStringList>

#include "logcategories.h"
#include "analysisresultspresenter.h"
#include "pvboarddialog.h"

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

    const QString position = m_refs.line && ply <= m_refs.line->moveCount()
        ? SfenUtils::normalizePositionLikeSfen(m_refs.line->sfenAt(ply)) : QString();
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

    // GUI更新用に結果を保存（次のonPositionPreparedでシグナルを発行）
    m_lastCommittedPly = ply;
    m_lastCommittedScoreCp = hasNumericEval || !mateText.isEmpty()
        ? curVal : std::numeric_limits<int>::min();
    m_lastCommittedMate = isBook ? QString() : mateText;
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

QString AnalysisResultHandler::extractUsiMoveFromKanji(const QString& kanjiMove)
{
    // 形式例:
    //   「▲７六歩(77)」 -> "7g7f"
    //   「▲５五角打」 -> "B*5e"
    //   「▲３三歩成(34)」 -> "3d3c+"
    if (kanjiMove.isEmpty()) {
        return QString();
    }

    static const QString senteMark = QStringLiteral("▲");
    static const QString goteMark = QStringLiteral("△");

    qsizetype markPos = kanjiMove.indexOf(senteMark);
    if (markPos < 0) {
        markPos = kanjiMove.indexOf(goteMark);
    }
    if (markPos < 0 || kanjiMove.length() <= markPos + 2) {
        return QString();
    }

    const QString afterMark = kanjiMove.mid(markPos + 1);
    if (afterMark.startsWith(QStringLiteral("同"))) {
        qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: 同 notation, cannot extract USI move";
        return QString();
    }

    const bool isDrop = afterMark.contains(QStringLiteral("打"));
    const bool isPromotion = afterMark.contains(QStringLiteral("成")) &&
                             !afterMark.contains(QStringLiteral("不成"));

    const QChar fileChar = afterMark.at(0);
    const QChar rankChar = afterMark.at(1);

    int fileTo = 0;
    if (fileChar >= QChar(0xFF11) && fileChar <= QChar(0xFF19)) {
        fileTo = fileChar.unicode() - 0xFF11 + 1;
    }

    int rankTo = 0;
    static const QString kanjiRanks = QStringLiteral("一二三四五六七八九");
    const qsizetype rankIdx = kanjiRanks.indexOf(rankChar);
    if (rankIdx >= 0) {
        rankTo = static_cast<int>(rankIdx) + 1;
    }

    if (fileTo < 1 || fileTo > 9 || rankTo < 1 || rankTo > 9) {
        qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: invalid destination:" << fileTo << rankTo;
        return QString();
    }

    const QChar toRankAlpha = QChar('a' + rankTo - 1);

    if (isDrop) {
        static const QString pieceChars = QStringLiteral("歩香桂銀金角飛");
        static const QString usiPieces = QStringLiteral("PLNSGBR");

        QChar pieceUsi;
        for (qsizetype i = 0; i < pieceChars.size(); ++i) {
            if (afterMark.contains(pieceChars.at(i))) {
                pieceUsi = usiPieces.at(i);
                break;
            }
        }

        if (pieceUsi.isNull()) {
            qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: could not identify piece for drop";
            return QString();
        }

        return QStringLiteral("%1*%2%3").arg(pieceUsi).arg(fileTo).arg(toRankAlpha);
    }

    const qsizetype parenStart = afterMark.indexOf(QLatin1Char('('));
    const qsizetype parenEnd = afterMark.indexOf(QLatin1Char(')'));
    if (parenStart < 0 || parenEnd < 0 || parenEnd <= parenStart + 1) {
        qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: could not find source position in parentheses";
        return QString();
    }

    const QString srcStr = afterMark.mid(parenStart + 1, parenEnd - parenStart - 1);
    if (srcStr.length() != 2) {
        qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: invalid source string:" << srcStr;
        return QString();
    }

    const int fileFrom = srcStr.at(0).digitValue();
    const int rankFrom = srcStr.at(1).digitValue();
    if (fileFrom < 1 || fileFrom > 9 || rankFrom < 1 || rankFrom > 9) {
        qCDebug(lcAnalysis).noquote() << "extractUsiMoveFromKanji: invalid source coordinates:" << fileFrom << rankFrom;
        return QString();
    }

    const QChar fromRankAlpha = QChar('a' + rankFrom - 1);
    QString usiMove = QStringLiteral("%1%2%3%4")
                          .arg(fileFrom)
                          .arg(fromRankAlpha)
                          .arg(fileTo)
                          .arg(toRankAlpha);
    if (isPromotion) {
        usiMove += QLatin1Char('+');
    }
    return usiMove;
}
