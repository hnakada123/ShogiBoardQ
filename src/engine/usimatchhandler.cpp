/// @file usimatchhandler.cpp
/// @brief 対局通信フロー・盤面データ管理を担当するハンドラクラスの実装

#include "usimatchhandler.h"
#include "logcategories.h"
#include "usiprotocolhandler.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include <limits>

namespace {

void ensureMovesKeyword(QString& s)
{
    if (!s.contains(QStringLiteral(" moves"))) {
        s = s.trimmed();
        s += QStringLiteral(" moves");
    }
}

} // namespace

UsiMatchHandler::UsiMatchHandler(UsiProtocolHandler* protocolHandler)
    : m_protocolHandler(protocolHandler)
{
    m_responseTimer.setSingleShot(true);
    m_responseTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_responseTimer, &QTimer::timeout, this, &UsiMatchHandler::onSearchTimeout);
}

void UsiMatchHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

QString UsiMatchHandler::lastUsiMove() const
{
    return m_lastUsiMove;
}

void UsiMatchHandler::setLastUsiMove(const QString& move)
{
    qCDebug(lcEngine) << "setLastUsiMove:" << move;
    m_lastUsiMove = move;
}

void UsiMatchHandler::cancelAsync()
{
    m_responseTimer.stop();
    m_pending = Pending::None;
}

void UsiMatchHandler::requestMove(const QString& position, const QString& ponder, const UsiTimingParams& timing)
{
    cancelAsync();
    m_position = position;
    if (searchTiming(timing).expired) return;
    m_ponder = ponder;
    m_timing = timing;
    const auto phase = m_protocolHandler->currentPhase();
    if (phase == UsiProtocolHandler::SearchPhase::Ponder
        || phase == UsiProtocolHandler::SearchPhase::StoppingPonder) {
        const bool hit = phase == UsiProtocolHandler::SearchPhase::Ponder
            && m_protocolHandler->isPonderEnabled() && !m_protocolHandler->predictedMove().isEmpty()
            && position.simplified() == ponder.simplified();
        if (!hit) {
            m_pending = Pending::PonderStop;
            m_responseTimer.start(qMin(2000, searchTiming(timing).timeoutMs));
            if (phase != UsiProtocolHandler::SearchPhase::StoppingPonder) m_protocolHandler->sendStop();
            return;
        }
        m_lastUsiMove = position.section(QLatin1Char(' '), -1);
        m_pending = Pending::Move;
        m_responseTimer.start(searchTiming(timing).timeoutMs);
        m_protocolHandler->sendPonderHit();
        return;
    }
    startAsyncSearch();
}

void UsiMatchHandler::startAsyncSearch()
{
    const auto timing = searchTiming(m_timing);
    if (timing.expired) { cancelAsync(); return; }
    if (m_position.contains(QStringLiteral(" moves ")))
        m_lastUsiMove = m_position.section(QLatin1Char(' '), -1);
    const UsiGoTime goTime = UsiGoTimeCalculator::compute(timing.input);
    m_pending = Pending::Move;
    m_responseTimer.start(timing.timeoutMs);
    m_protocolHandler->sendPosition(m_position);
    m_protocolHandler->sendGo(goTime);
}

void UsiMatchHandler::onBestMoveReceived()
{
    if (m_pending == Pending::PonderStop) {
        m_responseTimer.stop();
        startAsyncSearch();
        return;
    }
    if (m_pending != Pending::Move) return;
    m_pending = Pending::None;
    m_responseTimer.stop();
    if (m_hooks.finishTurn && !m_hooks.finishTurn()) return;
    if (m_protocolHandler->specialMove() != SpecialMove::None) return;
    const std::optional<UsiMove> move = m_protocolHandler->parseBestMove();
    if (!move) return;
    appendBestMoveAndStartPondering(m_position, m_ponder, m_timing);
    // コールバック先で次の探索や破棄が起きても、参照中の結果は変化させない。
    const auto position = m_position;
    const auto ponder = m_ponder;
    if (m_hooks.onMoveReady) m_hooks.onMoveReady(*move, position, ponder);
}

UsiMatchHandler::SearchTiming UsiMatchHandler::searchTiming(const UsiTimingParams& timing, bool pondering) const
{
    // エンジンの手番。先読みを始めるときは m_position に自分の指した手まで入っているので、その1手前の手番
    const bool whiteToMove = SfenReplay::sfenFromPositionCommand(m_position).contains(QStringLiteral(" w "));
    const Turn sideToMove = (whiteToMove != pondering) ? Turn::White : Turn::Black;
    if (m_hooks.readTiming) return m_hooks.readTiming(timing, sideToMove, pondering);
    SearchTiming result;
    result.input.timeControl = timing.timeControl;
    result.input.btimeMs = timing.btimeMs;
    result.input.wtimeMs = timing.wtimeMs;
    result.input.extraMs = timing.extraMs;
    result.input.marginMs = timing.marginMs;
    result.input.sideToMove = sideToMove;
    result.timeoutMs = static_cast<int>(qBound(qint64(1), UsiGoTimeCalculator::budgetMs(result.input),
                                              qint64(std::numeric_limits<int>::max())));
    return result;
}

void UsiMatchHandler::onSearchTimeout()
{
    cancelAsync();
    if (searchTiming(m_timing).expired) return;
    if (!m_protocolHandler->isTimeoutDeclared() && m_hooks.onBestmoveTimeout) m_hooks.onBestmoveTimeout();
}

void UsiMatchHandler::startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr,
                                                const UsiTimingParams& timing)
{
    const QString predictedMove = m_protocolHandler->predictedMove();
    if (!predictedMove.isEmpty() && m_protocolHandler->isPonderEnabled()) {
        ensureMovesKeyword(positionStr);
        positionPonderStr = positionStr + " " + predictedMove;
        m_lastUsiMove = predictedMove;
        const UsiGoTime goTime = UsiGoTimeCalculator::compute(searchTiming(timing, true).input);
        m_protocolHandler->sendPosition(positionPonderStr);
        m_protocolHandler->sendGoPonder(goTime);
    }
}

void UsiMatchHandler::appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr,
                                                     const UsiTimingParams& timing)
{
    ensureMovesKeyword(positionStr);
    positionStr += " " + m_protocolHandler->bestMove();
    startPonderingAfterBestMove(positionStr, positionPonderStr, timing);
}
