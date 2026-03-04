/// @file analysisresulthandler_dialog.cpp
/// @brief AnalysisResultHandler のPVダイアログ・棋譜変換処理

#include "analysisresulthandler.h"

#include "analysisresultspresenter.h"
#include "kifuanalysislistmodel.h"
#include "kifuanalysisresultsdisplay.h"
#include "kifudisplay.h"
#include "kifurecordlistmodel.h"
#include "pvboarddialog.h"
#include "sfenutils.h"

#include "logcategories.h"

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
    if (baseSfen.isEmpty() && m_refs.sfenHistory && row < m_refs.sfenHistory->size()) {
        baseSfen = SfenUtils::normalizePositionLikeSfen(m_refs.sfenHistory->at(row));
    }
    if (baseSfen.isEmpty()) {
        baseSfen = SfenUtils::hirateSfen();
    }

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
    if (lastMove.isEmpty()) {
        const int ply = row;
        if (m_refs.recordModel && ply > 0 && ply < m_refs.recordModel->rowCount()) {
            if (KifuDisplay* moveDisp = m_refs.recordModel->item(ply)) {
                lastMove = extractUsiMoveFromKanji(moveDisp->currentMove());
            }
        }
    }
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
