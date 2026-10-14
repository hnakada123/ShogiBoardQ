/// @file usiprotocolhandler_ops.cpp
/// @brief UsiProtocolHandler の局所操作（checkmate/座標変換/operation context）実装

#include "usiprotocolhandler.h"
#include <QPointer>
#include <QRegularExpression>

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

std::optional<UsiMove> UsiProtocolHandler::parseBestMove()
{
    if (m_specialMove != SpecialMove::None) return std::nullopt;

    const QString move = m_bestMove.trimmed();
    const std::optional<UsiMove> parsed = UsiMove::parse(move);
    if (!parsed) {
        emit errorOccurred(tr("Invalid bestmove format: \"%1\"").arg(move));
        cancelCurrentOperation();
    }
    return parsed;
}

quint64 UsiProtocolHandler::beginOperationContext()
{
    return ++m_seq;
}

void UsiProtocolHandler::cancelCurrentOperation()
{
    invalidateCandidate();
    m_initializationTimer.stop();
    m_initialization = Initialization::Idle;
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
    m_reportedOptionLines.clear();
    m_reportedEngineName.clear();
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
    m_searchCandidate.baseSfen = m_baseSfen;
    if (pondering) m_searchCandidate.predictedMove = m_predictedOpponentMove;
    emit searchCandidateChanged();
}

void UsiProtocolHandler::invalidateCandidate()
{
    m_searchCandidate = {};
    emit searchCandidateChanged();
}

void UsiProtocolHandler::updateCandidate(const UsiInfo& info)
{
    if (!m_searchCandidate.active || m_activeSearchSeq != m_seq) return;
    // 候補は最善（multipv 1）の読み筋の初手。info string の中身はパーサが読み筋にしない
    if (info.pv.isEmpty() || info.multipv != 1) return;
    const QString& move = info.pv.first();
    static const QRegularExpression validMove(QStringLiteral("^(?:[1-9][a-i][1-9][a-i]\\+?|[PLNSGBR]\\*[1-9][a-i])$"));
    if (!validMove.match(move).hasMatch() || move == m_searchCandidate.move) return;
    m_searchCandidate.move = move;
    emit searchCandidateChanged();
}
