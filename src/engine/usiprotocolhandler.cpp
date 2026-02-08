/// @file usiprotocolhandler.cpp
/// @brief USIプロトコル送受信管理クラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "usiprotocolhandler.h"
#include "engineprocessmanager.h"
#include "thinkinginfopresenter.h"
#include "shogigamecontroller.h"
#include "enginesettingsconstants.h"

#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QSettings>
#include <QDir>
#include <QDebug>
#include <QRegularExpression>
#include <QThread>

using namespace EngineSettingsConstants;

// ============================================================
// 静的メンバ変数の定義
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
const QMap<int, QString>& UsiProtocolHandler::firstPlayerPieceMap()
{
    static const auto& m = *[]() {
        static const QMap<int, QString> map = {
            {1, "P*"}, {2, "L*"}, {3, "N*"}, {4, "S*"}, {5, "G*"}, {6, "B*"}, {7, "R*"}
        };
        return &map;
    }();
    return m;
}

/// @todo remove コメントスタイルガイド適用済み
const QMap<int, QString>& UsiProtocolHandler::secondPlayerPieceMap()
{
    static const auto& m = *[]() {
        static const QMap<int, QString> map = {
            {3, "R*"}, {4, "B*"}, {5, "G*"}, {6, "S*"}, {7, "N*"}, {8, "L*"}, {9, "P*"}
        };
        return &map;
    }();
    return m;
}

/// @todo remove コメントスタイルガイド適用済み
const QMap<QChar, int>& UsiProtocolHandler::pieceRankWhite()
{
    static const auto& m = *[]() {
        static const QMap<QChar, int> map = {
            {'P', 9}, {'L', 8}, {'N', 7}, {'S', 6}, {'G', 5}, {'B', 4}, {'R', 3}
        };
        return &map;
    }();
    return m;
}

/// @todo remove コメントスタイルガイド適用済み
const QMap<QChar, int>& UsiProtocolHandler::pieceRankBlack()
{
    static const auto& m = *[]() {
        static const QMap<QChar, int> map = {
            {'P', 1}, {'L', 2}, {'N', 3}, {'S', 4}, {'G', 5}, {'B', 6}, {'R', 7}
        };
        return &map;
    }();
    return m;
}

namespace {
/// @todo remove コメントスタイルガイド適用済み
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

/// @todo remove コメントスタイルガイド適用済み
UsiProtocolHandler::UsiProtocolHandler(QObject* parent)
    : QObject(parent)
{
}

/// @todo remove コメントスタイルガイド適用済み
UsiProtocolHandler::~UsiProtocolHandler()
{
    cancelCurrentOperation();
}

// ============================================================
// 依存関係設定
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::setProcessManager(EngineProcessManager* manager)
{
    m_processManager = manager;
    
    if (m_processManager) {
        connect(m_processManager, &EngineProcessManager::dataReceived,
                this, &UsiProtocolHandler::onDataReceived);
    }
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::setPresenter(ThinkingInfoPresenter* presenter)
{
    m_presenter = presenter;
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::setGameController(ShogiGameController* controller)
{
    m_gameController = controller;
}

// ============================================================
// 初期化
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
bool UsiProtocolHandler::initializeEngine(const QString& /*engineName*/)
{
    sendUsi();
    if (!waitForUsiOk(5000)) {
        emit errorOccurred(tr("Timeout waiting for usiok"));
        return false;
    }

    for (const QString& cmd : m_setOptionCommands) {
        sendCommand(cmd);
    }

    sendIsReady();
    if (!waitForReadyOk(5000)) {
        emit errorOccurred(tr("Timeout waiting for readyok"));
        return false;
    }

    sendUsiNewGame();

    return true;
}

/// @todo remove コメントスタイルガイド適用済み
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

    QDir::setCurrent(QCoreApplication::applicationDirPath());
    QSettings settings(SettingsFileName, QSettings::IniFormat);

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

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendCommand(const QString& command)
{
    if (m_processManager) {
        m_processManager->sendCommand(command);
    }
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendUsi()
{
    sendCommand("usi");
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendIsReady()
{
    sendCommand("isready");
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendUsiNewGame()
{
    sendCommand("usinewgame");
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendPosition(const QString& positionStr)
{
    sendCommand(positionStr);
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGo(int byoyomiMs, const QString& btime, const QString& wtime,
                                int bincMs, int wincMs, bool useByoyomi)
{
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }

    QString command;
    if (useByoyomi) {
        command = "go btime " + btime + " wtime " + wtime + 
                  " byoyomi " + QString::number(byoyomiMs);
    } else {
        command = "go btime " + btime + " wtime " + wtime +
                  " binc " + QString::number(bincMs) +
                  " winc " + QString::number(wincMs);
    }

    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    sendCommand(command);
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGoPonder()
{
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }

    m_phase = SearchPhase::Ponder;
    ++m_ponderSession;

    sendCommand("go ponder");
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGoMate(int timeMs, bool infinite)
{
    m_modeTsume = true;

    if (infinite || timeMs <= 0) {
        sendCommand("go mate infinite");
    } else {
        sendCommand(QString("go mate %1").arg(timeMs));
    }
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGoDepth(int depth)
{
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }

    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    sendCommand(QStringLiteral("go depth %1").arg(depth));
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGoNodes(qint64 nodes)
{
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }

    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    sendCommand(QStringLiteral("go nodes %1").arg(nodes));
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGoMovetime(int timeMs)
{
    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }

    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    sendCommand(QStringLiteral("go movetime %1").arg(timeMs));
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGoSearchmoves(const QStringList& moves, bool infinite)
{
    if (moves.isEmpty()) {
        qWarning() << "[USI] sendGoSearchmoves: moves list is empty, sending go infinite instead";
        sendCommand(QStringLiteral("go infinite"));
        return;
    }

    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }

    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    QString command = QStringLiteral("go searchmoves ") + moves.join(QLatin1Char(' '));
    if (infinite) {
        command += QStringLiteral(" infinite");
    }
    sendCommand(command);
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGoSearchmovesDepth(const QStringList& moves, int depth)
{
    if (moves.isEmpty()) {
        qWarning() << "[USI] sendGoSearchmovesDepth: moves list is empty, sending go depth instead";
        sendGoDepth(depth);
        return;
    }

    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }

    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    QString command = QStringLiteral("go searchmoves ") + moves.join(QLatin1Char(' '));
    command += QStringLiteral(" depth %1").arg(depth);
    sendCommand(command);
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGoSearchmovesMovetime(const QStringList& moves, int timeMs)
{
    if (moves.isEmpty()) {
        qWarning() << "[USI] sendGoSearchmovesMovetime: moves list is empty, sending go movetime instead";
        sendGoMovetime(timeMs);
        return;
    }

    if (m_presenter) {
        m_presenter->requestClearThinkingInfo();
    }

    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    QString command = QStringLiteral("go searchmoves ") + moves.join(QLatin1Char(' '));
    command += QStringLiteral(" movetime %1").arg(timeMs);
    sendCommand(command);
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendStop()
{
    sendCommand("stop");
    emit stopOrPonderhitSent();
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendPonderHit()
{
    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();

    sendCommand("ponderhit");
    emit stopOrPonderhitSent();

    m_phase = SearchPhase::Main;
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendGameOver(const QString& result)
{
    sendCommand("gameover " + result);
}

/// @todo remove コメントスタイルガイド適用済み
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

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendSetOption(const QString& name, const QString& value)
{
    sendCommand("setoption name " + name + " value " + value);
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::sendRaw(const QString& command)
{
    sendCommand(command);
}

// ============================================================
// 待機メソッド
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
bool UsiProtocolHandler::waitForUsiOk(int timeoutMs)
{
    if (m_usiOkReceived) return true;

    m_usiOkReceived = false;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &UsiProtocolHandler::usiOkReceived, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    return m_usiOkReceived;
}

/// @todo remove コメントスタイルガイド適用済み
bool UsiProtocolHandler::waitForReadyOk(int timeoutMs)
{
    if (m_readyOkReceived) return true;

    m_readyOkReceived = false;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &UsiProtocolHandler::readyOkReceived, &loop, &QEventLoop::quit);

    timer.start(timeoutMs);
    loop.exec();

    return m_readyOkReceived;
}

/// @todo remove コメントスタイルガイド適用済み
bool UsiProtocolHandler::waitForBestMove(int timeoutMs)
{
    if (m_bestMoveReceived) return true;
    if (timeoutMs <= 0) return false;

    m_bestMoveReceived = false;

    QElapsedTimer timer;
    timer.start();

    static constexpr int kSliceMs = 10;

    while (!m_bestMoveReceived) {
        if (timer.elapsed() >= timeoutMs) {
            return false;
        }
        if (shouldAbortWait()) {
            return false;
        }

        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(kSliceMs);
        QObject::connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        tick.start();
        spin.exec(QEventLoop::AllEvents);
    }

    return true;
}

/// @todo remove コメントスタイルガイド適用済み
bool UsiProtocolHandler::waitForBestMoveWithGrace(int budgetMs, int graceMs)
{
    QElapsedTimer t;
    t.start();
    const qint64 hard = budgetMs + qMax(0, graceMs);

    m_bestMoveReceived = false;
    const quint64 expectedId = m_seq;

    while (t.elapsed() < hard) {
        if (m_seq != expectedId || shouldAbortWait()) return false;

        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (m_bestMoveReceived) return true;

        QThread::msleep(1);
    }
    return m_bestMoveReceived;
}

/// @todo remove コメントスタイルガイド適用済み
bool UsiProtocolHandler::keepWaitingForBestMove()
{
    m_bestMoveReceived = false;

    while (!m_bestMoveReceived) {
        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(10);
        QObject::connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        tick.start();
        spin.exec(QEventLoop::AllEvents);

        if (shouldAbortWait()) {
            return false;
        }
    }

    return true;
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::waitForStopOrPonderhit()
{
    QEventLoop loop;
    connect(this, &UsiProtocolHandler::stopOrPonderhitSent, &loop, &QEventLoop::quit);
    loop.exec();
}

/// @todo remove コメントスタイルガイド適用済み
bool UsiProtocolHandler::shouldAbortWait() const
{
    if (m_timeoutDeclared) return true;
    
    if (m_processManager) {
        if (m_processManager->shutdownState() != 
            EngineProcessManager::ShutdownState::Running) {
            return true;
        }
        if (!m_processManager->isRunning()) {
            return true;
        }
    }
    
    return false;
}

// ============================================================
// データ受信ハンドラ
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
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
        qDebug() << "[USI] [drop-after-timeout]" << line;
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
        qDebug().noquote() << "[UsiProtocolHandler] emitting infoLineReceived:" << line.left(60);
        emit infoLineReceived(line);
        if (m_presenter) {
            m_presenter->onInfoReceived(line);
        }
        return;
    }

    if (line.contains(QStringLiteral("readyok"))) {
        m_readyOkReceived = true;
        emit readyOkReceived();
        return;
    }

    if (line.contains(QStringLiteral("usiok"))) {
        m_usiOkReceived = true;
        emit usiOkReceived();
        return;
    }
}

/// @todo remove コメントスタイルガイド適用済み
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
        qDebug() << "[USI] [drop-bestmove] (op-id-mismatch)" << line;
        return;
    }

    m_bestMove = tokens.at(bestMoveIndex + 1);

    qint64 elapsed = m_goTimer.isValid() ? m_goTimer.elapsed() : -1;
    m_lastGoToBestmoveMs = (elapsed >= 0) ? elapsed : 0;
    m_bestMoveReceived = true;

    if (m_bestMove.compare(QStringLiteral("resign"), Qt::CaseInsensitive) == 0) {
        // 重複シグナル防止
        if (m_resignNotified) {
            qDebug() << "[USI] [dup-resign-ignored]";
            return;
        }
        m_resignNotified = true;
        m_isResignMove = true;
        emit bestMoveResignReceived();
        return;
    }

    if (m_bestMove.compare(QStringLiteral("win"), Qt::CaseInsensitive) == 0) {
        // 重複シグナル防止
        if (m_winNotified) {
            qDebug() << "[USI] [dup-win-ignored]";
            return;
        }
        m_winNotified = true;
        m_isWinMove = true;
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

/// @todo remove コメントスタイルガイド適用済み
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

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::parseMoveCoordinates(int& fileFrom, int& rankFrom,
                                              int& fileTo, int& rankTo)
{
    fileFrom = rankFrom = fileTo = rankTo = -1;

    const QString move = m_bestMove.trimmed();
    const QString lmove = move.toLower();

    if (lmove == QLatin1String("resign") ||
        lmove == QLatin1String("win") ||
        lmove == QLatin1String("draw")) {
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

    parseMoveFrom(move, fileFrom, rankFrom);
    if (fileFrom < 0 || rankFrom < 0) return;

    parseMoveTo(move, fileTo, rankTo);
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::parseMoveFrom(const QString& move, int& fileFrom, int& rankFrom)
{
    if (QStringLiteral("123456789").contains(move[0])) {
        fileFrom = move[0].digitValue();
        rankFrom = alphabetToRank(move[1]);
        return;
    }

    if (QStringLiteral("PLNSGBR").contains(move[0]) && move[1] == '*') {
        if (m_gameController && 
            m_gameController->currentPlayer() == ShogiGameController::Player1) {
            fileFrom = SENTE_HAND_FILE;
            rankFrom = pieceToRankBlack(move[0]);
        } else {
            fileFrom = GOTE_HAND_FILE;
            rankFrom = pieceToRankWhite(move[0]);
        }
        return;
    }

    emit errorOccurred(tr("Invalid move format in moveFrom"));
    cancelCurrentOperation();
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::parseMoveTo(const QString& move, int& fileTo, int& rankTo)
{
    if (!move[2].isDigit() || !move[3].isLetter()) {
        emit errorOccurred(tr("Invalid move format in moveTo"));
        cancelCurrentOperation();
        return;
    }

    fileTo = move[2].digitValue();
    rankTo = alphabetToRank(move[3]);
}

/// @todo remove コメントスタイルガイド適用済み
QString UsiProtocolHandler::convertHumanMoveToUsi(const QPoint& from, const QPoint& to,
                                                  bool promote)
{
    const int fileFrom = from.x();
    const int rankFrom = from.y();
    const int fileTo = to.x();
    const int rankTo = to.y();

    QString result;

    if (fileFrom >= 1 && fileFrom <= BOARD_SIZE) {
        result = QString::number(fileFrom) + rankToAlphabet(rankFrom);
        result += QString::number(fileTo) + rankToAlphabet(rankTo);
        if (promote) result += "+";
    } else if (fileFrom == SENTE_HAND_FILE) {
        result = convertFirstPlayerPieceSymbol(rankFrom);
        result += QString::number(fileTo) + rankToAlphabet(rankTo);
    } else if (fileFrom == GOTE_HAND_FILE) {
        result = convertSecondPlayerPieceSymbol(rankFrom);
        result += QString::number(fileTo) + rankToAlphabet(rankTo);
    } else {
        emit errorOccurred(tr("Invalid fileFrom value"));
        return QString();
    }

    return result;
}

// ============================================================
// 座標変換ユーティリティ
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
QChar UsiProtocolHandler::rankToAlphabet(int rank)
{
    return QChar('a' + rank - 1);
}

/// @todo remove コメントスタイルガイド適用済み
int UsiProtocolHandler::alphabetToRank(QChar c)
{
    return c.toLatin1() - 'a' + 1;
}

/// @todo remove コメントスタイルガイド適用済み
QString UsiProtocolHandler::convertFirstPlayerPieceSymbol(int rankFrom) const
{
    return firstPlayerPieceMap().value(rankFrom, QString());
}

/// @todo remove コメントスタイルガイド適用済み
QString UsiProtocolHandler::convertSecondPlayerPieceSymbol(int rankFrom) const
{
    return secondPlayerPieceMap().value(rankFrom, QString());
}

/// @todo remove コメントスタイルガイド適用済み
int UsiProtocolHandler::pieceToRankWhite(QChar c)
{
    return pieceRankWhite().value(c, 0);
}

/// @todo remove コメントスタイルガイド適用済み
int UsiProtocolHandler::pieceToRankBlack(QChar c)
{
    return pieceRankBlack().value(c, 0);
}

// ============================================================
// オペレーションコンテキスト
// ============================================================

/// @todo remove コメントスタイルガイド適用済み
quint64 UsiProtocolHandler::beginOperationContext()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    m_opCtx = new QObject(this);
    return ++m_seq;
}

/// @todo remove コメントスタイルガイド適用済み
void UsiProtocolHandler::cancelCurrentOperation()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    ++m_seq;
}
