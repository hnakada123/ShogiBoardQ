/// @file usiprotocolhandler_ops.cpp
/// @brief UsiProtocolHandler の局所操作（checkmate/座標変換/operation context）実装

#include "usiprotocolhandler.h"
#include <QPointer>
#include <QRegularExpression>
#include "thinkinginfopresenter.h"
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
    // "checkmate timeout"（時間切れ）は指し手列ではないので結果不明として扱う
    if (rest.isEmpty()
        || rest.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0
        || rest.compare(QStringLiteral("timeout"), Qt::CaseInsensitive) == 0) {
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

    auto fromCoord = UsiMoveCoordinateConverter::parseMoveFrom(move, isP1);
    auto toCoord = UsiMoveCoordinateConverter::parseMoveTo(move);
    if (!fromCoord || !toCoord) {
        emit errorOccurred(tr("Invalid bestmove coordinates: \"%1\"").arg(move));
        cancelCurrentOperation();
        return;
    }
    fileFrom = fromCoord->file;
    rankFrom = fromCoord->rank;
    fileTo = toCoord->file;
    rankTo = toCoord->rank;
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
    invalidateCandidate();
    m_initializationTimer.stop();
    m_initialization = Initialization::Idle;
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    m_stopOrPonderhitPending = false;
    m_bestMoveReceived = false;
    m_phase = SearchPhase::Idle;
    m_predictedOpponentMove.clear();
    ++m_seq;
}

void UsiProtocolHandler::initializeEngineAsync(int timeoutMs)
{
    cancelCurrentOperation();
    clearHardTimeout();
    m_reportedOptions.clear();
    m_initializationTimeoutMs = qMax(1, timeoutMs);
    m_initialization = Initialization::UsiOk;
    m_initializationTimer.start(m_initializationTimeoutMs);
    sendUsi();
}

void UsiProtocolHandler::onInitializationUsiOk()
{
    if (m_initialization != Initialization::UsiOk) return;
    m_initialization = Initialization::ReadyOk;
    sendConfiguredOptions();
    m_initializationTimer.start(m_initializationTimeoutMs);
    sendIsReady();
}

void UsiProtocolHandler::onInitializationReadyOk()
{
    if (m_initialization != Initialization::ReadyOk) return;
    m_initialization = Initialization::Idle;
    m_initializationTimer.stop();
    sendUsiNewGame();
    emit initializationFinished(true);
}

void UsiProtocolHandler::onInitializationTimeout()
{
    const auto state = m_initialization;
    if (state == Initialization::Idle) return;
    cancelCurrentOperation();
    const QPointer<UsiProtocolHandler> guard(this);
    emit errorOccurred(state == Initialization::UsiOk ? tr("Timeout waiting for usiok")
                                                     : tr("Timeout waiting for readyok"));
    if (guard) emit initializationFinished(false);
}

void UsiProtocolHandler::beginCandidateSearch(bool pondering)
{
    m_searchCandidate = {};
    m_searchCandidate.active = true;
    m_searchCandidate.pondering = pondering;
    if (m_presenter) m_searchCandidate.baseSfen = m_presenter->baseSfen();
    if (pondering) m_searchCandidate.predictedMove = m_predictedOpponentMove;
    emit searchCandidateChanged();
}

void UsiProtocolHandler::invalidateCandidate()
{
    m_searchCandidate = {};
    emit searchCandidateChanged();
}

void UsiProtocolHandler::updateCandidate(const QString& line)
{
    if (!m_searchCandidate.active || m_activeSearchSeq != m_seq) return;
    const auto tokens = line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    // info string 内の文字列をPVとして解釈しない。
    const auto pv = tokens.indexOf(QStringLiteral("pv"));
    const auto infoString = tokens.indexOf(QStringLiteral("string"));
    if (pv < 0 || pv + 1 >= tokens.size() || (infoString >= 0 && infoString < pv)) return;
    const auto multipv = tokens.indexOf(QStringLiteral("multipv"));
    if (multipv >= 0 && (multipv + 1 >= tokens.size() || tokens.at(multipv + 1) != QLatin1String("1"))) return;
    const QString move = tokens.at(pv + 1);
    static const QRegularExpression validMove(QStringLiteral("^(?:[1-9][a-i][1-9][a-i]\\+?|[PLNSGBR]\\*[1-9][a-i])$"));
    if (!validMove.match(move).hasMatch() || move == m_searchCandidate.move) return;
    m_searchCandidate.move = move;
    emit searchCandidateChanged();
}
