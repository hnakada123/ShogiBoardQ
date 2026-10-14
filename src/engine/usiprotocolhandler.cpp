/// @file usiprotocolhandler.cpp
/// @brief USIプロトコル送受信管理クラスの実装

#include "usiprotocolhandler.h"
#include "logcategories.h"
#include "engineprocessmanager.h"
#include "sfenpositiontracer.h"
#include "enginesettingsconstants.h"
#include "enginepondersettings.h"
#include "settingscommon.h"
#include "usiinfoparser.h"

#include <QSettings>
#include <QRegularExpression>

using namespace EngineSettingsConstants;

namespace {
const QRegularExpression& whitespaceRe()
{
    static const auto& re = *[]() {
        static const QRegularExpression r(QStringLiteral("\\s+"));
        return &r;
    }();
    return re;
}
} // anonymous namespace

// ============================================================
// コンストラクタ・デストラクタ
// ============================================================

UsiProtocolHandler::UsiProtocolHandler(QObject* parent)
    : QObject(parent)
{
    m_initializationTimer.setSingleShot(true);
    connect(&m_initializationTimer, &QTimer::timeout, this, &UsiProtocolHandler::onInitializationTimeout);
    connect(this, &UsiProtocolHandler::usiOkReceived, this, &UsiProtocolHandler::onInitializationUsiOk);
    connect(this, &UsiProtocolHandler::readyOkReceived, this, &UsiProtocolHandler::onInitializationReadyOk);
}

UsiProtocolHandler::~UsiProtocolHandler()
{
    cancelCurrentOperation();
}

// ============================================================
// 依存関係設定
// ============================================================

void UsiProtocolHandler::setProcessManager(EngineProcessManager* manager)
{
    if (m_processManager == manager) {
        return;
    }

    if (m_processManager) {
        disconnect(m_processManager, &EngineProcessManager::dataReceived,
                   this, &UsiProtocolHandler::onDataReceived);
    }

    m_processManager = manager;

    if (m_processManager) {
        connect(m_processManager, &EngineProcessManager::dataReceived,
                this, &UsiProtocolHandler::onDataReceived,
                Qt::UniqueConnection);
    }
}

void UsiProtocolHandler::setBaseSfen(const QString& sfen)
{
    m_baseSfen = sfen;
    emit positionChanged(sfen);
}

// ============================================================
// 初期化
// ============================================================

void UsiProtocolHandler::sendConfiguredOptions()
{
    // エンジンが報告したオプションのみ送信する。
    // 設定ファイルに保存されていてもエンジンが対応していないオプションは送信しない。
    for (const QString& cmd : std::as_const(m_setOptionCommands)) {
        // "setoption name <name> value ..." または "setoption name <name>" から名前を抽出
        static const QString prefix = QStringLiteral("setoption name ");
        if (cmd.startsWith(prefix)) {
            qsizetype valuePos = cmd.indexOf(QStringLiteral(" value "), prefix.length());
            QString optName = (valuePos > 0)
                ? cmd.mid(prefix.length(), valuePos - prefix.length())
                : cmd.mid(prefix.length());
            if (!m_reportedOptions.contains(optName)) {
                qCDebug(lcEngine) << "Skipping unsupported option:" << optName;
                continue;
            }
        }
        sendCommand(cmd);
    }

    // この接続だけの設定は、保存済みの設定の後に送って上書きする
    for (const auto& option : std::as_const(m_sessionOptions)) {
        if (m_reportedOptions.contains(option.first)) sendSetOption(option.first, option.second);
    }

    // USI_Ponderは報告を省略できる予約オプション。通知可否とGUIの先読み許可は独立。
    if (m_sendUnreportedPonderOption || m_reportedOptions.contains(QStringLiteral("USI_Ponder"))) {
        sendSetOption(QStringLiteral("USI_Ponder"),
                      m_isPonderEnabled ? QStringLiteral("true") : QStringLiteral("false"));
    }

}

void UsiProtocolHandler::loadEngineOptions(const QString& engineName)
{
    m_setOptionCommands.clear();
    const auto ponder = EnginePonderSettings::load(engineName);
    m_isPonderEnabled = ponder.enabled;
    m_sendUnreportedPonderOption = ponder.sendUnreportedOption;

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    int count = settings.beginReadArray(engineName);

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);

        QString name = settings.value("name").toString();
        QString value = settings.value("value").toString();
        QString type = settings.value("type").toString();

        // GUI設定から初期化時に一度だけ送信する。
        if (name == QLatin1String("USI_Ponder")) continue;

        if (type == QLatin1String("button")) {
            // buttonタイプは"on"の場合のみvalueなしで送信
            if (value == QLatin1String("on")) {
                m_setOptionCommands.append("setoption name " + name);
            }
        } else {
            m_setOptionCommands.append("setoption name " + name + " value " + value);
        }
    }

    settings.endArray();
}

// ============================================================
// USIコマンド送信
// ============================================================

void UsiProtocolHandler::sendCommand(const QString& command)
{
    if (m_processManager) {
        m_processManager->sendCommand(command);
    }
}

void UsiProtocolHandler::sendUsi()
{
    // 以前のハンドシェイク状態を引き継がない
    (void)beginOperationContext();
    sendCommand("usi");
}

void UsiProtocolHandler::sendIsReady()
{
    // 以前のハンドシェイク状態を引き継がない
    (void)beginOperationContext();
    sendCommand("isready");
}

void UsiProtocolHandler::sendUsiNewGame()
{
    sendCommand("usinewgame");
}

void UsiProtocolHandler::sendPosition(const QString& positionStr)
{
    const QString sfen = SfenPositionTracer::sfenFromPositionCommand(positionStr);
    if (!sfen.isEmpty()) setBaseSfen(sfen);
    sendCommand(positionStr);
}

void UsiProtocolHandler::beginMainSearch()
{
    m_activeSearchSeq = beginOperationContext();
    m_bestMoveReceived = false;
    m_specialMove = SpecialMove::None;
    m_predictedOpponentMove.clear();

    emit searchStarted();
    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;
    beginCandidateSearch(false);
}

void UsiProtocolHandler::sendGo(const UsiGoTime& goTime)
{
    beginMainSearch();
    sendCommand(goTime.toCommand(false));
}

void UsiProtocolHandler::sendGoPonder(const UsiGoTime& goTime)
{
    m_activeSearchSeq = beginOperationContext();
    m_bestMoveReceived = false;
    m_specialMove = SpecialMove::None;
    emit searchStarted();
    m_phase = SearchPhase::Ponder;
    beginCandidateSearch(true);
    sendCommand(goTime.toCommand(true));
}

void UsiProtocolHandler::sendGoMate(int timeMs, bool infinite)
{
    invalidateCandidate();
    m_activeSearchSeq = beginOperationContext();
    m_bestMoveReceived = false;
    m_specialMove = SpecialMove::None;
    m_predictedOpponentMove.clear();

    if (infinite || timeMs <= 0) {
        sendCommand("go mate infinite");
    } else {
        sendCommand(QString("go mate %1").arg(timeMs));
    }
}

void UsiProtocolHandler::sendGoDepth(int depth)
{
    beginMainSearch();
    sendCommand(QStringLiteral("go depth %1").arg(depth));
}

void UsiProtocolHandler::sendGoMovetime(int timeMs)
{
    beginMainSearch();
    sendCommand(QStringLiteral("go movetime %1").arg(timeMs));
}

void UsiProtocolHandler::sendGoInfinite()
{
    beginMainSearch();
    sendCommand(QStringLiteral("go infinite"));
}

void UsiProtocolHandler::sendGoSearchmoves(const QStringList& moves, bool infinite)
{
    if (moves.isEmpty()) {
        qCWarning(lcEngine) << "sendGoSearchmoves: 手リストが空、go infiniteを送信";
        sendGoInfinite();
        return;
    }
    beginMainSearch();
    QString command = QStringLiteral("go searchmoves ") + moves.join(QLatin1Char(' '));
    if (infinite) {
        command += QStringLiteral(" infinite");
    }
    sendCommand(command);
}

void UsiProtocolHandler::sendGoSearchmovesDepth(const QStringList& moves, int depth)
{
    if (moves.isEmpty()) {
        qCWarning(lcEngine) << "sendGoSearchmovesDepth: 手リストが空、go depthを送信";
        sendGoDepth(depth);
        return;
    }
    beginMainSearch();
    QString command = QStringLiteral("go searchmoves ") + moves.join(QLatin1Char(' '));
    command += QStringLiteral(" depth %1").arg(depth);
    sendCommand(command);
}

void UsiProtocolHandler::sendGoSearchmovesMovetime(const QStringList& moves, int timeMs)
{
    if (moves.isEmpty()) {
        qCWarning(lcEngine) << "sendGoSearchmovesMovetime: 手リストが空、go movetimeを送信";
        sendGoMovetime(timeMs);
        return;
    }
    beginMainSearch();
    QString command = QStringLiteral("go searchmoves ") + moves.join(QLatin1Char(' '));
    command += QStringLiteral(" movetime %1").arg(timeMs);
    sendCommand(command);
}

void UsiProtocolHandler::sendStop()
{
    invalidateCandidate();
    if (m_phase == SearchPhase::StoppingPonder) return;
    if (m_phase == SearchPhase::Ponder) {
        m_phase = SearchPhase::StoppingPonder;
        m_predictedOpponentMove.clear();
    }
    sendCommand("stop");
    emit stopOrPonderhitSent();
}

void UsiProtocolHandler::sendPonderHit()
{
    if (m_phase != SearchPhase::Ponder || m_bestMoveReceived) return;
    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    m_searchCandidate.pondering = false;
    m_searchCandidate.predictedMove.clear();
    emit searchCandidateChanged();

    sendCommand("ponderhit");
    emit stopOrPonderhitSent();

}

void UsiProtocolHandler::sendGameOver(GameOverResult result)
{
    sendCommand(QStringLiteral("gameover ") + gameOverResultToString(result));
}

void UsiProtocolHandler::sendQuit()
{
    cancelCurrentOperation();
    m_activeSearchSeq = 0;
    if (m_processManager) {
        m_processManager->setShutdownState(
            EngineProcessManager::ShutdownState::IgnoreAllExceptInfoString);
        m_processManager->setPostQuitInfoStringLinesLeft(1);
    }

    sendCommand("quit");

    if (m_processManager) {
        m_processManager->discardStdout();
        m_processManager->discardStderr();
        m_processManager->closeWriteChannel();
    }
}

void UsiProtocolHandler::sendSetOption(const QString& name, const QString& value)
{
    sendCommand("setoption name " + name + " value " + value);
}

void UsiProtocolHandler::sendSetOption(const QString& name, const QString& value,
                                       const QString& type)
{
    if (type.compare(QStringLiteral("button"), Qt::CaseInsensitive) == 0) {
        sendCommand("setoption name " + name);
        return;
    }
    sendSetOption(name, value);
}

void UsiProtocolHandler::sendConsoleCommand(const QString& command)
{
    sendCommand(command);
}

// ============================================================
// データ受信ハンドラ
// ============================================================

void UsiProtocolHandler::onDataReceived(const QString& line)
{
    if (m_timeoutDeclared) {
        qCDebug(lcEngine) << "[drop-after-timeout]" << line;
        return;
    }

    if (line.startsWith(QStringLiteral("checkmate"))) {
        handleCheckmateLine(line);
        return;
    }

    if (line.startsWith(QStringLiteral("bestmove"))) {
        if (handleBestMoveLine(line)) {
            m_bestMoveReceived = true;
            emit bestMoveReceived();
        }
        return;
    }

    if (line.startsWith(QStringLiteral("info"))) {
        // info 行の解釈はここで一度だけ行い、受け手には型付きの値を渡す
        const std::optional<UsiInfo> info = UsiInfoParser::parse(line);
        if (!info) {
            qCDebug(lcEngine) << "[drop-info] (unreadable)" << line.left(60);
            return;
        }
        updateCandidate(*info);
        qCDebug(lcEngine) << "info行受信:" << line.left(60);
        emit infoReceived(*info);
        return;
    }

    const QString trimmed = line.trimmed();

    if (trimmed == QStringLiteral("readyok")) {
        emit readyOkReceived();
        return;
    }

    // エンジンが報告したオプションを記録する（usi〜usiok間）
    if (const auto option = UsiInfoParser::parseOptionDeclaration(line)) {
        m_reportedOptions.insert(option->name);
        m_reportedOptionLines.insert(option->name, trimmed);
        return;
    }

    if (const auto name = UsiInfoParser::parseIdName(line)) {
        m_reportedEngineName = *name;
        return;
    }

    if (trimmed == QStringLiteral("usiok")) {
        emit usiOkReceived();
        return;
    }
}

bool UsiProtocolHandler::handleBestMoveLine(const QString& line)
{
    const quint64 observedId = m_seq;

    // stop/cancel で世代が進んだ後に届いた遅延 bestmove は破棄する
    if (m_activeSearchSeq != 0 && observedId != m_activeSearchSeq) {
        qCDebug(lcEngine) << "[drop-bestmove] (inactive-search-seq)" << line;
        return false;
    }

    const QStringList tokens = line.split(whitespaceRe(), Qt::SkipEmptyParts);
    const int bestMoveIndex = static_cast<int>(tokens.indexOf(QStringLiteral("bestmove")));

    if (bestMoveIndex == -1 || bestMoveIndex + 1 >= tokens.size()) {
        emit errorOccurred(tr("Invalid bestmove format: %1").arg(line));
        cancelCurrentOperation();
        return false;
    }

    // キャンセル済みオペレーションからのbestmoveは破棄
    if (observedId != m_seq) {
        qCDebug(lcEngine) << "[drop-bestmove] (op-id-mismatch)" << line;
        return false;
    }

    m_bestMove = tokens.at(bestMoveIndex + 1);

    qint64 elapsed = m_goTimer.isValid() ? m_goTimer.elapsed() : -1;
    m_lastGoToBestmoveMs = (elapsed >= 0) ? elapsed : 0;
    m_specialMove = parseSpecialMove(m_bestMove);

    const bool ponderResult = m_phase == SearchPhase::Ponder
        || m_phase == SearchPhase::StoppingPonder;
    m_phase = SearchPhase::Idle;
    invalidateCandidate();
    // stopで回収する予測局面の投了・宣言勝ちは、実対局の結果ではない。
    if (ponderResult) {
        m_predictedOpponentMove.clear();
        return true;
    }

    if (m_specialMove == SpecialMove::Resign) {
        if (m_squelchResignLogging) {
            qCDebug(lcEngine) << "[resign-squelched]";
            return true;
        }
        // 重複シグナル防止
        if (m_resignNotified) {
            qCDebug(lcEngine) << "[dup-resign-ignored]";
            return true;
        }
        m_resignNotified = true;
        emit bestMoveResignReceived();
        return true;
    }

    if (m_specialMove == SpecialMove::Win) {
        // 重複シグナル防止
        if (m_winNotified) {
            qCDebug(lcEngine) << "[dup-win-ignored]";
            return true;
        }
        m_winNotified = true;
        emit bestMoveWinReceived();
        return true;
    }

    const int ponderIdx = static_cast<int>(tokens.indexOf(QStringLiteral("ponder")));
    if (ponderIdx != -1 && ponderIdx + 1 < tokens.size()) {
        m_predictedOpponentMove = tokens.at(ponderIdx + 1);
    } else {
        m_predictedOpponentMove.clear();
    }

    return true;
}
