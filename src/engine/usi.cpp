/**
 * @file usi.cpp
 * @brief USIプロトコル通信クラスの実装
 *
 * 修正内容:
 * 1. QObjectの動的プロパティ使用を正式メンバ変数に変更
 * 2. ビジーウェイトループをQEventLoopベースの待機に変更
 * 3. UsiThreadを削除し、シグナル/スロットベースの非同期設計に変更
 * 4. メモリ管理をスマートポインタに変更
 * 5. マジックナンバーを定数に置き換え
 * 6. const_castの使用を削除
 */

#include <QProcess>
#include <iostream>
#include <QString>
#include <QTime>
#include <QFileInfo>
#include <QDir>
#include <QTextStream>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QValueAxis>
#include <QLineEdit>
#include <QApplication>
#include <QSettings>
#include <QLocale>
#include <QMessageBox>
#include <QMap>
#include <QElapsedTimer>
#include <QThread>
#include <QDateTime>
#include <QDebug>
#include <QTimer>
#include <QRegularExpression>
#include <QEventLoop>
#include <memory>

#include "usi.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogienginethinkingmodel.h"
#include "playmode.h"
#include "enginesettingsconstants.h"
#include "shogiutils.h"

using namespace EngineSettingsConstants;

// === 静的メンバ変数の定義 ===

const QMap<int, QString> Usi::s_firstPlayerPieceMap = {
    {1, "P*"}, {2, "L*"}, {3, "N*"}, {4, "S*"}, {5, "G*"}, {6, "B*"}, {7, "R*"}
};

const QMap<int, QString> Usi::s_secondPlayerPieceMap = {
    {3, "R*"}, {4, "B*"}, {5, "G*"}, {6, "S*"}, {7, "N*"}, {8, "L*"}, {9, "P*"}
};

const QMap<QChar, int> Usi::s_pieceRankWhite = {
    {'P', 9}, {'L', 8}, {'N', 7}, {'S', 6}, {'G', 5}, {'B', 4}, {'R', 3}
};

const QMap<QChar, int> Usi::s_pieceRankBlack = {
    {'P', 1}, {'L', 2}, {'N', 3}, {'S', 4}, {'G', 5}, {'B', 6}, {'R', 7}
};

// === ユーティリティ関数 ===

namespace {

// QRegularExpressionの関数ローカルstatic版（clazy警告回避）
const QRegularExpression& whitespaceRe()
{
    static const QRegularExpression re(QStringLiteral("\\s+"));
    return re;
}

QString nowIso()
{
    return QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
}

QString fmtMs(qint64 ms)
{
    if (ms < 0) return QStringLiteral("n/a");
    const qint64 s = ms / 1000;
    const qint64 rem = ms % 1000;
    return QString("%1.%2s").arg(s).arg(rem, 3, 10, QChar('0'));
}

void ensureMovesKeyword(QString& s)
{
    if (!s.contains(QStringLiteral(" moves"))) {
        s = s.trimmed();
        s += QStringLiteral(" moves");
    }
}

} // anonymous namespace

// === コンストラクタ・デストラクタ ===

Usi::Usi(UsiCommLogModel* model, ShogiEngineThinkingModel* modelThinking,
         ShogiGameController* gameController, PlayMode& playMode, QObject* parent)
    : QObject(parent)
    , m_locale(QLocale::English)
    , m_model(model)
    , m_modelThinking(modelThinking)
    , m_gameController(gameController)
    , m_playMode(playMode)
{
}

Usi::~Usi()
{
    cleanupEngineProcessAndThread();
}

// === プロセス・スレッド管理 ===

void Usi::cleanupEngineProcessAndThread()
{
    // エンジンプロセスの終了
    if (m_process) {
        if (m_process->state() == QProcess::Running) {
            sendQuitCommand();

            if (!m_process->waitForFinished(3000)) {
                m_process->terminate();
                if (!m_process->waitForFinished(1000)) {
                    m_process->kill();
                    m_process->waitForFinished(-1);
                }
            }
        }

        disconnect(m_process, nullptr, this, nullptr);
        delete m_process;
        m_process = nullptr;
    }

    // infoバッファをクリア
    m_infoBuffer.clear();
    m_infoFlushScheduled = false;
}

// === 公開インターフェース実装 ===

QString Usi::scoreStr() const
{
    return m_scoreStr;
}

bool Usi::isResignMove() const
{
    return m_isResignMove;
}

int Usi::lastScoreCp() const
{
    return m_lastScoreCp;
}

QString Usi::pvKanjiStr() const
{
    return m_pvKanjiStr;
}

void Usi::setPvKanjiStr(const QString& newPvKanjiStr)
{
    m_pvKanjiStr = newPvKanjiStr;
}

void Usi::setPreviousFileTo(int newPreviousFileTo)
{
    m_previousFileTo = newPreviousFileTo;
}

void Usi::setPreviousRankTo(int newPreviousRankTo)
{
    m_previousRankTo = newPreviousRankTo;
}

// === 駒文字変換 ===

QString Usi::convertFirstPlayerPieceNumberToSymbol(int rankFrom) const
{
    return s_firstPlayerPieceMap.value(rankFrom, QString());
}

QString Usi::convertSecondPlayerPieceNumberToSymbol(int rankFrom) const
{
    return s_secondPlayerPieceMap.value(rankFrom, QString());
}

int Usi::pieceToRankWhite(QChar c)
{
    return s_pieceRankWhite.value(c, 0);
}

int Usi::pieceToRankBlack(QChar c)
{
    return s_pieceRankBlack.value(c, 0);
}

QChar Usi::rankToAlphabet(int rank) const
{
    return QChar('a' + rank - 1);
}

int Usi::alphabetToRank(QChar c)
{
    return int(c.toLatin1() - 'a' + 1);
}

// === エラーハンドリング ===

void Usi::onProcessError(QProcess::ProcessError error)
{
    QString errorMessage;

    switch (error) {
    case QProcess::FailedToStart:
        errorMessage = tr("An error occurred in Usi::onProcessError. The process failed to start.");
        break;
    case QProcess::Crashed:
        errorMessage = tr("An error occurred in Usi::onProcessError. The process crashed.");
        break;
    case QProcess::Timedout:
        errorMessage = tr("An error occurred in Usi::onProcessError. The process timed out.");
        break;
    case QProcess::WriteError:
        errorMessage = tr("An error occurred in Usi::onProcessError. An issue occurred while writing data.");
        break;
    case QProcess::ReadError:
        errorMessage = tr("An error occurred in Usi::onProcessError. An issue occurred while reading data.");
        break;
    default:
        errorMessage = tr("An error occurred in Usi::onProcessError. An unknown error occurred.");
        break;
    }

    cleanupEngineProcessAndThread();
    emit errorOccurred(errorMessage);
    cancelCurrentOperation();
}

void Usi::onProcessFinished(int /*exitCode*/, QProcess::ExitStatus /*status*/)
{
    if (!m_process) return;

    const QByteArray outTail = m_process->readAllStandardOutput();
    const QByteArray errTail = m_process->readAllStandardError();

    auto flushTail = [this](const QByteArray& data, bool isStderr) {
        if (data.isEmpty()) return;

        const QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);
        for (const QString& raw : lines) {
            const QString line = raw.trimmed();
            if (line.isEmpty()) continue;

            if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
                if (line.startsWith(QStringLiteral("info string")) &&
                    m_postQuitInfoStringLinesLeft > 0) {
                    const QString pfx = logPrefix();
                    if (m_model) {
                        m_model->appendUsiCommLog(pfx + " < " + line);
                    }
                    --m_postQuitInfoStringLinesLeft;
                    if (m_postQuitInfoStringLinesLeft <= 0) {
                        m_shutdownState = ShutdownState::IgnoreAll;
                    }
                }
                continue;
            }

            if (m_shutdownState == ShutdownState::IgnoreAll) {
                if (line.startsWith(QStringLiteral("info string"))) {
                    const QString pfx = logPrefix();
                    if (m_model) m_model->appendUsiCommLog(pfx + " < " + line);
                }
                continue;
            }

            const QString pfx = logPrefix();
            if (m_model) {
                if (isStderr) m_model->appendUsiCommLog(pfx + " <stderr> " + line);
                else          m_model->appendUsiCommLog(pfx + " < " + line);
            }
        }
    };

    flushTail(outTail, false);
    flushTail(errTail, true);
    m_shutdownState = ShutdownState::IgnoreAll;
}

// === エンジン起動・初期化 ===

void Usi::initializeAndStartEngineCommunication(QString& engineFile, QString& enginename)
{
    if (engineFile.isEmpty()) {
        cleanupEngineProcessAndThread();
        QString errorMessage = tr("An error occurred in Usi::initializeAndStartEngineCommunication. Engine file path is empty.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    changeDirectoryToEnginePath(engineFile);
    startAndInitializeEngine(engineFile, enginename);
}

void Usi::changeDirectoryToEnginePath(const QString& engineFile)
{
    const QFileInfo fileInfo(engineFile);

    if (!QDir::setCurrent(fileInfo.path())) {
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::changeDirectoryToEnginePath. Failed to change current directory to %1 for the shogi engine %2.")
                .arg(fileInfo.path(), fileInfo.baseName());
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

void Usi::startAndInitializeEngine(const QString& engineFile, const QString& enginename)
{
    startEngine(engineFile);
    sendInitialCommands(enginename);
}

void Usi::startEngine(const QString& engineFile)
{
    if (engineFile.isEmpty() || !QFile::exists(engineFile)) {
        const QString errorMessage =
            tr("An error occurred in Usi::startEngine. The specified engine file does not exist: %1").arg(engineFile);
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    QStringList args;

    if (m_process) {
        disconnect(m_process, nullptr, this, nullptr);
        if (m_process->state() != QProcess::NotRunning) {
            sendQuitCommand();
            m_process->waitForFinished(1000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    m_process = new QProcess(this);
    m_process->start(engineFile, args, QIODevice::ReadWrite);

    if (!m_process->waitForStarted()) {
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::startEngine. Failed to start the engine: %1").arg(engineFile);
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    connect(m_process, &QProcess::readyReadStandardOutput, this, &Usi::readFromEngine);
    connect(m_process, &QProcess::readyReadStandardError, this, &Usi::readFromEngineStderr);
    connect(m_process, &QProcess::errorOccurred, this, &Usi::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Usi::onProcessFinished);
}

void Usi::sendInitialCommands(const QString& enginename)
{
    generateSetOptionCommands(enginename);
    sendUsiCommandAndWaitForUsiOk();
    sendSetOptionCommands();
    sendIsReadyCommandAndWaitForReadyOk();
    sendUsiNewGameCommand();
}

// === コマンド送信 ===

void Usi::sendCommand(const QString& command) const
{
    if (!m_process ||
        (m_process->state() != QProcess::Running && m_process->state() != QProcess::Starting)) {
        qWarning() << "[USI] process not ready; command ignored:" << command;
        return;
    }

    m_process->write((command + "\n").toUtf8());

    const QString pfx = logPrefix();
    if (m_model) {
        m_model->appendUsiCommLog(pfx + " > " + command);
    }
    qDebug().nospace() << pfx << " usidebug> " << command;
}

void Usi::sendUsiCommand()
{
    sendCommand("usi");
}

void Usi::sendIsReadyCommand()
{
    sendCommand("isready");
}

void Usi::sendUsiNewGameCommand()
{
    sendCommand("usinewgame");
}

void Usi::sendPositionCommand(QString& positionStr)
{
    sendCommand(positionStr);
}

void Usi::sendRaw(const QString& command) const
{
    if (!m_process) {
        qWarning() << "Usi::sendRaw called but m_process is null.";
        return;
    }
    sendCommand(command);
}

void Usi::sendStopCommand()
{
    sendCommand("stop");
    emit stopOrPonderhitCommandSent();
}

void Usi::sendPonderHitCommand()
{
    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();

    qDebug().nospace() << logPrefix()
                       << " PONDERHIT session=" << m_ponderSession
                       << " predicted=" << (m_predictedOpponentMove.isEmpty() ? "-" : m_predictedOpponentMove)
                       << " prevTo=(" << m_previousFileTo << "," << m_previousRankTo << ")";

    sendCommand("ponderhit");
    emit stopOrPonderhitCommandSent();
    m_phase = SearchPhase::Main;
}

void Usi::sendGoPonderCommand()
{
    infoRecordClear();
    m_phase = SearchPhase::Ponder;
    ++m_ponderSession;

    qDebug().nospace() << logPrefix()
                       << " start at go ponder  predicted="
                       << (m_predictedOpponentMove.isEmpty() ? "-" : m_predictedOpponentMove);

    sendCommand("go ponder");
    clearResponseData();
}

void Usi::sendGameOverCommand(const QString& result)
{
    sendCommand("gameover " + result);
    clearResponseData();
}

void Usi::sendQuitCommand()
{
    if (!m_process) return;

    cancelCurrentOperation();

    m_shutdownState = ShutdownState::IgnoreAllExceptInfoString;
    m_postQuitInfoStringLinesLeft = 1;

    sendCommand(QStringLiteral("quit"));

    (void)m_process->readAllStandardOutput();
    (void)m_process->readAllStandardError();
    m_process->closeWriteChannel();
}

void Usi::sendGameOverLoseAndQuitCommands()
{
    if (!m_process || m_process->state() != QProcess::Running) return;

    if (!m_gameoverSent) {
        sendGameOverCommand(QStringLiteral("lose"));
        m_gameoverSent = true;
    }

    sendQuitCommand();

    m_shutdownState = ShutdownState::IgnoreAllExceptInfoString;
    m_postQuitInfoStringLinesLeft = 1;
    (void)m_process->readAllStandardOutput();
    (void)m_process->readAllStandardError();
    m_process->closeWriteChannel();
}

void Usi::sendGameOverWinAndQuitCommands()
{
    if (!m_process || m_process->state() != QProcess::Running) return;

    if (!m_gameoverSent) {
        sendGameOverCommand(QStringLiteral("win"));
        m_gameoverSent = true;
    }

    sendQuitCommand();

    m_shutdownState = ShutdownState::IgnoreAllExceptInfoString;
    m_postQuitInfoStringLinesLeft = 1;
    (void)m_process->readAllStandardOutput();
    (void)m_process->readAllStandardError();
    m_process->closeWriteChannel();
}

void Usi::sendGoCommand(int byoyomiMilliSec, const QString& btime, const QString& wtime,
                        int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    infoRecordClear();

    QString command;
    if (useByoyomi) {
        command = "go btime " + btime + " wtime " + wtime + " byoyomi " + QString::number(byoyomiMilliSec);
    } else {
        command = "go btime " + btime + " wtime " + wtime
                  + " binc " + QString::number(addEachMoveMilliSec1)
                  + " winc " + QString::number(addEachMoveMilliSec2);
    }

    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    qDebug().nospace() << logPrefix()
                       << " start at go  btime=" << btime
                       << " wtime=" << wtime
                       << " byoyomi=" << byoyomiMilliSec << "ms";

    sendCommand(command);
}

// === 待機メソッド（QEventLoopベース）===

/**
 * @brief usiokを受信するまで待機する
 *
 * ビジーウェイトの代わりにQEventLoopを使用して、
 * GUIの応答性を維持しながら待機する。
 */
bool Usi::waitForUsiOK(int timeoutMilliseconds)
{
    if (m_usiOkSignalReceived) return true;

    m_usiOkSignalReceived = false;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    // タイムアウト時にループ終了
    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);

    // usiok受信時にループ終了
    connect(this, &Usi::usiOkReceived, &loop, &QEventLoop::quit);

    timer.start(timeoutMilliseconds);
    loop.exec();

    return m_usiOkSignalReceived;
}

/**
 * @brief readyokを受信するまで待機する
 */
bool Usi::waitForReadyOk(int timeoutMilliseconds)
{
    if (m_readyOkSignalReceived) return true;

    m_readyOkSignalReceived = false;

    QEventLoop loop;
    QTimer timer;
    timer.setSingleShot(true);

    connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
    connect(this, &Usi::readyOkReceived, &loop, &QEventLoop::quit);

    timer.start(timeoutMilliseconds);
    loop.exec();

    return m_readyOkSignalReceived;
}

/**
 * @brief bestmoveを受信するまで待機する
 *
 * 10msごとのネストイベントループで通常イベントを回し、
 * ShogiClock等のタイマ遅延を起こしにくくする。
 */
bool Usi::waitForBestMove(int timeoutMilliseconds)
{
    if (m_bestMoveSignalReceived) return true;
    if (timeoutMilliseconds <= 0) return false;

    m_bestMoveSignalReceived = false;

    QElapsedTimer timer;
    timer.start();

    static constexpr int kSliceMs = 10;

    while (!m_bestMoveSignalReceived) {
        if (timer.elapsed() >= timeoutMilliseconds) {
            return false;
        }
        if (m_shutdownState != ShutdownState::Running) {
            return false;
        }

        // 10msだけイベントを回す
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

bool Usi::waitForBestMoveWithGrace(int capMs, int graceMs)
{
    QElapsedTimer t;
    t.start();
    const qint64 hard = capMs + qMax(0, graceMs);

    m_bestMoveSignalReceived = false;
    const quint64 expectedId = m_seq;

    while (t.elapsed() < hard) {
        if (m_seq != expectedId || shouldAbortWait()) return false;

        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (m_bestMoveSignalReceived) return true;

        QThread::msleep(1);
    }
    return m_bestMoveSignalReceived;
}

bool Usi::keepWaitingForBestMove()
{
    m_bestMoveSignalReceived = false;

    while (!m_bestMoveSignalReceived) {
        // QEventLoopベースの待機
        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(10);
        QObject::connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        tick.start();
        spin.exec(QEventLoop::AllEvents);

        if (m_shutdownState != ShutdownState::Running) {
            return false;
        }
    }

    return true;
}

void Usi::waitForStopOrPonderhitCommand()
{
    QEventLoop loop;
    connect(this, &Usi::stopOrPonderhitCommandSent, &loop, &QEventLoop::quit);
    loop.exec();
}

void Usi::sendUsiCommandAndWaitForUsiOk()
{
    sendUsiCommand();

    if (!waitForUsiOK(5000)) {
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::sendUsiCommandAndWaitForUsiOk. Timeout waiting for usiok.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

void Usi::sendIsReadyCommandAndWaitForReadyOk()
{
    sendIsReadyCommand();

    if (!waitForReadyOk(5000)) {
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::sendIsReadyCommandAndWaitForReadyOk. Timeout waiting for readyok.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

// === データ受信・処理 ===

void Usi::readFromEngine()
{
    if (!m_process) {
        qWarning() << "[USI] readFromEngine called but m_process is null; ignoring.";
        return;
    }

    static constexpr int kMaxLinesPerChunk = 64;
    int processed = 0;

    while (m_process && m_process->canReadLine() && processed < kMaxLinesPerChunk) {
        const QByteArray data = m_process->readLine();
        QString line = QString::fromUtf8(data).trimmed();
        if (line.isEmpty()) { ++processed; continue; }

        // エンジン名確定
        if (line.startsWith(QStringLiteral("id name "))) {
            const QString n = line.mid(8).trimmed();
            if (!n.isEmpty() && m_logEngineName.isEmpty())
                m_logEngineName = n;
        }

        // 終局/終了後の遮断ロジック
        if (m_timeoutDeclared) {
            qDebug().nospace() << logPrefix() << " [drop-after-timeout] " << line;
            ++processed;
            continue;
        }
        if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
            if (line.startsWith(QStringLiteral("info string")) &&
                m_postQuitInfoStringLinesLeft > 0) {
                const QString pfx = logPrefix();
                if (m_model) {
                    m_model->appendUsiCommLog(pfx + " < " + line);
                }
                qDebug().nospace() << pfx << " usidebug< " << line;
                --m_postQuitInfoStringLinesLeft;
            }
            ++processed;
            continue;
        }
        if (m_shutdownState == ShutdownState::IgnoreAll) {
            qDebug().nospace() << logPrefix() << " [drop-ignore-all] " << line;
            ++processed;
            continue;
        }

        // 通常処理
        const QString pfx = logPrefix();
        m_lines.append(line);

        if (m_model) {
            m_model->appendUsiCommLog(pfx + " < " + line);
        }
        qDebug().nospace() << pfx << " usidebug< " << line;

        // 詰将棋の最終結果
        if (line.startsWith(QStringLiteral("checkmate"))) {
            handleCheckmateLine(line);
            ++processed;
            continue;
        }

        // bestmove/info等
        if (line.startsWith(QStringLiteral("bestmove"))) {
            m_bestMoveSignalReceived = true;
            onBestMoveReceived(line);
            emit bestMoveReceived();  // 待機用シグナル
        } else if (line.startsWith(QStringLiteral("info"))) {
            m_infoSignalReceived = true;
            infoReceived(line);
        } else if (line.contains(QStringLiteral("readyok"))) {
            m_readyOkSignalReceived = true;
            emit readyOkReceived();  // 待機用シグナル
        } else if (line.contains(QStringLiteral("usiok"))) {
            m_usiOkSignalReceived = true;
            emit usiOkReceived();  // 待機用シグナル
        }

        ++processed;
    }

    // まだ読み残しがある場合
    if (m_process && m_process->canReadLine()) {
        QTimer::singleShot(0, this, &Usi::readFromEngine);
    }
}

void Usi::readFromEngineStderr()
{
    if (!m_process) return;

    if (m_shutdownState == ShutdownState::IgnoreAll) {
        (void)m_process->readAllStandardError();
        return;
    }

    while (m_process->bytesAvailable() || m_process->canReadLine()) {
        const QByteArray data = m_process->readAllStandardError();
        if (data.isEmpty()) break;
        const QStringList lines = QString::fromUtf8(data).split('\n', Qt::SkipEmptyParts);

        for (const QString& l : lines) {
            const QString line = l.trimmed();
            if (line.isEmpty()) continue;

            if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
                if (m_postQuitInfoStringLinesLeft > 0 && shouldLogAfterQuit(line)) {
                    const QString pfx = logPrefix();
                    if (m_model) {
                        m_model->appendUsiCommLog(pfx + " <stderr> " + line);
                    }
                    qDebug().nospace() << pfx << " usidebug<stderr> " << line;

                    if (--m_postQuitInfoStringLinesLeft <= 0) {
                        m_shutdownState = ShutdownState::IgnoreAll;
                    }
                }
                continue;
            }

            const QString pfx = logPrefix();
            if (m_model) {
                m_model->appendUsiCommLog(pfx + " <stderr> " + line);
            }
            qDebug().nospace() << pfx << " usidebug<stderr> " << line;
        }
    }
}

void Usi::clearResponseData()
{
    m_lines.clear();
}

// === info処理（バッファリング方式）===

/**
 * @brief info行を受信した時の処理
 *
 * 動的プロパティの代わりに正式なメンバ変数を使用し、
 * 50msごとにバッファをフラッシュしてGUIを更新する。
 */
void Usi::infoReceived(QString& line)
{
    if (m_shutdownState != ShutdownState::Running) return;

    // バッファに追加
    m_infoBuffer.append(line);

    // まだフラッシュがスケジュールされていなければ予約
    if (!m_infoFlushScheduled) {
        m_infoFlushScheduled = true;
        QTimer::singleShot(50, this, &Usi::flushInfoBuffer);
    }
}

/**
 * @brief infoバッファをフラッシュしてGUIを更新する
 */
void Usi::flushInfoBuffer()
{
    m_infoFlushScheduled = false;

    if (m_shutdownState != ShutdownState::Running) {
        m_infoBuffer.clear();
        return;
    }

    // バッファを効率的にスワップ
    QStringList batch;
    batch.swap(m_infoBuffer);

    if (batch.isEmpty()) return;

    for (const QString& line : batch) {
        if (m_shutdownState != ShutdownState::Running) break;
        if (line.isEmpty()) continue;

        processInfoLine(line);
    }
}

/**
 * @brief 単一のinfo行を処理する
 *
 * メモリリークを防ぐためスマートポインタを使用。
 */
void Usi::processInfoLine(const QString& line)
{
    int scoreInt = 0;

    // スマートポインタでメモリ管理
    auto info = std::make_unique<ShogiEngineInfoParser>();

    info->setPreviousFileTo(m_previousFileTo);
    info->setPreviousRankTo(m_previousRankTo);

    // const_castを避けるためlineをコピー
    QString lineCopy = line;
    info->parseEngineOutputAndUpdateState(
        lineCopy,
        m_gameController,
        m_clonedBoardData,
        m_isPonderEnabled
    );

    // GUI項目の更新
    updateSearchedHand(info.get());
    updateDepth(info.get());
    updateNodes(info.get());
    updateNps(info.get());
    updateHashfull(info.get());

    // 評価値/詰み手数の更新
    updateEvaluationInfo(info.get(), scoreInt);

    // 思考タブへ追記
    if (!info->time().isEmpty() || !info->depth().isEmpty() ||
        !info->nodes().isEmpty() || !info->score().isEmpty() ||
        !info->pvKanjiStr().isEmpty()) {

        if (m_modelThinking) {
            m_modelThinking->prependItem(
                new ShogiInfoRecord(info->time(),
                                    info->depth(),
                                    info->nodes(),
                                    info->score(),
                                    info->pvKanjiStr()));
        }

        // シグナルも発行（View層への直接依存を減らすオプション）
        emit thinkingInfoUpdated(info->time(), info->depth(),
                                 info->nodes(), info->score(),
                                 info->pvKanjiStr());
    }
}

// === GUI更新ヘルパメソッド ===

void Usi::updateSearchedHand(const ShogiEngineInfoParser* info)
{
    if (m_model) {
        m_model->setSearchedMove(info->searchedHand());
    }
}

void Usi::updateDepth(const ShogiEngineInfoParser* info)
{
    if (!m_model) return;

    if (info->seldepth().isEmpty()) {
        m_model->setSearchDepth(info->depth());
    } else {
        m_model->setSearchDepth(info->depth() + "/" + info->seldepth());
    }
}

void Usi::updateNodes(const ShogiEngineInfoParser* info)
{
    if (!m_model) return;

    unsigned long long nodes = info->nodes().toULongLong();
    m_model->setNodeCount(m_locale.toString(nodes));
}

void Usi::updateNps(const ShogiEngineInfoParser* info)
{
    if (!m_model) return;

    unsigned long long nps = info->nps().toULongLong();
    m_model->setNodesPerSecond(m_locale.toString(nps));
}

void Usi::updateHashfull(const ShogiEngineInfoParser* info)
{
    if (!m_model) return;

    if (info->hashfull().isEmpty()) {
        m_model->setHashUsage("");
    } else {
        unsigned long long hash = info->hashfull().toULongLong() / 10;
        m_model->setHashUsage(QString::number(hash) + "%");
    }
}

// === 評価値処理 ===

int Usi::calculateScoreInt(const ShogiEngineInfoParser* info) const
{
    int scoreInt = 0;

    if ((info->scoreMate().toLongLong() > 0) || (info->scoreMate() == "+")) {
        scoreInt = 2000;
    } else if ((info->scoreMate().toLongLong() < 0) || (info->scoreMate() == "-")) {
        scoreInt = -2000;
    }

    return scoreInt;
}

void Usi::updateScoreMateAndLastScore(ShogiEngineInfoParser* info, int& scoreInt)
{
    if (info->scoreMate().isEmpty()) {
        m_lastScoreCp = 0;
    } else {
        scoreInt = calculateScoreInt(info);

        QString scoreMate = info->scoreMate();

        if ((scoreMate == "+") || (scoreMate == "-")) {
            scoreMate = "詰";
        } else {
            scoreMate += "手詰";
        }

        info->setScore(scoreMate);
        m_scoreStr = info->scoreMate();
        m_lastScoreCp = scoreInt;
    }
}

void Usi::updateAnalysisModeAndScore(const ShogiEngineInfoParser* info, int& scoreInt)
{
    if (m_analysisMode) {
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            scoreInt = info->scoreCp().toInt();
            m_scoreStr = info->scoreCp();
        } else if (m_gameController->currentPlayer() == ShogiGameController::Player2) {
            scoreInt = -info->scoreCp().toInt();
            m_scoreStr = QString::number(scoreInt);
        }
    } else {
        m_scoreStr = info->scoreCp();
        scoreInt = m_scoreStr.toInt();

        if (info->evaluationBound() == ShogiEngineInfoParser::EvaluationBound::LowerBound) {
            m_scoreStr += "++";
        } else if (info->evaluationBound() == ShogiEngineInfoParser::EvaluationBound::UpperBound) {
            m_scoreStr += "--";
        }
    }
}

void Usi::updateLastScore(int scoreInt)
{
    if (scoreInt > 2000) {
        m_lastScoreCp = 2000;
    } else if (scoreInt < -2000) {
        m_lastScoreCp = -2000;
    } else {
        m_lastScoreCp = scoreInt;
    }
}

void Usi::updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreInt)
{
    if (info->scoreCp().isEmpty()) {
        updateScoreMateAndLastScore(info, scoreInt);
    } else {
        updateAnalysisModeAndScore(info, scoreInt);
        info->setScore(m_scoreStr);
        setPvKanjiStr(info->pvKanjiStr());
        updateLastScore(scoreInt);
    }
}

void Usi::infoRecordClear()
{
    if (m_modelThinking) {
        m_modelThinking->clearAllItems();
    }
}
// === usi.cpp Part 2 ===

// === bestmove処理 ===

void Usi::onBestMoveReceived(const QString& line)
{
    const quint64 observedId = m_seq;

    if (m_timeoutDeclared || m_shutdownState != ShutdownState::Running) {
        qDebug().nospace() << logPrefix() << " [drop-bestmove] (inactive-state) " << line;
        return;
    }

    const QStringList tokens = line.split(whitespaceRe(), Qt::SkipEmptyParts);
    const int bestMoveIndex = tokens.indexOf(QStringLiteral("bestmove"));

    if (bestMoveIndex == -1 || bestMoveIndex + 1 >= tokens.size()) {
        const QString msg = tr("An error occurred in Usi::onBestMoveReceived. bestmove or its succeeding string not found.");
        qWarning().noquote() << logPrefix() << " " << msg << " line=" << line;
        emit errorOccurred(msg);
        cancelCurrentOperation();
        return;
    }

    if (observedId != m_seq) {
        qDebug().nospace() << logPrefix() << " [drop-bestmove] (op-id-mismatch) " << line;
        return;
    }

    m_bestMove = tokens.at(bestMoveIndex + 1);

    qint64 elapsed = m_goTimer.isValid() ? m_goTimer.elapsed() : -1;
    m_lastGoToBestmoveMs = (elapsed >= 0) ? elapsed : 0;
    m_bestMoveSignalReceived = true;

    if (m_bestMove.compare(QStringLiteral("resign"), Qt::CaseInsensitive) == 0) {
        if (m_resignNotified) {
            qDebug().nospace() << logPrefix() << " [dup-resign-ignored]";
            return;
        }
        m_resignNotified = true;
        m_isResignMove = true;
        qDebug().nospace() << logPrefix() << " [TRACE] resign-detected t+" << ShogiUtils::nowMs() << "ms";
        emit bestMoveResignReceived();
        return;
    }

    const int ponderIdx = tokens.indexOf(QStringLiteral("ponder"));
    if (ponderIdx != -1 && ponderIdx + 1 < tokens.size()) {
        m_predictedOpponentMove = tokens.at(ponderIdx + 1);
        qDebug().nospace() << logPrefix() << " [ponder] " << m_predictedOpponentMove;
    } else {
        m_predictedOpponentMove.clear();
    }
}

// === 指し手解析 ===

void Usi::parseMoveFrom(const QString& move, int& fileFrom, int& rankFrom)
{
    if (QStringLiteral("123456789").contains(move[0])) {
        fileFrom = move[0].digitValue();
        rankFrom = alphabetToRank(move[1]);
        return;
    }

    if (QStringLiteral("PLNSGBR").contains(move[0]) && move[1] == '*') {
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            fileFrom = SENTE_HAND_FILE;
            rankFrom = pieceToRankBlack(move[0]);
        } else {
            fileFrom = GOTE_HAND_FILE;
            rankFrom = pieceToRankWhite(move[0]);
        }
        return;
    }

    cleanupEngineProcessAndThread();
    const QString errorMessage =
        tr("An error occurred in Usi::parseMoveFrom. Invalid move format in moveFrom.");
    emit errorOccurred(errorMessage);
    cancelCurrentOperation();
}

void Usi::parseMoveTo(const QString& move, int& fileTo, int& rankTo)
{
    if (!move[2].isDigit() || !move[3].isLetter()) {
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::parseMoveTo. Invalid move format in moveTo.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    fileTo = move[2].digitValue();
    rankTo = alphabetToRank(move[3]);
}

void Usi::parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo)
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
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::parseMoveCoordinates. Invalid bestmove format: \"%1\"").arg(move);
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    const bool promote = (move.size() >= 5 && move.at(4) == QLatin1Char('+'));
    if (m_gameController) m_gameController->setPromote(promote);

    parseMoveFrom(move, fileFrom, rankFrom);
    if (fileFrom < 0 || rankFrom < 0) return;

    parseMoveTo(move, fileTo, rankTo);
}

// === 指し手変換 ===

QString Usi::convertBoardMoveToUsi(int fileFrom, int rankFrom, int fileTo, int rankTo, bool promote) const
{
    QString bestMove = QString::number(fileFrom) + rankToAlphabet(rankFrom);
    bestMove += QString::number(fileTo) + rankToAlphabet(rankTo);
    if (promote) bestMove += "+";
    return bestMove;
}

QString Usi::convertDropMoveToUsi(int fileFrom, int rankFrom, int fileTo, int rankTo) const
{
    QString bestMove;

    if (fileFrom == SENTE_HAND_FILE) {
        bestMove = convertFirstPlayerPieceNumberToSymbol(rankFrom);
    } else if (fileFrom == GOTE_HAND_FILE) {
        bestMove = convertSecondPlayerPieceNumberToSymbol(rankFrom);
    }

    bestMove += QString::number(fileTo) + rankToAlphabet(rankTo);
    return bestMove;
}

QString Usi::convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote)
{
    const int fileFrom = outFrom.x();
    const int rankFrom = outFrom.y();
    const int fileTo = outTo.x();
    const int rankTo = outTo.y();

    QString bestMove;

    if (fileFrom >= 1 && fileFrom <= BOARD_SIZE) {
        bestMove = convertBoardMoveToUsi(fileFrom, rankFrom, fileTo, rankTo, promote);
    } else if (fileFrom == SENTE_HAND_FILE || fileFrom == GOTE_HAND_FILE) {
        bestMove = convertDropMoveToUsi(fileFrom, rankFrom, fileTo, rankTo);
    } else {
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::convertHumanMoveToUsiFormat. Invalid fileFrom.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return QString();
    }

    return bestMove;
}
// === usi.cpp Part 3 ===

// === 盤面処理 ===

void Usi::cloneCurrentBoardData()
{
    m_clonedBoardData = m_gameController->board()->boardData();
}

void Usi::applyMovesToBoardFromBestMoveAndPonder()
{
    ShogiEngineInfoParser info;
    info.parseAndApplyMoveToClonedBoard(m_bestMove, m_clonedBoardData);
    info.parseAndApplyMoveToClonedBoard(m_predictedOpponentMove, m_clonedBoardData);
}

void Usi::printShogiBoard(const QVector<QChar>& boardData) const
{
    if (boardData.size() != NUM_BOARD_SQUARES) {
        qDebug() << "無効な盤面データ";
        return;
    }

    for (int rank = 0; rank < BOARD_SIZE; ++rank) {
        for (int file = BOARD_SIZE - 1; file >= 0; --file) {
            QChar piece = boardData[rank * BOARD_SIZE + file];
            if (piece == ' ') {
                std::cout << "  ";
            } else {
                std::cout << piece.toLatin1() << ' ';
            }
        }
        std::cout << std::endl;
    }
    std::cout << "----------------------------------------" << std::endl;
}

// === 通信処理 ===

void Usi::waitAndCheckForBestMove(int time)
{
    if (!waitForBestMove(time)) {
        const QString errorMessage =
            tr("An error occurred in Usi::waitAndCheckForBestMove. Timeout waiting for bestmove.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

void Usi::waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec,
                                               const QString& btime,
                                               const QString& wtime,
                                               bool useByoyomi)
{
    if (shouldAbortWait()) {
        qDebug().nospace() << logPrefix() << " [wait-abort] timeout/ignore declared";
        return;
    }

    const bool p1turn = (m_gameController->currentPlayer() == ShogiGameController::Player1);
    const int mainMs = p1turn ? btime.toInt() : wtime.toInt();
    int capMs = useByoyomi ? (mainMs + byoyomiMilliSec) : mainMs;
    if (capMs >= 200) capMs -= 100;

    qDebug().nospace()
        << "[USI] waitBestmove  turn=" << (p1turn ? "P1" : "P2")
        << " cap=" << capMs << "ms"
        << " (main=" << mainMs
        << ", byoyomi=" << byoyomiMilliSec
        << ", mode=" << (useByoyomi ? "byoyomi" : "increment") << ")";

    const bool got = waitForBestMoveWithGrace(capMs, kBestmoveGraceMs);

    if (!got) {
        if (shouldAbortWait()) {
            qDebug().nospace() << logPrefix() << " [wait-abort-2] timeout/quit during wait";
            return;
        }
        const QString errorMessage =
            tr("An error occurred in Usi::waitAndCheckForBestMoveRemainingTime. Timeout waiting for bestmove.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

void Usi::startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr)
{
    if (!m_predictedOpponentMove.isEmpty() && m_isPonderEnabled) {
        applyMovesToBoardFromBestMoveAndPonder();

        ensureMovesKeyword(positionStr);
        positionPonderStr = positionStr + " " + m_predictedOpponentMove;

        sendPositionCommand(positionPonderStr);
        sendGoPonderCommand();
    }
}

void Usi::appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr)
{
    ensureMovesKeyword(positionStr);
    positionStr += " " + m_bestMove;
    startPonderingAfterBestMove(positionStr, positionPonderStr);
}

void Usi::sendCommandsAndProcess(
    int byoyomiMilliSec, QString& positionStr, const QString& btime, const QString& wtime,
    QString& positionPonderStr, int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    const quint64 opId = beginOperationContext();

    const QString posPreview = (positionStr.size() > 200)
                                   ? (positionStr.left(200) + QStringLiteral(" ..."))
                                   : positionStr;
    qInfo() << "[USI] send position+go"
            << "opId=" << opId
            << "position=" << posPreview
            << "byoyomiMs=" << byoyomiMilliSec
            << "btime=" << btime << "wtime=" << wtime
            << "inc1=" << addEachMoveMilliSec1 << "inc2=" << addEachMoveMilliSec2
            << "useByoyomi=" << useByoyomi;

    sendPositionCommand(positionStr);
    cloneCurrentBoardData();
    sendGoCommand(byoyomiMilliSec, btime, wtime, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

    waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, useByoyomi);

    if (m_isResignMove) return;

    appendBestMoveAndStartPondering(positionStr, positionPonderStr);
}

void Usi::sendPositionAndGoCommands(int byoyomiMilliSec, QString& positionStr)
{
    sendPositionCommand(positionStr);
    sendGoCommandByAnalysys(byoyomiMilliSec);
}

void Usi::sendGoCommandByAnalysys(int byoyomiMilliSec)
{
    const quint64 opId = beginOperationContext();

    infoRecordClear();
    sendCommand("go infinite");

    if (byoyomiMilliSec <= 0) {
        keepWaitingForBestMove();
        return;
    }

    auto* timer = new QTimer(m_opCtx);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, m_opCtx, [this]() {
        sendCommand("stop");
    });
    timer->start(byoyomiMilliSec);

    static constexpr int kPostStopGraceMs = 4000;
    const int waitBudget = qMax(byoyomiMilliSec + kPostStopGraceMs, 2500);

    if (!waitForBestMove(waitBudget)) {
        auto* chase = new QTimer(m_opCtx);
        chase->setSingleShot(true);
        connect(chase, &QTimer::timeout, m_opCtx, [this]() { sendCommand("stop"); });
        chase->start(0);

        if (!waitForBestMove(2000)) {
            qWarning() << "[USI][Analysis] bestmove timeout (non-fatal). waited(ms)="
                       << (waitBudget + 2000);
            return;
        }
    }
}

void Usi::processEngineResponse(QString& positionStr, QString& positionPonderStr, int byoyomiMilliSec,
                                const QString& btime, const QString& wtime,
                                int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    if (m_predictedOpponentMove.isEmpty() || !m_isPonderEnabled) {
        sendCommandsAndProcess(byoyomiMilliSec, positionStr, btime, wtime,
                               positionPonderStr, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
        if (m_isResignMove) return;
    } else {
        if (m_bestMove == m_predictedOpponentMove) {
            cloneCurrentBoardData();
            sendPonderHitCommand();

            if (byoyomiMilliSec == 0) {
                keepWaitingForBestMove();
            } else {
                waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, useByoyomi);
            }
            if (m_isResignMove) return;

            appendBestMoveAndStartPondering(positionStr, positionPonderStr);
        } else {
            qDebug().nospace() << logPrefix()
                               << " PONDER MISMATCH → send stop"
                               << " predicted=" << m_predictedOpponentMove
                               << " actual=" << m_bestMove;

            sendStopCommand();

            if (byoyomiMilliSec == 0) {
                keepWaitingForBestMove();
            } else {
                waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, useByoyomi);
            }
            if (m_isResignMove) return;

            sendCommandsAndProcess(byoyomiMilliSec, positionStr, btime, wtime,
                                   positionPonderStr, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
            if (m_isResignMove) return;
        }
    }
}

void Usi::executeEngineCommunication(QString& positionStr, QString& positionPonderStr, QPoint& outFrom,
                                     QPoint& outTo, int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                     int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    processEngineResponse(positionStr, positionPonderStr, byoyomiMilliSec, btime, wtime,
                          addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

    if (m_isResignMove) return;

    int fileFrom, rankFrom, fileTo, rankTo;
    parseMoveCoordinates(fileFrom, rankFrom, fileTo, rankTo);

    outFrom = QPoint(fileFrom, rankFrom);
    outTo = QPoint(fileTo, rankTo);
}

void Usi::handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                           QPoint& outFrom, QPoint& outTo,
                                           int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                           QStringList& positionStrList, int addEachMoveMilliSec1,
                                           int addEachMoveMilliSec2, bool useByoyomi)
{
    qDebug() << "[USI][HvE] enter handleHumanVsEngineCommunication"
             << "byoyomiMs=" << byoyomiMilliSec
             << "btime=" << btime << "wtime=" << wtime;

    const QString prePreview = (positionStr.size() > 200)
                                   ? (positionStr.left(200) + QStringLiteral(" ..."))
                                   : positionStr;
    qDebug() << "[USI][HvE] position(before ensureMoves/bestmove) =" << prePreview;

    m_bestMove = convertHumanMoveToUsiFormat(outFrom, outTo, m_gameController->promote());

    ensureMovesKeyword(positionStr);
    positionStr += " " + m_bestMove;
    positionStrList.append(positionStr);

    const QString postPreview = (positionStr.size() > 200)
                                    ? (positionStr.left(200) + QStringLiteral(" ..."))
                                    : positionStr;
    qDebug() << "[USI][HvE] position(after append bestmove) =" << postPreview;

    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo,
                               byoyomiMilliSec, btime, wtime,
                               addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
}

void Usi::handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr, QString& positionPonderStr,
                                                        QPoint& outFrom, QPoint& outTo,
                                                        int byoyomiMilliSec, const QString& btime,
                                                        const QString& wtime,
                                                        int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                                        bool useByoyomi)
{
    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo, byoyomiMilliSec, btime, wtime,
                               addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
}

void Usi::executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec)
{
    cloneCurrentBoardData();
    sendPositionAndGoCommands(byoyomiMilliSec, positionStr);
}

// === 設定関連 ===

void Usi::generateSetOptionCommands(const QString& engineName)
{
    QDir::setCurrent(QApplication::applicationDirPath());

    QSettings settings(SettingsFileName, QSettings::IniFormat);

    int settingCount = settings.beginReadArray(engineName);

    m_isPonderEnabled = false;

    for (int i = 0; i < settingCount; ++i) {
        settings.setArrayIndex(i);

        QString settingName = settings.value("name").toString();
        QString settingValue = settings.value("value").toString();
        QString settingType = settings.value("type").toString();

        if (settingType == "button" && settingValue == "on") {
            m_setOptionCommandList.append("setoption name " + settingName);
        } else {
            m_setOptionCommandList.append("setoption name " + settingName + " value " + settingValue);

            if (settingName == "USI_Ponder") {
                m_isPonderEnabled = (settingValue == "true");
            }
        }
    }

    settings.endArray();
}

void Usi::sendSetOptionCommands()
{
    for (const QString& command : m_setOptionCommandList) {
        sendCommand(command);
    }
}

// === ログ・状態管理 ===

void Usi::setLogIdentity(const QString& engineTag, const QString& sideTag, const QString& engineName)
{
    m_logEngineTag = engineTag;
    m_logSideTag = sideTag;
    m_logEngineName = engineName;
}

QString Usi::phaseTag() const
{
    switch (m_phase) {
    case SearchPhase::Ponder: return QString("[PONDER#%1]").arg(m_ponderSession);
    case SearchPhase::Main:   return QString("[MAIN]");
    default:                  return QString();
    }
}

QString Usi::logPrefix() const
{
    QString within = m_logEngineTag.isEmpty() ? "[E?]" : m_logEngineTag;
    if (!m_logSideTag.isEmpty())
        within.insert(within.size() - 1, "/" + m_logSideTag);

    QString head = within + (m_logEngineName.isEmpty() ? "" : " " + m_logEngineName);

    const QString ph = phaseTag();
    return ph.isEmpty() ? head : (head + " " + ph);
}

bool Usi::shouldLogAfterQuit(const QString& line) const
{
    return line.startsWith(QStringLiteral("info string"));
}

void Usi::setSquelchResignLogging(bool on)
{
    m_squelchResignLogs = on;
}

bool Usi::shouldAbortWait() const
{
    if (m_timeoutDeclared) return true;
    if (m_shutdownState != ShutdownState::Running) return true;
    if (!m_process || m_process->state() != QProcess::Running) return true;
    return false;
}

// === オペレーションコンテキスト ===

quint64 Usi::beginOperationContext()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    m_opCtx = new QObject(this);
    return ++m_seq;
}

void Usi::cancelCurrentOperation()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    ++m_seq;
}

// === 詰将棋関連 ===

void Usi::sendPositionAndGoMate(const QString& sfen, int timeMs, bool infinite)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        const QString errorMessage = tr("USI engine is not running.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    sendCommand(QStringLiteral("position sfen %1").arg(sfen.trimmed()));

    if (infinite || timeMs <= 0) {
        sendCommand(QStringLiteral("go mate infinite"));
    } else {
        sendCommand(QStringLiteral("go mate %1").arg(timeMs));
    }

    m_modeTsume = true;
}

void Usi::sendStopForMate()
{
    if (!m_process || m_process->state() != QProcess::Running) return;
    sendCommand(QStringLiteral("stop"));
}

void Usi::executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec)
{
    cloneCurrentBoardData();
    sendPositionAndGoMateCommands(mateLimitMilliSec, positionStr);
}

void Usi::sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr)
{
    sendPositionCommand(positionStr);

    if (mateLimitMilliSec <= 0) {
        sendCommand(QStringLiteral("go mate infinite"));
    } else {
        sendCommand(QStringLiteral("go mate %1").arg(mateLimitMilliSec));
    }
}

void Usi::handleCheckmateLine(const QString& line)
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
