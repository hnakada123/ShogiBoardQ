/// @file usimatchhandler.cpp
/// @brief 対局での position/go/bestmove のやり取りの実装

#include "usimatchhandler.h"

#include "logcategories.h"
#include "matchsearchsnapshot.h"
#include "shogiclock.h"
#include "usiprotocolhandler.h"

#include <limits>

UsiMatchHandler::UsiMatchHandler(UsiProtocolHandler* protocolHandler, QObject* parent)
    : QObject(parent)
    , m_protocolHandler(protocolHandler)
{
    m_responseTimer.setSingleShot(true);
    m_responseTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_responseTimer, &QTimer::timeout, this, &UsiMatchHandler::onSearchTimeout);
    connect(m_protocolHandler, &UsiProtocolHandler::bestMoveReceived, this, &UsiMatchHandler::onBestMoveReceived);
}

void UsiMatchHandler::cancelAsync()
{
    m_responseTimer.stop();
    m_pending = Pending::None;
}

void UsiMatchHandler::requestMove(const UsiPosition& position, const UsiTimingParams& timing)
{
    cancelAsync();
    m_position = position;
    m_timing = timing;
    if (searchTiming(m_position.sideToMove()).expired) return;

    using State = UsiProtocolHandler::State;
    const bool pondering = m_protocolHandler->isSearching()
        && m_protocolHandler->searchKind() == UsiProtocolHandler::SearchKind::Ponder;
    if (!pondering) {
        m_ponder = {};
        startAsyncSearch();
        return;
    }
    // 先読みの予想が当たれば ponderhit で続ける。外れたら stop で回収してから探し直す
    const bool hit = m_protocolHandler->state() == State::Searching && !m_ponder.isEmpty() && position == m_ponder;
    m_ponder = {};
    if (!hit) {
        m_pending = Pending::PonderStop;
        m_responseTimer.start(qMin(2000, searchTiming(m_position.sideToMove()).timeoutMs));
        m_protocolHandler->sendStop();
        return;
    }
    m_pending = Pending::Move;
    m_responseTimer.start(searchTiming(m_position.sideToMove()).timeoutMs);
    m_protocolHandler->sendPonderHit();
}

void UsiMatchHandler::startAsyncSearch()
{
    const SearchTiming timing = searchTiming(m_position.sideToMove());
    if (timing.expired) {
        cancelAsync();
        return;
    }
    m_pending = Pending::Move;
    m_responseTimer.start(timing.timeoutMs);
    m_protocolHandler->startSearch(m_position, UsiGo::clock(UsiGoTimeCalculator::compute(timing.input)));
}

void UsiMatchHandler::onBestMoveReceived(const UsiBestMove& result)
{
    if (m_pending == Pending::PonderStop) {
        // 外れた先読みを回収した。手番の探索を始める
        m_responseTimer.stop();
        startAsyncSearch();
        return;
    }
    if (m_pending != Pending::Move || result.pondering) return;
    m_pending = Pending::None;
    m_responseTimer.stop();
    const Turn side = m_position.sideToMove();
    // 投了・宣言勝ちは時計を確定する前に知らせる（終局の処理が時計を止める）
    if (result.special == SpecialMove::Resign) {
        emit resigned(side);
        return;
    }
    if (result.special == SpecialMove::Win) {
        emit declaredWin(side);
        return;
    }
    if (!finishTurn() || !result.isMove()) return;
    const std::optional<UsiMove> move = UsiMove::parse(result.move.trimmed());
    if (!move) {
        emit errorOccurred(tr("Invalid bestmove format: \"%1\"").arg(result.move));
        return;
    }

    UsiMatchMove played;
    played.side = side;
    played.move = *move;
    played.elapsedMs = result.elapsedMs;
    played.score = result.score;
    // 指せない手（反則）かは対局の側が確かめて終局にする。指せない手の後は先読みしない
    if (const std::optional<UsiPosition> after = m_position.withMove(*move)) {
        m_position = *after;
        startPondering(result);
    } else {
        m_ponder = {};
    }
    emit moveReady(played);
}

void UsiMatchHandler::startPondering(const UsiBestMove& result)
{
    m_ponder = {};
    if (result.ponder.isEmpty() || !m_protocolHandler->isPonderEnabled()) return;
    const std::optional<UsiMove> predicted = UsiMove::parse(result.ponder.trimmed());
    const std::optional<UsiPosition> ponder = predicted ? m_position.withMove(*predicted) : std::nullopt;
    if (!ponder) return;
    m_ponder = *ponder;
    // 先読みの手番は自分の手の後（相手の手番）だが、持ち時間は自分のものを読む
    const SearchTiming timing = searchTiming(opponent(m_position.sideToMove()), true);
    m_protocolHandler->startSearch(m_ponder, UsiGo::ponder(UsiGoTimeCalculator::compute(timing.input)));
}

bool UsiMatchHandler::finishTurn()
{
    if (!m_clock) return true;
    m_clock->finishTurn();
    return !m_clock->isGameOver();
}

UsiMatchHandler::SearchTiming UsiMatchHandler::searchTiming(Turn side, bool pondering) const
{
    if (m_clock) return matchSearchSnapshot(m_clock, side, m_timing, pondering);
    SearchTiming result;
    result.input.timeControl = m_timing.timeControl;
    result.input.btimeMs = m_timing.btimeMs;
    result.input.wtimeMs = m_timing.wtimeMs;
    result.input.extraMs = m_timing.extraMs;
    result.input.marginMs = m_timing.marginMs;
    result.input.sideToMove = side;
    result.timeoutMs = static_cast<int>(qBound(qint64(1), UsiGoTimeCalculator::budgetMs(result.input),
                                              qint64(std::numeric_limits<int>::max())));
    return result;
}

void UsiMatchHandler::onSearchTimeout()
{
    cancelAsync();
    if (searchTiming(m_position.sideToMove()).expired) return;
    emit timedOut();
}
