/// @file usimatchhandler.cpp
/// @brief 対局での position/go/bestmove のやり取りの実装

#include "usimatchhandler.h"

#include "logcategories.h"
#include "usienginesession.h"
#include "usiprotocolhandler.h"

UsiMatchHandler::UsiMatchHandler(UsiEngineSession* session, QObject* parent)
    : QObject(parent)
    , m_session(session)
    , m_protocolHandler(session->handler())
{
    m_responseTimer.setSingleShot(true);
    m_responseTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_responseTimer, &QTimer::timeout, this, &UsiMatchHandler::onSearchTimeout);
    m_bookTimer.setSingleShot(true);
    m_bookTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_bookTimer, &QTimer::timeout, this, &UsiMatchHandler::onBookMoveTimer);
    connect(m_protocolHandler, &UsiProtocolHandler::bestMoveReceived, this, &UsiMatchHandler::onBestMoveReceived);
}

void UsiMatchHandler::cancelAsync()
{
    m_responseTimer.stop();
    m_bookTimer.stop();
    m_pending = Pending::None;
}

void UsiMatchHandler::requestMove(const UsiPosition& position)
{
    cancelAsync();
    m_position = position;
    if (searchTiming(m_position.sideToMove()).expired) return;

    // 先読みの予想が当たれば ponderhit で続ける
    const bool hit = m_protocolHandler->state() == UsiProtocolHandler::State::Searching
        && m_protocolHandler->searchKind() == UsiProtocolHandler::SearchKind::Ponder
        && !m_ponder.isEmpty() && position == m_ponder;
    m_ponder = {};
    if (hit) {
        m_pending = Pending::Move;
        m_responseTimer.start(searchTiming(m_position.sideToMove()).timeoutMs);
        m_protocolHandler->sendPonderHit();
        return;
    }
    // 外れた先読みは、探索の部品が stop で回収して結果を捨ててから探し直す
    startAsyncSearch();
}

void UsiMatchHandler::requestBookMove(const UsiPosition& position, const UsiMove& move, int delayMs)
{
    // 持ち時間に対して、待つ時間はこれだけ残す（時計の精算と盤への反映の間に時間切れにならないように）
    constexpr int kTimeMarginMs = 500;
    cancelAsync();
    m_position = position;
    const SearchTiming timing = searchTiming(m_position.sideToMove());
    if (timing.expired) return;
    // 先読みしていれば止め、返ってくる bestmove は捨てる
    m_session->cancelSearch();
    m_ponder = {};
    m_bookMove = move;
    m_pending = Pending::BookMove;
    m_bookElapsed.start();
    m_bookTimer.start(qBound(0, timing.timeoutMs - kTimeMarginMs, delayMs));
}

bool UsiMatchHandler::playBookMoveNow()
{
    if (m_pending != Pending::BookMove) return false;
    m_bookTimer.stop();
    onBookMoveTimer();
    return true;
}

void UsiMatchHandler::onBookMoveTimer()
{
    if (m_pending != Pending::BookMove) return;
    m_pending = Pending::None;
    const Turn side = m_position.sideToMove();
    if (!finishTurn()) return;
    UsiMatchMove played;
    played.side = side;
    played.move = m_bookMove;
    played.elapsedMs = m_bookElapsed.elapsed();
    if (const std::optional<UsiPosition> after = m_position.withMove(m_bookMove)) m_position = *after;
    emit moveReady(played);
}

void UsiMatchHandler::startAsyncSearch()
{
    const Turn side = m_position.sideToMove();
    const SearchTiming timing = searchTiming(side);
    if (timing.expired) {
        cancelAsync();
        return;
    }
    m_pending = Pending::Move;
    // 持ち時間は依頼したときから数える（先読みの停止を待つ間も時計は進む）
    m_responseTimer.start(timing.timeoutMs);
    UsiSearchRequest request;
    request.position = m_position;
    // go の時間は送るときの残り時間で決める
    request.goAtStart = [this, side]() { return UsiGo::clock(UsiGoTimeCalculator::compute(searchTiming(side).input)); };
    if (!m_session->search(request)) cancelAsync();
}

void UsiMatchHandler::onBestMoveReceived(const UsiBestMove& result)
{
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
    UsiSearchRequest request;
    request.position = m_ponder;
    request.go = UsiGo::ponder(UsiGoTimeCalculator::compute(timing.input));
    m_session->search(request);
}

bool UsiMatchHandler::finishTurn()
{
    return !m_timeSource || m_timeSource->finishTurn();
}

UsiMatchHandler::SearchTiming UsiMatchHandler::searchTiming(Turn side, bool pondering) const
{
    return (m_timeSource ? static_cast<const MatchTimeSource&>(*m_timeSource) : m_noTime).snapshot(side, pondering);
}

void UsiMatchHandler::onSearchTimeout()
{
    cancelAsync();
    if (searchTiming(m_position.sideToMove()).expired) return;
    emit timedOut();
}
