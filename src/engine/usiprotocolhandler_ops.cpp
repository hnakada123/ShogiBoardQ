/// @file usiprotocolhandler_ops.cpp
/// @brief UsiProtocolHandler の局所操作（checkmate/座標変換/operation context）実装

#include "usiprotocolhandler.h"
#include "usimovecoordinateconverter.h"
#include "shogigamecontroller.h"

void UsiProtocolHandler::handleCheckmateLine(const QString& line)
{
    const QString rest = line.mid(QStringLiteral("checkmate").size()).trimmed();

    if (rest.compare(QStringLiteral("nomate"), Qt::CaseInsensitive) == 0) {
        emit checkmateNoMate();
        return;
    }
    if (rest.compare(QStringLiteral("notimplemented"), Qt::CaseInsensitive) == 0) {
        emit checkmateNotImplemented();
        return;
    }
    if (rest.isEmpty() || rest.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0) {
        emit checkmateUnknown();
        return;
    }

    const QStringList pv = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (pv.isEmpty()) {
        emit checkmateUnknown();
        return;
    }
    emit checkmateSolved(pv);
}

void UsiProtocolHandler::parseMoveCoordinates(int& fileFrom, int& rankFrom,
                                              int& fileTo, int& rankTo)
{
    fileFrom = rankFrom = fileTo = rankTo = -1;
    const QString move = m_bestMove.trimmed();

    if (m_specialMove != SpecialMove::None) {
        if (m_gameController) {
            m_gameController->setPromote(false);
        }
        return;
    }

    if (move.size() < 4) {
        emit errorOccurred(tr("Invalid bestmove format: \"%1\"").arg(move));
        cancelCurrentOperation();
        return;
    }

    const bool promote = (move.size() >= 5 && move.at(4) == QLatin1Char('+'));
    if (m_gameController) {
        m_gameController->setPromote(promote);
    }

    const bool isP1 = m_gameController
        && m_gameController->currentPlayer() == ShogiGameController::Player1;

    QString errorMsg;
    if (!UsiMoveCoordinateConverter::parseMoveFrom(move, fileFrom, rankFrom, isP1, errorMsg)
        || !UsiMoveCoordinateConverter::parseMoveTo(move, fileTo, rankTo, errorMsg)) {
        emit errorOccurred(errorMsg);
        cancelCurrentOperation();
    }
}

QString UsiProtocolHandler::convertHumanMoveToUsi(const QPoint& from, const QPoint& to,
                                                  bool promote)
{
    QString errorMsg;
    QString result =
        UsiMoveCoordinateConverter::convertHumanMoveToUsi(from, to, promote, errorMsg);
    if (!errorMsg.isEmpty()) {
        emit errorOccurred(errorMsg);
    }
    return result;
}

QChar UsiProtocolHandler::rankToAlphabet(int rank)
{
    return UsiMoveCoordinateConverter::rankToAlphabet(rank);
}

std::optional<int> UsiProtocolHandler::alphabetToRank(QChar c)
{
    return UsiMoveCoordinateConverter::alphabetToRank(c);
}

quint64 UsiProtocolHandler::beginOperationContext()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    m_opCtx = new QObject(this);
    m_stopOrPonderhitPending = false;
    return ++m_seq;
}

void UsiProtocolHandler::cancelCurrentOperation()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    m_stopOrPonderhitPending = false;
    m_bestMoveReceived = false;
    m_modeTsume = false;
    ++m_seq;
}
