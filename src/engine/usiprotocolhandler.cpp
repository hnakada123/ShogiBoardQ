/// @file usiprotocolhandler.cpp
/// @brief USI エンジン1台の状態とコマンドの送受信の実装

#include "usiprotocolhandler.h"

#include "engineprocessmanager.h"
#include "logcategories.h"
#include "usilineparser.h"

#include <QPointer>

UsiProtocolHandler::UsiProtocolHandler(EngineProcessManager* process, QObject* parent)
    : QObject(parent)
    , m_process(process)
{
    m_initializationTimer.setSingleShot(true);
    connect(&m_initializationTimer, &QTimer::timeout, this, &UsiProtocolHandler::onInitializationTimeout);
    if (m_process) connect(m_process, &EngineProcessManager::dataReceived, this, &UsiProtocolHandler::onDataReceived);
}

UsiProtocolHandler::~UsiProtocolHandler() = default;

void UsiProtocolHandler::setState(State state)
{
    m_state = state;
}

// ============================================================
// 起動と初期化
// ============================================================

void UsiProtocolHandler::beginStartup()
{
    cancelCurrentOperation();
    setState(State::Starting);
}

void UsiProtocolHandler::initializeEngineAsync(int timeoutMs, bool untilUsiOk)
{
    cancelCurrentOperation();
    m_untilUsiOk = untilUsiOk;
    m_reportedOptions.clear();
    m_reportedOptionLines.clear();
    m_reportedEngineName.clear();
    m_reportedEngineAuthor.clear();
    m_initializationTimeoutMs = qMax(1, timeoutMs);
    setState(State::Handshaking);
    m_handshakeStep = HandshakeStep::UsiOk;
    m_initializationTimer.start(m_initializationTimeoutMs);
    sendUsi();
}

void UsiProtocolHandler::markStopped()
{
    cancelCurrentOperation();
    setState(State::NotStarted);
}

void UsiProtocolHandler::sendConfiguredOptions()
{
    // 設定ファイルに保存されていても、エンジンが報告していないオプションは送らない
    for (const EngineProfileStore::LaunchOptions::Option& option : std::as_const(m_launchOptions.options)) {
        if (!m_reportedOptions.contains(option.name)) {
            qCDebug(lcEngine) << "Skipping unsupported option:" << option.name;
            continue;
        }
        if (option.button) sendCommand(QStringLiteral("setoption name ") + option.name);
        else sendSetOption(option.name, option.value);
    }

    // この接続だけの設定は、保存済みの設定の後に送って上書きする
    for (const auto& option : std::as_const(m_sessionOptions)) {
        if (m_reportedOptions.contains(option.first)) sendSetOption(option.first, option.second);
    }

    // USI_Ponder は報告を省略できる予約オプション。送るかどうかと GUI の先読みの許可は別
    const QString ponder = QLatin1String(EngineProfileStore::kPonderOptionName);
    if (m_launchOptions.sendUnreportedPonder || m_reportedOptions.contains(ponder)) {
        sendSetOption(ponder, m_launchOptions.ponderEnabled ? QStringLiteral("true") : QStringLiteral("false"));
    }
}

void UsiProtocolHandler::handleUsiOk()
{
    if (m_state != State::Handshaking || m_handshakeStep != HandshakeStep::UsiOk) return;
    if (m_untilUsiOk) {
        m_initializationTimer.stop();
        setState(State::Ready);
        emit initializationFinished(true);
        return;
    }
    m_handshakeStep = HandshakeStep::ReadyOk;
    sendConfiguredOptions();
    m_initializationTimer.start(m_initializationTimeoutMs);
    sendIsReady();
}

void UsiProtocolHandler::handleReadyOk()
{
    if (m_state != State::Handshaking || m_handshakeStep != HandshakeStep::ReadyOk) return;
    m_initializationTimer.stop();
    setState(State::Ready);
    sendUsiNewGame();
    emit initializationFinished(true);
}

void UsiProtocolHandler::onInitializationTimeout()
{
    if (m_state != State::Handshaking) return;
    const bool waitingUsiOk = m_handshakeStep == HandshakeStep::UsiOk;
    cancelCurrentOperation();
    const QPointer<UsiProtocolHandler> guard(this);
    emit errorOccurred(waitingUsiOk ? tr("Timeout waiting for usiok") : tr("Timeout waiting for readyok"));
    if (guard) emit initializationFinished(false);
}

std::optional<EngineOption> UsiProtocolHandler::reportedOption(const QString& name) const
{
    const auto it = m_reportedOptions.constFind(name);
    if (it == m_reportedOptions.cend()) return std::nullopt;
    return *it;
}

// ============================================================
// 探索
// ============================================================

bool UsiProtocolHandler::startSearch(const UsiPosition& position, const UsiGo& go)
{
    // 局面を送らない go は前の局面を探索してしまうので送らない
    if (position.isEmpty()) {
        qCWarning(lcEngine) << "startSearch: no position to search";
        return false;
    }
    m_baseSfen = position.sfen();
    emit positionChanged(position);
    sendCommand(position.command());

    m_activeSearchSeq = ++m_seq;
    m_searchScore = {};
    m_searchKind = go.kind == UsiGo::Kind::Ponder ? SearchKind::Ponder
        : go.kind == UsiGo::Kind::Mate ? SearchKind::Mate : SearchKind::Main;
    setState(State::Searching);
    emit searchStarted();
    m_goTimer.start();
    sendCommand(go.command());
    return true;
}

void UsiProtocolHandler::sendStop()
{
    if (m_state != State::Searching) return;
    setState(State::Stopping);
    sendCommand(QStringLiteral("stop"));
}

void UsiProtocolHandler::abandonSearch()
{
    if (!isSearching()) return;
    // 結果は取り消した探索のものとして捨てる（finishSearch で待機に戻り、searchEnded を出す）
    ++m_seq;
    if (m_state == State::Searching) {
        setState(State::Stopping);
        sendCommand(QStringLiteral("stop"));
    }
}

void UsiProtocolHandler::sendPonderHit()
{
    if (m_state != State::Searching || m_searchKind != SearchKind::Ponder) return;
    m_searchKind = SearchKind::Main;
    m_goTimer.start();
    sendCommand(QStringLiteral("ponderhit"));
}

bool UsiProtocolHandler::isStaleResult() const
{
    return m_activeSearchSeq != 0 && m_activeSearchSeq != m_seq;
}

void UsiProtocolHandler::finishSearch()
{
    if (isSearching()) setState(State::Ready);
}

// ============================================================
// そのほかのコマンド
// ============================================================

void UsiProtocolHandler::sendCommand(const QString& command)
{
    if (m_process) m_process->sendCommand(command);
}

void UsiProtocolHandler::sendUsi()
{
    sendCommand(QStringLiteral("usi"));
}

void UsiProtocolHandler::sendIsReady()
{
    sendCommand(QStringLiteral("isready"));
}

void UsiProtocolHandler::sendUsiNewGame()
{
    sendCommand(QStringLiteral("usinewgame"));
}

void UsiProtocolHandler::sendGameOver(GameOverResult result)
{
    sendCommand(QStringLiteral("gameover ") + gameOverResultToString(result));
}

void UsiProtocolHandler::sendSetOption(const QString& name, const QString& value)
{
    sendCommand(QStringLiteral("setoption name ") + name + QStringLiteral(" value ") + value);
}

void UsiProtocolHandler::sendConsoleCommand(const QString& command)
{
    sendCommand(command);
}

void UsiProtocolHandler::cancelCurrentOperation()
{
    // 探索中のエンジンはまだ考えている。状態はそのままにして、遅れて届く結果を捨てる
    ++m_seq;
    if (isInitializing()) {
        m_initializationTimer.stop();
        setState(State::NotStarted);
    }
}

// ============================================================
// 受信
// ============================================================

void UsiProtocolHandler::onDataReceived(const QString& line)
{
    if (UsiLineParser::isBestMoveLine(line)) {
        handleBestMove(line);
        return;
    }
    if (const auto checkmate = UsiLineParser::parseCheckmate(line)) {
        handleCheckmate(*checkmate);
        return;
    }
    if (const auto info = UsiLineParser::parseInfo(line)) {
        // 取り消した探索の info は思考の表に出さない
        if (isStaleResult()) return;
        updateSearch(*info);
        emit infoReceived(*info);
        return;
    }

    const QString trimmed = line.trimmed();
    if (trimmed == QLatin1String("readyok")) {
        handleReadyOk();
        return;
    }
    if (trimmed == QLatin1String("usiok")) {
        handleUsiOk();
        return;
    }
    // usi〜usiok の報告
    if (trimmed.startsWith(QLatin1String("option "))) {
        m_reportedOptionLines.append(trimmed);
        if (const auto option = UsiLineParser::parseOption(line)) m_reportedOptions.insert(option->name, *option);
        return;
    }
    if (const auto name = UsiLineParser::parseIdName(line)) {
        m_reportedEngineName = *name;
        return;
    }
    if (const auto author = UsiLineParser::parseIdAuthor(line)) {
        m_reportedEngineAuthor = *author;
        return;
    }
}

void UsiProtocolHandler::handleBestMove(const QString& line)
{
    // 取り消した探索に遅れて届いた結果は知らせない
    if (isStaleResult()) {
        qCDebug(lcEngine) << "[drop-bestmove] (cancelled search)" << line;
        finishSearch();
        emit searchEnded();
        return;
    }
    std::optional<UsiBestMove> parsed = UsiLineParser::parseBestMove(line);
    if (!parsed) {
        const QPointer<UsiProtocolHandler> guard(this);
        emit errorOccurred(tr("Invalid bestmove format: %1").arg(line));
        if (!guard) return;
        cancelCurrentOperation();
        finishSearch();
        emit searchEnded();
        return;
    }

    UsiBestMove result = std::move(*parsed);
    result.elapsedMs = m_goTimer.isValid() ? m_goTimer.elapsed() : 0;
    result.score = m_searchScore;
    result.baseSfen = m_baseSfen;
    result.stopped = m_state == State::Stopping;
    result.pondering = isSearching() && m_searchKind == SearchKind::Ponder;
    finishSearch();
    const QPointer<UsiProtocolHandler> guard(this);
    emit bestMoveReceived(result);
    if (guard) emit searchEnded();
}

void UsiProtocolHandler::handleCheckmate(const UsiCheckmate& checkmate)
{
    if (isStaleResult()) {
        qCDebug(lcEngine) << "[drop-checkmate] (cancelled search)";
        finishSearch();
        emit searchEnded();
        return;
    }
    UsiCheckmate result = checkmate;
    result.stopped = m_state == State::Stopping;
    finishSearch();
    const QPointer<UsiProtocolHandler> guard(this);
    emit checkmateReceived(result);
    if (guard) emit searchEnded();
}

void UsiProtocolHandler::updateSearch(const UsiInfo& info)
{
    if (!isSearching() || isStaleResult()) return;
    // 評価値は最善の読み筋（multipv 1）から取る
    if (info.multipv != 1) return;
    if (info.hasScore()) m_searchScore = info.score;
}
