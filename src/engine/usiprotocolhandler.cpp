/// @file usiprotocolhandler.cpp
/// @brief USI エンジン1台の状態とコマンドの送受信の実装

#include "usiprotocolhandler.h"

#include "enginepondersettings.h"
#include "engineprocessmanager.h"
#include "enginesettingsconstants.h"
#include "logcategories.h"
#include "settingscommon.h"
#include "usilineparser.h"

#include <QPointer>
#include <QRegularExpression>
#include <QSettings>

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

void UsiProtocolHandler::loadEngineOptions(const QString& engineName)
{
    m_configuredOptions.clear();
    const auto ponder = EnginePonderSettings::load(engineName);
    m_isPonderEnabled = ponder.enabled;
    m_sendUnreportedPonderOption = ponder.sendUnreportedOption;

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);
    const int count = settings.beginReadArray(EngineSettingsConstants::engineOptionsGroup(engineName));
    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);
        const QString name = settings.value("name").toString();
        // 先読みの可否は GUI の設定から送る
        if (name == QLatin1String("USI_Ponder")) continue;
        const QString value = settings.value("value").toString();
        if (settings.value("type").toString() == QLatin1String("button")) {
            // button 型は "on" のときだけ値なしで送る
            if (value == QLatin1String("on")) m_configuredOptions.append({name, QString(), true});
        } else {
            m_configuredOptions.append({name, value, false});
        }
    }
    settings.endArray();
}

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
    for (const ConfiguredOption& option : std::as_const(m_configuredOptions)) {
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
    if (m_sendUnreportedPonderOption || m_reportedOptions.contains(QStringLiteral("USI_Ponder"))) {
        sendSetOption(QStringLiteral("USI_Ponder"),
                      m_isPonderEnabled ? QStringLiteral("true") : QStringLiteral("false"));
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

std::optional<UsiOptionDeclaration> UsiProtocolHandler::reportedOption(const QString& name) const
{
    const auto it = m_reportedOptions.constFind(name);
    if (it == m_reportedOptions.cend()) return std::nullopt;
    return *it;
}

// ============================================================
// 探索
// ============================================================

void UsiProtocolHandler::startSearch(const UsiPosition& position, const UsiGo& go)
{
    if (!position.isEmpty()) {
        m_baseSfen = position.sfen();
        emit positionChanged(position);
        sendCommand(position.command());
    }

    m_activeSearchSeq = ++m_seq;
    m_searchScore = {};
    m_searchKind = go.kind == UsiGo::Kind::Ponder ? SearchKind::Ponder
        : go.kind == UsiGo::Kind::Mate ? SearchKind::Mate : SearchKind::Main;
    // 先読みは直前の bestmove の ponder の手を予想として探す。それ以外の探索では予想を捨てる
    if (m_searchKind != SearchKind::Ponder) m_predictedMove.clear();
    setState(State::Searching);
    emit searchStarted();
    m_goTimer.start();
    if (m_searchKind == SearchKind::Mate) invalidateCandidate();
    else beginCandidateSearch(m_searchKind == SearchKind::Ponder);
    sendCommand(go.command());
}

void UsiProtocolHandler::sendStop()
{
    if (m_state != State::Searching) return;
    invalidateCandidate();
    // 外れた先読みの結果は使わない
    if (m_searchKind == SearchKind::Ponder) m_predictedMove.clear();
    setState(State::Stopping);
    sendCommand(QStringLiteral("stop"));
}

void UsiProtocolHandler::sendPonderHit()
{
    if (m_state != State::Searching || m_searchKind != SearchKind::Ponder) return;
    m_searchKind = SearchKind::Main;
    m_goTimer.start();
    m_searchCandidate.pondering = false;
    m_searchCandidate.predictedMove.clear();
    emit searchCandidateChanged();
    sendCommand(QStringLiteral("ponderhit"));
}

bool UsiProtocolHandler::isStaleResult() const
{
    return m_activeSearchSeq != 0 && m_activeSearchSeq != m_seq;
}

void UsiProtocolHandler::finishSearch()
{
    if (isSearching()) setState(State::Ready);
    invalidateCandidate();
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
    m_predictedMove.clear();
    invalidateCandidate();
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
        return;
    }
    std::optional<UsiBestMove> parsed = UsiLineParser::parseBestMove(line);
    if (!parsed) {
        emit errorOccurred(tr("Invalid bestmove format: %1").arg(line));
        cancelCurrentOperation();
        finishSearch();
        return;
    }

    UsiBestMove result = std::move(*parsed);
    result.elapsedMs = m_goTimer.isValid() ? m_goTimer.elapsed() : 0;
    result.score = m_searchScore;
    result.baseSfen = m_baseSfen;
    result.stopped = m_state == State::Stopping;
    result.pondering = isSearching() && m_searchKind == SearchKind::Ponder;
    // 外れた先読みの結果（予測局面での投了・宣言勝ちを含む）は、次の先読みの予想に使わない
    m_predictedMove = result.pondering || !result.isMove() ? QString() : result.ponder;
    finishSearch();
    emit bestMoveReceived(result);
}

void UsiProtocolHandler::handleCheckmate(const UsiCheckmate& checkmate)
{
    if (isStaleResult()) {
        qCDebug(lcEngine) << "[drop-checkmate] (cancelled search)";
        finishSearch();
        return;
    }
    UsiCheckmate result = checkmate;
    result.stopped = m_state == State::Stopping;
    finishSearch();
    emit checkmateReceived(result);
}

void UsiProtocolHandler::updateSearch(const UsiInfo& info)
{
    if (!isSearching() || isStaleResult()) return;
    // 評価値と候補手は最善の読み筋（multipv 1）から取る
    if (info.multipv != 1) return;
    if (info.hasScore()) m_searchScore = info.score;
    if (!m_searchCandidate.active || info.pv.isEmpty()) return;
    // info string の中身はパーサが読み筋にしない
    const QString& move = info.pv.first();
    static const QRegularExpression validMove(QStringLiteral("^(?:[1-9][a-i][1-9][a-i]\\+?|[PLNSGBR]\\*[1-9][a-i])$"));
    if (!validMove.match(move).hasMatch() || move == m_searchCandidate.move) return;
    m_searchCandidate.move = move;
    emit searchCandidateChanged();
}

void UsiProtocolHandler::beginCandidateSearch(bool pondering)
{
    m_searchCandidate = {};
    m_searchCandidate.active = true;
    m_searchCandidate.pondering = pondering;
    m_searchCandidate.baseSfen = m_baseSfen;
    if (pondering) m_searchCandidate.predictedMove = m_predictedMove;
    emit searchCandidateChanged();
}

void UsiProtocolHandler::invalidateCandidate()
{
    m_searchCandidate = {};
    emit searchCandidateChanged();
}
