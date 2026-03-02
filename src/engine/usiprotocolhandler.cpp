/// @file usiprotocolhandler.cpp
/// @brief USIプロトコル送受信管理クラスの実装

#include "usiprotocolhandler.h"
#include "usimovecoordinateconverter.h"
#include "logcategories.h"
#include "engineprocessmanager.h"
#include "thinkinginfopresenter.h"
#include "shogigamecontroller.h"
#include "enginesettingsconstants.h"
#include "settingscommon.h"

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
    m_processManager = manager;
    
    if (m_processManager) {
        connect(m_processManager, &EngineProcessManager::dataReceived,
                this, &UsiProtocolHandler::onDataReceived);
    }
}

void UsiProtocolHandler::setPresenter(ThinkingInfoPresenter* presenter)
{
    m_presenter = presenter;
}

void UsiProtocolHandler::setGameController(ShogiGameController* controller)
{
    m_gameController = controller;
}

// ============================================================
// 初期化
// ============================================================

bool UsiProtocolHandler::initializeEngine(const QString& /*engineName*/)
{
    m_reportedOptions.clear();

    sendUsi();
    if (!waitForUsiOk(5000)) {
        emit errorOccurred(tr("Timeout waiting for usiok"));
        return false;
    }

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

    // エンジンがUSI_Ponderを報告していない場合はponderを無効にする
    if (m_isPonderEnabled && !m_reportedOptions.contains(QStringLiteral("USI_Ponder"))) {
        m_isPonderEnabled = false;
    }

    sendIsReady();
    if (!waitForReadyOk(5000)) {
        emit errorOccurred(tr("Timeout waiting for readyok"));
        return false;
    }

    sendUsiNewGame();

    return true;
}

void UsiProtocolHandler::loadEngineOptions(const QString& engineName)
{
    // 処理フロー:
    // 1. 設定ファイルからエンジンオプション配列を読み込み
    // 2. 各オプションをsetoptionコマンド文字列に変換
    //    - buttonタイプ: "on"時のみvalueなしで送信
    //    - その他: name + valueで送信
    // 3. USI_Ponderオプションがあればponder有効フラグを更新

    m_setOptionCommands.clear();
    m_isPonderEnabled = false;

    QSettings settings(SettingsCommon::settingsFilePath(), QSettings::IniFormat);

    int count = settings.beginReadArray(engineName);

    for (int i = 0; i < count; ++i) {
        settings.setArrayIndex(i);

        QString name = settings.value("name").toString();
        QString value = settings.value("value").toString();
        QString type = settings.value("type").toString();

        if (type == QLatin1String("button")) {
            // buttonタイプは"on"の場合のみvalueなしで送信
            if (value == QLatin1String("on")) {
                m_setOptionCommands.append("setoption name " + name);
            }
        } else {
            m_setOptionCommands.append("setoption name " + name + " value " + value);

            if (name == QLatin1String("USI_Ponder")) {
                m_isPonderEnabled = (value == QLatin1String("true"));
            }
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
    sendCommand("usi");
}

void UsiProtocolHandler::sendIsReady()
{
    sendCommand("isready");
}

void UsiProtocolHandler::sendUsiNewGame()
{
    sendCommand("usinewgame");
}

void UsiProtocolHandler::sendPosition(const QString& positionStr)
{
    sendCommand(positionStr);
}

void UsiProtocolHandler::beginMainSearch()
{
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }
    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;
}

void UsiProtocolHandler::sendGo(int byoyomiMs, const QString& btime, const QString& wtime,
                                int bincMs, int wincMs, bool useByoyomi)
{
    beginMainSearch();

    QString command;
    if (useByoyomi) {
        command = "go btime " + btime + " wtime " + wtime +
                  " byoyomi " + QString::number(byoyomiMs);
    } else {
        command = "go btime " + btime + " wtime " + wtime +
                  " binc " + QString::number(bincMs) +
                  " winc " + QString::number(wincMs);
    }
    sendCommand(command);
}

void UsiProtocolHandler::sendGoPonder()
{
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }
    m_phase = SearchPhase::Ponder;
    ++m_ponderSession;
    sendCommand("go ponder");
}

void UsiProtocolHandler::sendGoMate(int timeMs, bool infinite)
{
    m_modeTsume = true;
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

void UsiProtocolHandler::sendGoNodes(qint64 nodes)
{
    beginMainSearch();
    sendCommand(QStringLiteral("go nodes %1").arg(nodes));
}

void UsiProtocolHandler::sendGoMovetime(int timeMs)
{
    beginMainSearch();
    sendCommand(QStringLiteral("go movetime %1").arg(timeMs));
}

void UsiProtocolHandler::sendGoSearchmoves(const QStringList& moves, bool infinite)
{
    if (moves.isEmpty()) {
        qCWarning(lcEngine) << "sendGoSearchmoves: 手リストが空、go infiniteを送信";
        sendCommand(QStringLiteral("go infinite"));
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
    sendCommand("stop");
    emit stopOrPonderhitSent();
}

void UsiProtocolHandler::sendPonderHit()
{
    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();

    sendCommand("ponderhit");
    emit stopOrPonderhitSent();

    m_phase = SearchPhase::Main;
}

void UsiProtocolHandler::sendGameOver(GameOverResult result)
{
    sendCommand(QStringLiteral("gameover ") + gameOverResultToString(result));
}

void UsiProtocolHandler::sendQuit()
{
    cancelCurrentOperation();
    
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

void UsiProtocolHandler::sendRaw(const QString& command)
{
    sendCommand(command);
}

// ============================================================
// データ受信ハンドラ
// ============================================================

void UsiProtocolHandler::onDataReceived(const QString& line)
{
    // 処理フロー:
    // 1. タイムアウト宣言済みなら全行を破棄
    // 2. checkmate行 → handleCheckmateLine（詰将棋結果）
    // 3. bestmove行 → handleBestMoveLine（最善手解析）
    // 4. info行 → シグナル発行 + Presenterへ転送
    // 5. readyok → フラグ＋シグナル
    // 6. usiok → フラグ＋シグナル

    if (m_timeoutDeclared) {
        qCDebug(lcEngine) << "[drop-after-timeout]" << line;
        return;
    }

    if (line.startsWith(QStringLiteral("checkmate"))) {
        handleCheckmateLine(line);
        return;
    }

    if (line.startsWith(QStringLiteral("bestmove"))) {
        m_bestMoveReceived = true;
        handleBestMoveLine(line);
        emit bestMoveReceived();
        return;
    }

    if (line.startsWith(QStringLiteral("info"))) {
        qCDebug(lcEngine) << "info行受信:" << line.left(60);
        emit infoLineReceived(line);
        if (m_presenter) {
            m_presenter->onInfoReceived(line);
        }
        return;
    }

    const QString trimmed = line.trimmed();

    if (trimmed == QStringLiteral("readyok")) {
        m_readyOkReceived = true;
        emit readyOkReceived();
        return;
    }

    if (line.startsWith(QStringLiteral("option name "))) {
        // エンジンが報告したオプション名を記録（usi〜usiok間）
        // 形式: "option name <name> type <type> ..."
        const qsizetype nameStart = 12; // "option name " の長さ
        qsizetype typePos = line.indexOf(QStringLiteral(" type "), nameStart);
        if (typePos > nameStart) {
            m_reportedOptions.insert(line.mid(nameStart, typePos - nameStart));
        }
        return;
    }

    if (trimmed == QStringLiteral("usiok")) {
        m_usiOkReceived = true;
        emit usiOkReceived();
        return;
    }
}

void UsiProtocolHandler::handleBestMoveLine(const QString& line)
{
    // 処理フロー:
    // 1. トークン分割してbestmoveキーワードの位置を特定
    // 2. オペレーションIDの一致確認（キャンセル済みなら破棄）
    // 3. 経過時間を記録
    // 4. resign/win判定 → 重複防止付きでシグナル発行
    // 5. 通常手 → ponder手があれば取得

    const quint64 observedId = m_seq;

    const QStringList tokens = line.split(whitespaceRe(), Qt::SkipEmptyParts);
    const int bestMoveIndex = static_cast<int>(tokens.indexOf(QStringLiteral("bestmove")));

    if (bestMoveIndex == -1 || bestMoveIndex + 1 >= tokens.size()) {
        emit errorOccurred(tr("Invalid bestmove format: %1").arg(line));
        cancelCurrentOperation();
        return;
    }

    // キャンセル済みオペレーションからのbestmoveは破棄
    if (observedId != m_seq) {
        qCDebug(lcEngine) << "[drop-bestmove] (op-id-mismatch)" << line;
        return;
    }

    m_bestMove = tokens.at(bestMoveIndex + 1);

    qint64 elapsed = m_goTimer.isValid() ? m_goTimer.elapsed() : -1;
    m_lastGoToBestmoveMs = (elapsed >= 0) ? elapsed : 0;
    m_bestMoveReceived = true;

    m_specialMove = parseSpecialMove(m_bestMove);

    if (m_specialMove == SpecialMove::Resign) {
        // 重複シグナル防止
        if (m_resignNotified) {
            qCDebug(lcEngine) << "[dup-resign-ignored]";
            return;
        }
        m_resignNotified = true;
        emit bestMoveResignReceived();
        return;
    }

    if (m_specialMove == SpecialMove::Win) {
        // 重複シグナル防止
        if (m_winNotified) {
            qCDebug(lcEngine) << "[dup-win-ignored]";
            return;
        }
        m_winNotified = true;
        emit bestMoveWinReceived();
        return;
    }

    const int ponderIdx = static_cast<int>(tokens.indexOf(QStringLiteral("ponder")));
    if (ponderIdx != -1 && ponderIdx + 1 < tokens.size()) {
        m_predictedOpponentMove = tokens.at(ponderIdx + 1);
    } else {
        m_predictedOpponentMove.clear();
    }
}

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

// ============================================================
// 指し手処理
// ============================================================

void UsiProtocolHandler::parseMoveCoordinates(int& fileFrom, int& rankFrom,
                                              int& fileTo, int& rankTo)
{
    fileFrom = rankFrom = fileTo = rankTo = -1;
    const QString move = m_bestMove.trimmed();

    if (m_specialMove != SpecialMove::None) {
        if (m_gameController) m_gameController->setPromote(false);
        return;
    }

    if (move.size() < 4) {
        emit errorOccurred(tr("Invalid bestmove format: \"%1\"").arg(move));
        cancelCurrentOperation();
        return;
    }

    const bool promote = (move.size() >= 5 && move.at(4) == QLatin1Char('+'));
    if (m_gameController) m_gameController->setPromote(promote);

    const bool isP1 = m_gameController &&
        m_gameController->currentPlayer() == ShogiGameController::Player1;

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
    QString result = UsiMoveCoordinateConverter::convertHumanMoveToUsi(from, to, promote, errorMsg);
    if (!errorMsg.isEmpty()) {
        emit errorOccurred(errorMsg);
    }
    return result;
}

// ============================================================
// 座標変換ユーティリティ（UsiMoveCoordinateConverterへの委譲）
// ============================================================

QChar UsiProtocolHandler::rankToAlphabet(int rank)
{
    return UsiMoveCoordinateConverter::rankToAlphabet(rank);
}

std::optional<int> UsiProtocolHandler::alphabetToRank(QChar c)
{
    return UsiMoveCoordinateConverter::alphabetToRank(c);
}

// ============================================================
// オペレーションコンテキスト
// ============================================================

quint64 UsiProtocolHandler::beginOperationContext()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    m_opCtx = new QObject(this);
    return ++m_seq;
}

void UsiProtocolHandler::cancelCurrentOperation()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    ++m_seq;
}
