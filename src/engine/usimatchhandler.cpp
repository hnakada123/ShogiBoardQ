/// @file usimatchhandler.cpp
/// @brief 対局通信フロー・盤面データ管理を担当するハンドラクラスの実装

#include "usimatchhandler.h"
#include "logcategories.h"
#include "shogiboard.h"
#include "shogigamecontroller.h"
#include "thinkinginfopresenter.h"
#include "usiprotocolhandler.h"
#include "shogiclock.h"
#include <limits>

namespace {

void ensureMovesKeyword(QString& s)
{
    if (!s.contains(QStringLiteral(" moves"))) {
        s = s.trimmed();
        s += QStringLiteral(" moves");
    }
}

/// USI形式の指し手を ShogiBoard 上に適用するローカルヘルパ（読み取れない指し手は無視する）
void applyUsiMoveToBoard(ShogiBoard* board, const QString& usiMove, bool isSenteMove)
{
    if (const auto move = UsiMove::parse(usiMove)) {
        board->applyUsiMove(*move, isSenteMove ? Turn::Black : Turn::White);
    }
}

} // anonymous namespace

// ============================================================
// 構築
// ============================================================

UsiMatchHandler::UsiMatchHandler(UsiProtocolHandler* protocolHandler,
                                 ThinkingInfoPresenter* presenter,
                                 ShogiGameController* gameController)
    : m_protocolHandler(protocolHandler)
    , m_presenter(presenter)
    , m_gameController(gameController)
{
    m_responseTimer.setSingleShot(true);
    // 時計の期限より先に発火して、通信エラーと誤判定することを防ぐ。
    m_responseTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_responseTimer, &QTimer::timeout, this, &UsiMatchHandler::onSearchTimeout);
}

void UsiMatchHandler::setHooks(const Hooks& hooks)
{
    m_hooks = hooks;
}

// ============================================================
// 盤面データ管理
// ============================================================

QString UsiMatchHandler::computeBaseSfenFromBoard() const
{
    if (!m_gameController || !m_gameController->board()) return QString();

    ShogiBoard* board = m_gameController->board();
    // board->currentPlayer() は setSfen() 時に SFEN の手番フィールドから設定されるため、
    // 棋譜ナビゲーション後も正しい手番を返す。
    // m_gameController->currentPlayer() は対局中の手番管理用であり、
    // ナビゲーション時には更新されないため使用しない。
    const Turn boardTurn = board->currentPlayer();
    const QString turn = turnToSfen(boardTurn);
    return board->convertBoardToSfen() + QStringLiteral(" ") + turn +
           QStringLiteral(" ") + board->convertStandToSfen() + QStringLiteral(" 1");
}

// ============================================================
// 最終指し手管理
// ============================================================

QString UsiMatchHandler::lastUsiMove() const
{
    return m_lastUsiMove;
}

void UsiMatchHandler::setLastUsiMove(const QString& move)
{
    qCDebug(lcEngine) << "setLastUsiMove:" << move;
    m_lastUsiMove = move;
}

// ============================================================
// 盤面処理（内部）
// ============================================================

void UsiMatchHandler::updateBaseSfenForPonder()
{
    QString currentSfen = m_presenter->baseSfen();
    if (currentSfen.isEmpty()) return;

    // 現在のbaseSfenから手番を取得
    QStringList sfenParts = currentSfen.split(QLatin1Char(' '));
    if (sfenParts.size() < 2) return;
    bool isSente = (sfenParts.at(1) == QStringLiteral("b"));

    // 一時的なShogiBoardを作成し、現在のbaseSfenを設定
    ShogiBoard tempBoard;
    tempBoard.setSfen(currentSfen);

    // bestmove（現在の手番のプレイヤーの指し手）を適用
    applyUsiMoveToBoard(&tempBoard, m_protocolHandler->bestMove(), isSente);

    // predictedMove（相手の予測手）を適用
    applyUsiMoveToBoard(&tempBoard, m_protocolHandler->predictedMove(), !isSente);

    // ポンダー局面のSFENを生成（2手適用後なので手番は元と同じ）
    QString ponderTurn = isSente ? QStringLiteral("b") : QStringLiteral("w");
    QString ponderBaseSfen = tempBoard.convertBoardToSfen() + QStringLiteral(" ") + ponderTurn +
                             QStringLiteral(" ") + tempBoard.convertStandToSfen() + QStringLiteral(" 1");

    qCDebug(lcEngine) << "updateBaseSfenForPonder:" << ponderBaseSfen.left(80);
    m_presenter->setBaseSfen(ponderBaseSfen);
}

// ============================================================
// 対局通信処理
// ============================================================

void UsiMatchHandler::cancelAsync()
{
    m_responseTimer.stop();
    m_pending = Pending::None;
    m_acceptBestMove = false;
}

void UsiMatchHandler::requestMove(const QString& position, const QString& ponder, const UsiTimingParams& timing)
{
    cancelAsync();
    if (!m_gameController) return;
    if (m_clock) m_clock->updateClock();
    if (m_clock && m_clock->isGameOver()) return;
    m_position = position;
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
            m_responseTimer.start(qMin(2000, remainingTimeMs(timing)));
            if (phase != UsiProtocolHandler::SearchPhase::StoppingPonder) m_protocolHandler->sendStop();
            return;
        }
        m_presenter->setBaseSfen(computeBaseSfenFromBoard());
        m_lastUsiMove = position.section(QLatin1Char(' '), -1);
        m_pending = Pending::Move;
        m_acceptBestMove = true;
        m_responseTimer.start(remainingTimeMs(timing));
        m_protocolHandler->sendPonderHit();
        return;
    }
    startAsyncSearch();
}

void UsiMatchHandler::startAsyncSearch()
{
    if (m_clock) m_clock->updateClock();
    if (m_clock && m_clock->isGameOver()) { cancelAsync(); return; }
    m_presenter->setBaseSfen(computeBaseSfenFromBoard());
    if (m_position.contains(QStringLiteral(" moves ")))
        m_lastUsiMove = m_position.section(QLatin1Char(' '), -1);
    const UsiGoTime goTime = UsiGoTimeCalculator::compute(goTimeInput(m_timing, false));
    m_pending = Pending::Move;
    m_acceptBestMove = true;
    m_responseTimer.start(remainingTimeMs(m_timing));
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
    if (m_acceptBestMove && m_clock) m_clock->finishTurn();
    m_acceptBestMove = false;
    if (m_pending != Pending::Move) return;
    m_pending = Pending::None;
    m_responseTimer.stop();
    if (m_clock && m_clock->isGameOver()) return;
    if (m_protocolHandler->specialMove() != SpecialMove::None) return;
    const std::optional<UsiMove> move = m_protocolHandler->parseBestMove();
    if (!move) return;
    appendBestMoveAndStartPondering(m_position, m_ponder, m_timing);
    // コールバック先で次の探索や破棄が起きても、参照中の結果は変化させない。
    const auto position = m_position;
    const auto ponder = m_ponder;
    if (m_hooks.onMoveReady) m_hooks.onMoveReady(*move, position, ponder);
}

int UsiMatchHandler::remainingTimeMs(const UsiTimingParams& timing) const
{
    const Turn side = m_gameController->currentPlayer();
    qint64 budget = 0;
    if (m_clock) {
        budget = m_clock->enforcesTimeout() ? m_clock->remainingTurnTimeMs(side) : kUnlimitedSearchTimeoutMs;
    } else {
        budget = UsiGoTimeCalculator::budgetMs(goTimeInput(timing, false));
    }
    return static_cast<int>(qBound(qint64(1), budget, qint64(std::numeric_limits<int>::max())));
}

void UsiMatchHandler::onSearchTimeout()
{
    cancelAsync();
    if (m_clock) m_clock->updateClock();
    if (m_clock && m_clock->isGameOver()) return;
    if (!m_protocolHandler->isTimeoutDeclared() && m_hooks.onBestmoveTimeout) m_hooks.onBestmoveTimeout();
}

UsiGoTimeInput UsiMatchHandler::goTimeInput(const UsiTimingParams& timing, bool pondering) const
{
    const Turn side = m_gameController->currentPlayer();
    UsiGoTimeInput input;
    input.sideToMove = side;
    input.extraMs = timing.extraMs;
    input.marginMs = timing.marginMs;
    if (!m_clock) {
        input.timeControl = timing.timeControl;
        input.btimeMs = timing.btimeMs;
        input.wtimeMs = timing.wtimeMs;
        return input;
    }
    // 時計の残り時間には、次の手番で使う加算が既に含まれている。USI の btime/wtime の意味に
    // するため差し引く。先読みでは、今の手の確定による手番側の加算がまだ足されていないので引かない。
    input.timeControl = m_clock->timeControl();
    const TimeControl tc = input.timeControl.normalized();
    const auto usiRemaining = [this, &tc, pondering, side](Turn player) -> qint64 {
        const qint64 increment = tc.usesByoyomi() ? 0 : tc.side(player).incrementMs;
        const bool incrementPending = pondering && player == side;
        return qMax<qint64>(0, m_clock->remainingMainTimeMs(player) - (incrementPending ? 0 : increment));
    };
    input.btimeMs = usiRemaining(Turn::Black);
    input.wtimeMs = usiRemaining(Turn::White);
    if (!pondering) {
        // stop待ち、局面準備、最後のtick以降も含めた、手番側の秒読みの残り
        input.moverByoyomiMs = qMax<qint64>(0, m_clock->remainingTurnTimeMs(side) - m_clock->remainingMainTimeMs(side));
    }
    return input;
}

void UsiMatchHandler::startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr,
                                                const UsiTimingParams& timing)
{
    const QString predictedMove = m_protocolHandler->predictedMove();
    if (!predictedMove.isEmpty() && m_protocolHandler->isPonderEnabled()) {
        ensureMovesKeyword(positionStr);
        positionPonderStr = positionStr + " " + predictedMove;
        updateBaseSfenForPonder();
        m_lastUsiMove = predictedMove;
        const UsiGoTime goTime = UsiGoTimeCalculator::compute(goTimeInput(timing, true));
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
