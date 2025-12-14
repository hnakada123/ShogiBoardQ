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

#include "usi.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogienginethinkingmodel.h"
#include "playmode.h"
#include "enginesettingsconstants.h"
#include "shogiutils.h"

using namespace EngineSettingsConstants;

// QRegularExpression の関数ローカル static 版（clazy 警告回避）
static const QRegularExpression& whitespaceRe()
{
    static const QRegularExpression re(QStringLiteral("\\s+"));
    return re;
}

static inline QString nowIso()
{
    return QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
}

static inline QString fmtMs(qint64 ms)
{
    if (ms < 0) return QStringLiteral("n/a");
    const qint64 s   = ms / 1000;
    const qint64 rem = ms % 1000;
    return QString("%1.%2s").arg(s).arg(rem, 3, 10, QChar('0')); // 例: "1.234s"
}

// usi.cpp（ファイル先頭の他ヘルパ定義付近に追加）
static inline void ensureMovesKeyword(QString& s) {
    // 既に " moves" が含まれていなければ挿入
    if (!s.contains(QStringLiteral(" moves"))) {
        s = s.trimmed();
        s += QStringLiteral(" moves");
    }
}

// USIプロトコル通信により、将棋エンジンと通信を行うクラス
// コンストラクタ
Usi::Usi(UsiCommLogModel* model, ShogiEngineThinkingModel* modelThinking, ShogiGameController* gameController, PlayMode& playMode, QObject* parent)
    : QObject(parent),
    m_locale(QLocale::English),
    m_process(nullptr),
    m_model(model),
    m_modelThinking(modelThinking),
    m_gameController(gameController),
    m_playMode(playMode),
    m_usiThread(nullptr),
    m_isResignMove(false),
    m_isWinMove(false),
    m_analysisMode(false)
{
    m_shutdownState = ShutdownState::Running;      // NEW
    m_postQuitInfoStringLinesLeft = 0;             // NEW
    m_gameoverSent = false;
    m_quitSent     = false;
}

// デストラクタ
Usi::~Usi()
{
    cleanupEngineProcessAndThread();
}

// 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
void Usi::cleanupEngineProcessAndThread()
{
    // --- 1) エンジンプロセスの終了 ---
    if (m_process) {
        // 起動中ならまず quit を送り、段階的に待つ → terminate → kill
        if (m_process->state() == QProcess::Running) {
            // 将棋エンジンに quit コマンドを送信
            sendQuitCommand();

            // 通常はここで終了を待つ
            if (!m_process->waitForFinished(3000)) {
                // 閉じない場合は terminate
                m_process->terminate();
                if (!m_process->waitForFinished(1000)) {
                    // それでも閉じない場合は kill（最終手段）
                    m_process->kill();
                    // kill 後は確実に終了を待つ
                    m_process->waitForFinished(-1);
                }
            }
        }

        // 念のためすべての接続を切断してから破棄
        disconnect(m_process, nullptr, this, nullptr);
        delete m_process;
        m_process = nullptr;
    }

    // --- 2) USI スレッドの後始末 ---
    if (m_usiThread) {
        // ★ UsiThread::run() 側で QEventLoop の終了待ちになっている可能性があるため
        //    stop / ponderhit 相当の合図を emit して確実にループを抜けさせる
        emit stopOrPonderhitCommandSent();

        // 協調的停止のお願い（run() がこれを見るなら有効）
        m_usiThread->requestInterruption();

        // まずは短時間だけ待ってみる
        if (!m_usiThread->wait(500)) {
            // run() 内で QThread::exec() を使っている場合のために quit() も投げる
            m_usiThread->quit();

            // それでも止まらない場合は、段階的に強く止める
            if (!m_usiThread->wait(1000)) {
                qWarning() << "[USI] UsiThread did not stop after quit(); forcing termination.";
                // 最終手段：terminate（本来は避けたいが、アプリ終了時のハング回避を優先）
                m_usiThread->terminate();
                m_usiThread->wait(1000);
            }
        }

        delete m_usiThread;
        m_usiThread = nullptr;
    }
}

// 評価値の文字列を返す。
QString Usi::scoreStr() const
{
    return m_scoreStr;
}

// QProcessのエラーが発生したときに呼び出されるスロット
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

    // 安全に後片付け（プロセス/スレッド終了・切断）
    cleanupEngineProcessAndThread();

    // 例外は投げず、上位（MainWindow 等）へ通知して終了
    emit errorOccurred(errorMessage);

    // ★ ここで“打ち切り”を確定：オペ用コンテキストを破棄して以降の継続処理を無効化
    cancelCurrentOperation();
    return;
}

// 将棋エンジンプロセスを起動し、対局を開始するUSIコマンドを送受信する。
void Usi::initializeAndStartEngineCommunication(QString& engineFile, QString& enginename)
{
    // 将棋エンジンのファイルパスが空の場合
    if (engineFile.isEmpty()) {
        // 安全に後片付け（プロセス/スレッド終了・切断）
        cleanupEngineProcessAndThread();

        // エラーメッセージ通知 → 以後の処理を打ち切り
        QString errorMessage = tr("An error occurred in Usi::initializeAndStartEngineCommunication. Engine file path is empty.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    // エンジンのあるディレクトリへ移動（失敗時は当該関数内でemit→打ち切り）
    changeDirectoryToEnginePath(engineFile);

    // 将棋エンジンを起動し、初期化
    startAndInitializeEngine(engineFile, enginename);
}

// カレントディレクトリをエンジンファイルのあるディレクトリに移動する。
void Usi::changeDirectoryToEnginePath(const QString& engineFile)
{
    const QFileInfo fileInfo(engineFile);

    // カレントディレクトリの切り替えに失敗した場合は即打ち切り
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

// 将棋エンジンを起動し、対局開始に関するコマンドを送信する。
void Usi::startAndInitializeEngine(const QString& engineFile, const QString& enginename)
{
    // 将棋エンジンを起動する。
    startEngine(engineFile);

    // 将棋エンジンに対局開始に関するコマンドを送信する。
    sendInitialCommands(enginename);
}

// 将棋エンジンに対局開始に関するコマンドを送信する。
void Usi::sendInitialCommands(const QString& enginename)
{
    // 設定ファイルから将棋エンジンオプションを読み込み、setoptionコマンドの文字列を生成する。
    generateSetOptionCommands(enginename);

    // usiコマンドを将棋エンジンに送り、usiokを待機する。
    sendUsiCommandAndWaitForUsiOk();

    // setoptionコマンドを将棋エンジンに送信する。
    sendSetOptionCommands();

    // isreadyコマンドを将棋エンジンに送り、readyokを待機する。
    sendIsReadyCommandAndWaitForReadyOk();

    // usinewgameコマンドを将棋エンジンに送る。
    sendUsiNewGameCommand();
}

// usiコマンドを将棋エンジンに送り、usiokを待機する。
void Usi::sendUsiCommandAndWaitForUsiOk()
{
    sendUsiCommand();

    // 5秒待っても usiok が来なければ打ち切り
    if (!waitForUsiOK(5000)) {
        cleanupEngineProcessAndThread();

        const QString errorMessage =
            tr("An error occurred in Usi::sendUsiCommandAndWaitForUsiOk. Timeout waiting for usiok.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

// isreadyコマンドを将棋エンジンに送り、readyokを待機する。
void Usi::sendIsReadyCommandAndWaitForReadyOk()
{
    sendIsReadyCommand();

    // 5秒待っても readyok が来なければ打ち切り
    if (!waitForReadyOk(5000)) {
        cleanupEngineProcessAndThread();

        const QString errorMessage =
            tr("An error occurred in Usi::sendIsReadyCommandAndWaitForReadyOk. Timeout waiting for readyok.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

// 先手が持ち駒を打つときの駒を文字列に変換する。
// rankFromは持ち駒の駒番号
QString Usi::convertFirstPlayerPieceNumberToSymbol(const int rankFrom) const
{
    static const QMap<int, QString> firstPlayerPieceMap {
        {1, "P*"},
        {2, "L*"},
        {3, "N*"},
        {4, "S*"},
        {5, "G*"},
        {6, "B*"},
        {7, "R*"}
    };

    return firstPlayerPieceMap.value(rankFrom, "");
}

// 後手が持ち駒を打つときの駒を文字列に変換する。
// rankFromは持ち駒の駒番号
QString Usi::convertSecondPlayerPieceNumberToSymbol(const int rankFrom) const
{
    static const QMap<int, QString> secondPlayerPieceMap {
        {3, "R*"},
        {4, "B*"},
        {5, "G*"},
        {6, "S*"},
        {7, "N*"},
        {8, "L*"},
        {9, "P*"}
    };

    return secondPlayerPieceMap.value(rankFrom, "");
}

// 盤上の駒を動かす場合の指し手をUSI形式に変換する。
QString Usi::convertBoardMoveToUsi(int fileFrom, int rankFrom, int fileTo, int rankTo, bool promote) const
{
    // 動かす駒の筋と段からbestmoveの文字列を作成する。
    QString bestMove = QString::number(fileFrom) + rankToAlphabet(rankFrom);

    bestMove += QString::number(fileTo) + rankToAlphabet(rankTo);

    // 成る場合は"+"を付ける。
    if (promote) bestMove += "+";

    return bestMove;
}

// 持ち駒を打つ場合の指し手をUSI形式に変換する。
QString Usi::convertDropMoveToUsi(int fileFrom, int rankFrom, int fileTo, int rankTo) const
{
    QString bestMove;

    if (fileFrom == 10) {
        // 先手の持ち駒を打った場合の文字列に変換する。（例．銀を打つ場合はS*）
        bestMove = convertFirstPlayerPieceNumberToSymbol(rankFrom);
    } else if (fileFrom == 11) {
        // 後手の持ち駒を打った場合の文字列に変換する。（例．銀を打つ場合はS*）
        bestMove = convertSecondPlayerPieceNumberToSymbol(rankFrom);
    }

    // 指した先の筋と段からbestmoveの文字列を作成する。
    bestMove += QString::number(fileTo) + rankToAlphabet(rankTo);

    return bestMove;
}

// 人間の指し手をUSI形式の指し手に直す。
QString Usi::convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote)
{
    const int fileFrom = outFrom.x();
    const int rankFrom = outFrom.y();
    const int fileTo   = outTo.x();
    const int rankTo   = outTo.y();

    QString bestMove;

    if (fileFrom >= 1 && fileFrom <= 9) {
        bestMove = convertBoardMoveToUsi(fileFrom, rankFrom, fileTo, rankTo, promote);
    } else if (fileFrom == 10 || fileFrom == 11) {
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

// bestmove文字列（例: "7g7f", "P*5e"など）から移動元の座標（盤上の駒の場合）または持ち駒の種類（持ち駒を打つ場合）を解析する。
void Usi::parseMoveFrom(const QString& move, int& fileFrom, int& rankFrom)
{
    if (QStringLiteral("123456789").contains(move[0])) {
        fileFrom = move[0].digitValue();
        rankFrom = alphabetToRank(move[1]);
        return;
    }

    if (QStringLiteral("PLNSGBR").contains(move[0]) && move[1] == '*') {
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            fileFrom = 10; rankFrom = pieceToRankBlack(move[0]);
        } else {
            fileFrom = 11; rankFrom = pieceToRankWhite(move[0]);
        }
        return;
    }

    // 不正
    cleanupEngineProcessAndThread();
    const QString errorMessage =
        tr("An error occurred in Usi::parseMoveFrom. Invalid move format in moveFrom.");
    emit errorOccurred(errorMessage);
    cancelCurrentOperation();
    return;
}

// bestmove文字列（例: "7g7f", "P*5e"など）から移動先の座標を解析する。
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

// 将棋エンジンがbestmove文字列で返した最善手から移動元の筋と段、移動先の筋と段を取得する。
// USIのbestmove（例: "7g7f", "7g7f+", "P*7f", "resign" など）を分解し、
// m_gameController->setPromote(...) を必ず設定してから from/to 座標を返す。
// エラー時はUIへ通知して打ち切り（座標は -1 に初期化）。
void Usi::parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo)
{
    // デフォルト（エラー/特殊トークン時はこの値のまま戻す）
    fileFrom = rankFrom = fileTo = rankTo = -1;

    const QString move = m_bestMove.trimmed();
    const QString lmove = move.toLower();

    // 特殊トークン（勝敗/引き分けなど）
    if (lmove == QLatin1String("resign") ||
        lmove == QLatin1String("win")    ||
        lmove == QLatin1String("draw"))
    {
        if (m_gameController) m_gameController->setPromote(false);
        return;
    }

    // 最低長チェック（"7g7f" や "P*7f" で4文字。昇格なら5文字 "7g7f+"）
    if (move.size() < 4) {
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::parseMoveCoordinates. Invalid bestmove format: \"%1\"").arg(move);
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    // ★ 昇格フラグを先に決めておく（末尾が '+' なら昇格）
    const bool promote = (move.size() >= 5 && move.at(4) == QLatin1Char('+'));
    if (m_gameController) m_gameController->setPromote(promote);

    // from/to を分解（各関数は不正時に emit 済み・早期return）
    // ここでは初期値 -1 のままかどうかで失敗を判断できるようにしている。
    parseMoveFrom(move, fileFrom, rankFrom);
    if (fileFrom < 0 || rankFrom < 0) return;  // 既にエラー通知＆打ち切り済み

    parseMoveTo(move, fileTo, rankTo);
    if (fileTo < 0 || rankTo < 0) return;      // 既にエラー通知＆打ち切り済み
}

// bestmoveを指定した時間内に受信するまで待機する。
// 指定した時間内に受信できなかった場合、エラーメッセージを表示して例外をスローする。
void Usi::waitAndCheckForBestMove(const int time)
{
    if (!waitForBestMove(time)) {
        const QString errorMessage =
            tr("An error occurred in Usi::waitAndCheckForBestMove. Timeout waiting for bestmove.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

// 将棋エンジンからの「bestmove」受信後に予想手を考慮した以下の処理を開始する。
// 1. 「bestmove」と「ponder」の情報をもとに駒配置を更新。
// 2. 予想される相手の指し手をposition文字列に追加。
// 3. 更新したpositionコマンドをエンジンに送信。
// 4. ponderモードをセット。
// 5. エンジンに「go ponder」コマンドを送信。
// 6. "stop"または"ponderhit"コマンドの待機のため新しいスレッドを生成。
void Usi::startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr)
{
    // ponderモードで相手の指し手が予想できた場合
    if (!m_predictedOpponentMove.isEmpty() && m_isPonderEnabled) {
        // 将棋エンジンから返された「bestmove A ponder B」の情報をもとに、将棋盤内の駒配置を更新する。
        applyMovesToBoardFromBestMoveAndPonder();

        // position文字列に予想した相手の指し手を追加する。
        ensureMovesKeyword(positionStr);
        positionPonderStr = positionStr + " " + m_predictedOpponentMove;

        // positionコマンドを将棋エンジンに送信する。
        sendPositionCommand(positionPonderStr);

        // go ponderコマンドを将棋エンジンに送信する。
        sendGoPonderCommand();

        // 新しいスレッドを生成し、GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待つ。
        // start関数が実行されることでrun関数が実行され、その中でwaitForStopOrPonderhitCommand関数が実行される。
        startUsiThread();
    }
}

// 将棋エンジンからの最善手をposition文字列に追加し、予想手を考慮した処理を開始する。
// @param positionStr position文字列
// @param positionPonderStr position文字列に予想手を追加したもの
void Usi::appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr)
{
    // position文字列に将棋エンジンが返した最善手の文字列を追加する。
    ensureMovesKeyword(positionStr);
    positionStr += " " + m_bestMove;

    // 相手の指し手が予想できた場合、予想手を考慮した処理を開始する。
    startPonderingAfterBestMove(positionStr, positionPonderStr);
}

// positionコマンドを送信し、ponderモードをオフにして、goコマンドを送信し、
// bestmoveを受信して予想手を考慮した処理を開始する。
// @param positionStr positionコマンドの文字列
// @param byoyomiMilliSec bestmoveを待機する時間（ミリ秒単位）
// @param btime 黒の残り時間
// @param wtime 白の残り時間
// @param positionPonderStr position文字列に予想手を追加したもの
void Usi::sendCommandsAndProcess(
    int byoyomiMilliSec, QString& positionStr, const QString& btime, const QString& wtime,
    QString& positionPonderStr, int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // ★ このセット（position→go→bestmove）専用の ctx/ID を開始
    const quint64 opId = beginOperationContext();

    //begin
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
    //end

    // position → go
    sendPositionCommand(positionStr);
    cloneCurrentBoardData();
    sendGoCommand(byoyomiMilliSec, btime, wtime, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

    // 待機（内部で “IDが変わったら離脱” を判定）
    waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, useByoyomi);

    if (m_isResignMove) return;

    // bestmove 受信後の処理（ここで opId と m_seq が食い違っていたら
    // bestmoveReceived 側で弾かれているため、通常通り続行してOK）
    appendBestMoveAndStartPondering(positionStr, positionPonderStr);
}

// positionコマンドとgoコマンドを送信し、bestmoveを受信するまで待機する。
void Usi::sendPositionAndGoCommands(int byoyomiMilliSec, QString& positionStr)
{
    // positionコマンドを将棋エンジンに送信する。
    sendPositionCommand(positionStr);

    // goコマンドを将棋エンジンに送信し、bestmoveを受信するまで待機する。
    sendGoCommandByAnalysys(byoyomiMilliSec);
}

// 盤面データを9x9のマスに表示する。
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

// 残り時間になるまでbestmoveを待機する。
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
    const int  mainMs = p1turn ? btime.toInt() : wtime.toInt();
    int capMs = useByoyomi ? (mainMs + byoyomiMilliSec) : mainMs;
    if (capMs >= 200) capMs -= 100;

    qDebug().nospace()
        << "[USI] waitBestmove  turn=" << (p1turn ? "P1" : "P2")
        << " cap=" << capMs << "ms"
        << " (main=" << mainMs
        << ", byoyomi=" << byoyomiMilliSec
        << ", mode=" << (useByoyomi ? "byoyomi" : "increment") << ")";

    // ★ ここで ID も監視（waitForBestMoveWithGrace 内で expectedId をチェック）
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

bool Usi::waitForBestMoveWithGrace(int capMs, int graceMs)
{
    QElapsedTimer t; t.start();
    const qint64 hard = capMs + qMax(0, graceMs);

    m_bestMoveSignalReceived = false;

    // ★ この待機が属するオペの “期待ID”
    const quint64 expectedId = m_seq;

    while (t.elapsed() < hard) {
        // ★ オペがキャンセル/切替されたら即離脱（旧bestmoveは採用しない）
        if (m_seq != expectedId || shouldAbortWait()) return false;

        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);
        if (m_bestMoveSignalReceived) return true;

        QThread::msleep(1);
    }
    return m_bestMoveSignalReceived;
}

// 将棋エンジンからのレスポンスに基づいて、適切なコマンドを送信し、必要に応じて処理を行う。
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
            // ★ ミスマッチを明示
            qDebug().nospace() << logPrefix()
                               << " PONDER MISMATCH → send stop"
                               << " predicted=" << m_predictedOpponentMove
                               << " actual="    << m_bestMove;

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

// 将棋エンジンとのUSIプロトコルに基づく通信を処理するための共通関数
// 人間対エンジン、エンジン対人間、およびエンジン同士の対局で共通して使用される。
void Usi::executeEngineCommunication(QString& positionStr, QString& positionPonderStr, QPoint& outFrom,
                                     QPoint& outTo, int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                     int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // 将棋エンジンからのレスポンスに基づいて、適切なコマンドを送信し、必要に応じて処理を行う。
    processEngineResponse(positionStr, positionPonderStr, byoyomiMilliSec, btime, wtime, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

    // bestmove resignを受信した場合
    if (m_isResignMove) return;

    int fileFrom, rankFrom, fileTo, rankTo;

    // 将棋エンジンがbestmove文字列で返した最善手から移動元の筋と段、移動先の筋と段を取得する。
    parseMoveCoordinates(fileFrom, rankFrom, fileTo, rankTo);

    // 移動元と移動先の筋と段をQPoint型のoutFrom、outToに代入する。
    outFrom = QPoint(fileFrom, rankFrom);
    outTo = QPoint(fileTo, rankTo);
}

// 人間対将棋エンジンの対局で将棋エンジンとUSIプロトコル通信を行う。
void Usi::handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                           QPoint& outFrom, QPoint& outTo,
                                           int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                           QStringList& positionStrList, int addEachMoveMilliSec1,
                                           int addEachMoveMilliSec2, bool useByoyomi)
{
    qDebug() << "[USI][HvE] enter handleHumanVsEngineCommunication"
             << "byoyomiMs=" << byoyomiMilliSec
             << "btime=" << btime << "wtime=" << wtime;

    // 送信前の position をプレビュー
    const QString prePreview = (positionStr.size() > 200)
                                   ? (positionStr.left(200) + QStringLiteral(" ..."))
                                   : positionStr;
    qDebug() << "[USI][HvE] position(before ensureMoves/bestmove) =" << prePreview;

    // bestmove 生成（人間手）
    m_bestMove = convertHumanMoveToUsiFormat(outFrom, outTo, m_gameController->promote());

    ensureMovesKeyword(positionStr);   // "moves" を必ず含める
    positionStr += " " + m_bestMove;   // 人間手を末尾に追加
    positionStrList.append(positionStr);

    const QString postPreview = (positionStr.size() > 200)
                                    ? (positionStr.left(200) + QStringLiteral(" ..."))
                                    : positionStr;
    qDebug() << "[USI][HvE] position(after append bestmove) =" << postPreview;

    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo,
                               byoyomiMilliSec, btime, wtime,
                               addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
}

// 将棋エンジン対人間および将棋エンジン同士の対局で将棋エンジンとUSIプロトコル通信を行う。
void Usi::handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr, QString& positionPonderStr, QPoint& outFrom, QPoint& outTo,
                                                        int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                                        int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // 共通のエンジン通信処理を実行する。
    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo, byoyomiMilliSec, btime, wtime,
                               addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
}

// 将棋エンジンからのレスポンスに基づいて、適切なコマンドを送信し、必要に応じて処理を行う。
void Usi::executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec)
{
    // 現在の局面（盤内のみ）をコピーする。
    cloneCurrentBoardData();

    // positionコマンドとgoコマンドを送信し、bestmoveを受信するまで待機する。
    sendPositionAndGoCommands(byoyomiMilliSec, positionStr);
}

// 漢字の指し手に変換したpv文字列を設定する。
void Usi::setPvKanjiStr(const QString& newPvKanjiStr)
{
    m_pvKanjiStr = newPvKanjiStr;
}

// GUIの「探索手」欄を更新する。
void Usi::updateSearchedHand(const ShogiEngineInfoParser* info)
{
    m_model->setSearchedMove(info->searchedHand());
}

// GUIの「深さ」欄を更新する。
void Usi::updateDepth(const ShogiEngineInfoParser* info)
{
    // 選択的に読んだ手の深さseldepthの値が何も無い時
    if (info->seldepth().isEmpty()) {
        // GUIの「深さ」欄に基本深さの値をセットする。
        m_model->setSearchDepth(info->depth());
    } else {
        // GUIの「深さ」欄に基本深さと選択的に読んだ手の深さをセットする。
        m_model->setSearchDepth(info->depth() + "/" + info->seldepth());
    }
}

// GUIの「ノード数」欄を更新する。
void Usi::updateNodes(const ShogiEngineInfoParser* info)
{
    // ノード数の情報を取得してGUIを更新する。
    unsigned long long nodes = info->nodes().toULongLong();

    // QLocaleを使用して3桁ごとにカンマを挿入する。
    m_model->setNodeCount(m_locale.toString(nodes));
}

// GUIの「局面探索数」欄を更新する。
void Usi::updateNps(const ShogiEngineInfoParser* info)
{
    // 探索局面数の情報を取得してGUIを更新する。
    unsigned long long nps = info->nps().toULongLong();

    // QLocaleを使用して3桁ごとにカンマを挿入する。
    m_model->setNodesPerSecond(m_locale.toString(nps));
}

// GUIの「ハッシュ使用率」欄を更新する。
void Usi::updateHashfull(const ShogiEngineInfoParser* info)
{
    // ハッシュの使用率hashfullの値が何も無い時
    if (info->hashfull().isEmpty()) {
        // GUIの「ハッシュ使用率」に空文字をセットする。
        m_model->setHashUsage("");
    } else {
        // hashfullの値を10で割って%表示にする。
        unsigned long long hash = info->hashfull().toULongLong() / 10;

        // GUIの「ハッシュ使用率」に%表示の値をセットする。
        m_model->setHashUsage(QString::number(hash) + "%");
    }
}

// 評価値を計算する。
int Usi::calculateScoreInt(const ShogiEngineInfoParser* info) const
{
    // 評価値を0で初期化
    int scoreInt = 0;

    // 詰み手数scoremateが正の時
    if ((info->scoreMate().toLongLong() > 0) || (info->scoreMate() == "+")) {
        // 最大評価値2000に設定
        scoreInt = 2000;
    }
    // 詰み手数scoremateが負の時
    else if ((info->scoreMate().toLongLong() < 0) || (info->scoreMate() == "-")) {
        // マイナスの評価値-2000に設定
        scoreInt = -2000;
    }
    // 詰み手数scoremateが0の時
    else {
        // 評価値を0に設定
        scoreInt = 0;
    }

    // 設定した評価値を返す。
    return scoreInt;
}

// 詰み手数（scoreMate）と最終評価値（lastScore）の更新を行う。
void Usi::updateScoreMateAndLastScore(ShogiEngineInfoParser* info, int& scoreInt)
{
    // 詰み手数scoremateの値が何も無い時
    if (info->scoreMate().isEmpty()) {
        // 最終評価値の値を0にする。
        m_lastScoreCp = 0;
    }
    // 詰み手数scoremateに値がある時
    else {
        scoreInt = calculateScoreInt(info);

        QString scoreMate = info->scoreMate();

        if ((scoreMate == "+") || (scoreMate == "-")) {
            scoreMate = "詰";
        } else {
            scoreMate += "手詰";
        }

        info->setScore(scoreMate);

        // 評価値の文字列に詰み手数を代入
        m_scoreStr = info->scoreMate();

        // 評価値の値を保存する。
        m_lastScoreCp = scoreInt;
    }
}

// 棋譜解析モードにより、評価値の処理を行う。
void Usi::updateAnalysisModeAndScore(const ShogiEngineInfoParser* info, int& scoreInt)
{
    // 棋譜解析モードの時
    if (m_analysisMode) {
        // 手番がPlayer1の時
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            // 評価値の文字列を整数に変換
            scoreInt = info->scoreCp().toInt();

            // 評価値の文字列を取得
            m_scoreStr = info->scoreCp();
        }
        // 手番がPlayer2の時
        else if (m_gameController->currentPlayer() == ShogiGameController::Player2) {
            // 評価値の文字列を整数に変換し、符号を反転させる。
            scoreInt = -info->scoreCp().toInt();

            // 評価値の文字列を取得
            m_scoreStr = QString::number(scoreInt);
        }
    }
    // 棋譜解析モード以外の時
    else {
        // 評価値の文字列を取得
        m_scoreStr = info->scoreCp();

        // 評価値の文字列を整数に変換
        scoreInt = m_scoreStr.toInt();

        if (info->evaluationBound() == ShogiEngineInfoParser::EvaluationBound::LowerBound) {
            m_scoreStr += "++";
        } else if (info->evaluationBound() == ShogiEngineInfoParser::EvaluationBound::UpperBound) {
            m_scoreStr += "--";
        }
    }
}

// 入力された評価値（scoreint）を範囲内（-2000〜2000）に制限して、その値をm_lastScorecpに設定する。
void Usi::updateLastScore(const int scoreInt)
{
    // 評価値が2000より大きいとき
    if (scoreInt > 2000) {
        // GUIの評価値グラフの上限値2000をm_lastScorecpに設定
        m_lastScoreCp = 2000;
    }
    // 評価値が-2000より小さいとき
    else if (scoreInt < -2000) {
        // GUIの評価値グラフの下限値2000をm_lastScorecpに設定
        m_lastScoreCp = -2000;
    }
    // 評価値が-2000〜2000の範囲の時
    else {
        // そのままの評価値をm_lastScorecpに設定
        m_lastScoreCp = scoreInt;
    }
}

// 現在の評価値（scoreCp）が存在するかどうかに基づき、詰み手数（scoremate）と最終評価値（lastScore）を更新する。
// scorecpが存在する場合は、棋譜解析モードに基づいた評価値の処理を行い、その他の情報（時間、深さ、ノード数、評価値、読み筋）をセットする。
// さらに、評価値を範囲内（-2000〜2000）に制限した上で、最終評価値を更新する。
void Usi::updateEvaluationInfo(ShogiEngineInfoParser* info, int& scoreInt)
{
    // 現在の評価値scorecpの値が何も無い時
    if (info->scoreCp().isEmpty()) {
        // 詰み手数（scoremate）と最終評価値（lastScore）の更新を行う。
        updateScoreMateAndLastScore(info, scoreInt);
    }
    // 現在の評価値scorecpに値がある時
    else {
        // 棋譜解析モードにより、評価値の処理を行う。
        updateAnalysisModeAndScore(info, scoreInt);

        info->setScore(m_scoreStr);

        // 漢字表記の読み筋を取得する。
        setPvKanjiStr(info->pvKanjiStr());

        // 入力された評価値（scoreInt）を範囲内（-2000〜2000）に制限して、その値をm_lastScorecpに設定する。
        updateLastScore(scoreInt);
    }
}

// 1手前に指した移動先の筋を設定する。
void Usi::setPreviousFileTo(int newPreviousFileTo)
{
    m_previousFileTo = newPreviousFileTo;
}

// 1手前に指した移動先の段を設定する。
void Usi::setPreviousRankTo(int newPreviousRankTo)
{
    m_previousRankTo = newPreviousRankTo;
}

// 漢字表記の読み筋を取得する。
QString Usi::pvKanjiStr() const
{
    return m_pvKanjiStr;
}

// 一番最後に受信した指し手の評価値を返す。
int Usi::lastScoreCp() const
{
    return m_lastScoreCp;
}

// GUIがbestmove resignを受信したかどうかのフラグを返す。
bool Usi::isResignMove() const
{
    return m_isResignMove;
}

// 将棋エンジンからinfoを受信した時にinfo行を解析し、GUIの「思考」タブに表示する。
// 将棋エンジンから受信したinfo行の読み筋を日本語に変換し、GUIの「探索手」「深さ」「ノード数」
// 「局面探索数」「ハッシュ使用率」「思考タブ」欄を更新する。
void Usi::infoReceived(QString& line)
{
    // 停止中は何もしない（従来互換）
    if (m_shutdownState != ShutdownState::Running) return;

    // ──────────────────────────────────────────────────────────────
    // バッファリング＆合成反映（最小改修：QObjectの動的プロパティを利用、ヘッダ変更なし）
    //   - "_infoBuf"        : QVariantList に QString を詰める
    //   - "_infoScheduled"  : bool フラグ（flush 予約済みか）
    // 50ms ごとにまとめて UI 反映し、イベントループ占有を抑える
    // ──────────────────────────────────────────────────────────────

    // 1) 行をバッファに積む
    {
        QVariantList buf = this->property("_infoBuf").toList();
        buf.append(line); // コピーして保持
        this->setProperty("_infoBuf", buf);
    }

    // 2) まだ flush が予約されていなければ予約する（50ms後に実行）
    if (!this->property("_infoScheduled").toBool()) {
        this->setProperty("_infoScheduled", true);

        // この Usi インスタンスをコンテキストに、50ms 後まとめて処理
        QTimer::singleShot(50, this, [this]() {
            // 予約フラグを解除
            this->setProperty("_infoScheduled", false);

            // 状態が既に停止していたら捨てる
            if (m_shutdownState != ShutdownState::Running) {
                this->setProperty("_infoBuf", QVariantList());
                return;
            }

            // 3) バッファを取り出してクリア（swap 的に）
            QVariantList batch;
            {
                batch = this->property("_infoBuf").toList();
                this->setProperty("_infoBuf", QVariantList());
            }
            if (batch.isEmpty()) return;

            // 4) まとめて処理（元の per-line ロジックをそのままループ内で実行）
            //    ※ ここで「UI更新を1行ずつ」行っても、readyRead 直列処理よりは
            //       スライスされるためイベントループが回りやすくなります。
            for (const QVariant& v : batch) {
                if (m_shutdownState != ShutdownState::Running) break; // 途中停止安全弁
                const QString ln = v.toString();
                if (ln.isEmpty()) continue;

                // ───────────── 以降、従来 infoReceived(line) の中身 ─────────────
                int scoreInt = 0;

                // info行を解析するクラス
                ShogiEngineInfoParser* info = new ShogiEngineInfoParser;

                // 前回の指し手の筋・段
                info->setPreviousFileTo(m_previousFileTo);
                info->setPreviousRankTo(m_previousRankTo);

                // 読み筋を日本語に変換＋内部状態更新
                info->parseEngineOutputAndUpdateState(
                    const_cast<QString&>(ln), // 既存シグネチャ互換のため const_cast
                    m_gameController,
                    m_clonedBoardData,
                    m_isPonderEnabled
                    );

                // 各種GUI項目の更新
                updateSearchedHand(info);
                updateDepth(info);
                updateNodes(info);
                updateNps(info);
                updateHashfull(info);

                // 評価値/詰み手数/最終評価値の更新
                updateEvaluationInfo(info, scoreInt);

                // 何かしら更新がある場合は思考タブへ追記
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
                }

                delete info;
                // ───────────── ここまで従来ロジック ─────────────
            }
        });
    }
}

void Usi::sendGameOverLoseAndQuitCommands()
{
    if (!m_process || m_process->state() != QProcess::Running) return;

    if (!m_gameoverSent) {                               // ★ 2重送信ガード
        sendGameOverCommand(QStringLiteral("lose"));
        m_gameoverSent = true;
    }

    sendQuitCommand();                                   // ★ 中で m_quitSent を確認

    // 以降は以前のとおり（info string だけ許可／バッファ廃棄／EOF）
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

// 将棋エンジンから受信したデータを1行ごとにm_linesに貯え、GUIに受信データをログ出力する。
// 標準出力からの受信
void Usi::readFromEngine()
{
    // スロット内で例外を投げるとアプリが死にやすいので、警告に留めて戻る
    if (!m_process) {
        qWarning() << "[USI] readFromEngine called but m_process is null; ignoring.";
        return;
    }

    // 1回の呼び出しで処理する最大行数（イベントループ占有を避けるための上限）
    static constexpr int kMaxLinesPerChunk = 64;
    int processed = 0;

    while (m_process && m_process->canReadLine() && processed < kMaxLinesPerChunk) {
        const QByteArray data = m_process->readLine();
        QString line = QString::fromUtf8(data).trimmed();
        if (line.isEmpty()) { ++processed; continue; }

        // 起動直後のみエンジン名を確定（ログ接頭辞に使う）
        if (line.startsWith(QStringLiteral("id name "))) {
            const QString n = line.mid(8).trimmed();
            if (!n.isEmpty() && m_logEngineName.isEmpty())
                m_logEngineName = n;
        }

        // ---- 終局/終了後の遮断ロジック ---------------------------------------
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
                } else {
                    qWarning().noquote() << pfx << " usidebug< (no-log-model) " << line;
                }
                qDebug().nospace() << pfx << " usidebug< " << line;
                --m_postQuitInfoStringLinesLeft;
            } else {
                qDebug().nospace() << logPrefix() << " [drop-after-quit] " << line;
            }
            ++processed;
            continue;
        }
        if (m_shutdownState == ShutdownState::IgnoreAll) {
            qDebug().nospace() << logPrefix() << " [drop-ignore-all] " << line;
            ++processed;
            continue;
        }
        // ---------------------------------------------------------------------

        // === 通常処理 ===
        const QString pfx = logPrefix();
        m_lines.append(line);

        // ★ 受信ログのヌルガード（EvEでcommモデル未配線でも落とさない）
        if (m_model) {
            m_model->appendUsiCommLog(pfx + " < " + line);
        } else {
            qWarning().noquote() << pfx << " usidebug< (no-log-model) " << line;
        }
        qDebug().nospace() << pfx << " usidebug< " << line;

        // ───────── 詰将棋の最終結果は最優先で処理 ─────────
        if (line.startsWith(QStringLiteral("checkmate"))) {
            handleCheckmateLine(line);
            ++processed;
            continue;
        }

        // ───────── 既存のbestmove/infoなど ─────────
        if (line.startsWith(QStringLiteral("bestmove"))) {
            m_bestMoveSignalReceived = true;
            bestMoveReceived(line);
        } else if (line.startsWith(QStringLiteral("info"))) {
            m_infoSignalReceived = true;
            infoReceived(line);
        } else if (line.contains(QStringLiteral("readyok"))) {
            m_readyOkSignalReceived = true;
        } else if (line.contains(QStringLiteral("usiok"))) {
            m_usiOkSignalReceived = true;
        }

        ++processed;
    }

    // まだ読み残しがある場合は、イベントループに返してから続きを処理
    if (m_process && m_process->canReadLine()) {
        QTimer::singleShot(0, this, &Usi::readFromEngine);
    }
}

// 標準エラーからの受信（新規）
void Usi::readFromEngineStderr()
{
    if (!m_process) return;

    // 完全遮断ならまとめて読み捨て
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

            // --- NEW: 終了状態のフィルタ ---
            if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
                if (m_postQuitInfoStringLinesLeft > 0 && shouldLogAfterQuit(line)) {
                    const QString pfx = logPrefix();
                    m_model->appendUsiCommLog(pfx + " <stderr> " + line);
                    qDebug().nospace() << pfx << " usidebug<stderr> " << line;

                    if (--m_postQuitInfoStringLinesLeft <= 0) {
                        m_shutdownState = ShutdownState::IgnoreAll;
                    }
                }
                continue; // 許可しない行は捨てる
            }

            // --- 通常時のみ stderr をログ ---
            const QString pfx = logPrefix();
            m_model->appendUsiCommLog(pfx + " <stderr> " + line);
            qDebug().nospace() << pfx << " usidebug<stderr> " << line;
        }
    }
}

// 将棋エンジンを起動する。
void Usi::startEngine(const QString& engineFile)
{
    // エンジンファイル存在チェック
    if (engineFile.isEmpty() || !QFile::exists(engineFile)) {
        const QString errorMessage =
            tr("An error occurred in Usi::startEngine. The specified engine file does not exist: %1").arg(engineFile);
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    QStringList args;

    // 旧プロセスが生きていたら念のため終了（重複起動防止）
    if (m_process) {
        disconnect(m_process, nullptr, this, nullptr);
        if (m_process->state() != QProcess::NotRunning) {
            sendQuitCommand();
            m_process->waitForFinished(1000);
        }
        m_process->deleteLater();
        m_process = nullptr;
    }

    // 新規 QProcess 準備
    m_process = new QProcess(this);

    // 起動
    m_process->start(engineFile, args, QIODevice::ReadWrite);

    // 起動失敗 → 打ち切り
    if (!m_process->waitForStarted()) {
        cleanupEngineProcessAndThread();
        const QString errorMessage =
            tr("An error occurred in Usi::startEngine. Failed to start the engine: %1").arg(engineFile);
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    // コールバック接続
    connect(m_process, &QProcess::readyReadStandardOutput, this, &Usi::readFromEngine);
    connect(m_process, &QProcess::readyReadStandardError,  this, &Usi::readFromEngineStderr);
    connect(m_process, &QProcess::errorOccurred,           this, &Usi::onProcessError);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &Usi::onProcessFinished);
}

// 将棋エンジンにコマンドを送信し、GUIのUSIプロトコル通信ログに表示する。
void Usi::sendCommand(const QString& command) const
{
    if (!m_process || (m_process->state() != QProcess::Running && m_process->state() != QProcess::Starting)) {
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

// 将棋エンジンからデータを受信して保管した行リストをクリアする。
void Usi::clearResponseData()
{
    // 行リストのクリア
    m_lines.clear();

    // エラーチェック
    if (!m_lines.isEmpty()) {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // 行リストのクリアに失敗したことを表示する。
        QString errorMessage = tr("An error occurred in Usi::clearResponseData. Failed to clear the line list.");

        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }
}

void Usi::sendQuitCommand()
{
    if (!m_process) return;

    // ★ 現行オペをキャンセル（ctx破棄＋ID進め）
    cancelCurrentOperation();

    m_shutdownState = ShutdownState::IgnoreAllExceptInfoString;
    m_postQuitInfoStringLinesLeft = 1;

    sendCommand(QStringLiteral("quit"));

    (void)m_process->readAllStandardOutput();
    (void)m_process->readAllStandardError();
    m_process->closeWriteChannel();
}

// usinewgameコマンドを将棋エンジンに送信する。
void Usi::sendUsiNewGameCommand()
{
    // 将棋エンジンにコマンドを送信する。
    sendCommand("usinewgame");
}

// positionコマンドを将棋エンジンに送信する。
void Usi::sendPositionCommand(QString& positionStr)
{
    // 将棋エンジンにコマンドを送信する。
    sendCommand(positionStr);
}

// 設定ファイルから将棋エンジンオプションを読み込み、
// setoptionコマンドの文字列を生成する。
void Usi::generateSetOptionCommands(const QString& engineName)
{
    // 実行パスをGUIと同じディレクトリに設定
    QDir::setCurrent(QApplication::applicationDirPath());

    // 設定ファイルから設定を読み込むためのQSettingsオブジェクトを作成する。
    QSettings settings(SettingsFileName, QSettings::IniFormat);

    // エンジン名に関連する設定の数を取得する。
    int settingCount = settings.beginReadArray(engineName);

    // ponderモードが有効かどうかのフラグを無効にする。
    m_isPonderEnabled = false;

     // 各設定をループで処理する。
    for (int i = 0; i < settingCount; ++i) {
        // 現在のインデックスを設定する。
        settings.setArrayIndex(i);

        // 設定の名前、値、タイプを取得する。
        QString settingName = settings.value("name").toString();
        QString settingValue = settings.value("value").toString();
        QString settingType = settings.value("type").toString();

        // タイプがボタンで、値が"on"の場合
        if (settingType == "button" && settingValue == "on") {
            // 名前だけのsetoptionコマンドを生成する。
            m_setOptionCommandList.append("setoption name " + settingName);
        }
        // それ以外の場合
        else {
            // 名前と値を持つsetoptionコマンドを生成する。
            m_setOptionCommandList.append("setoption name " + settingName + " value " + settingValue);

            // オプションがUSI_Ponderの場合
            if (settingName == "USI_Ponder") {
                // ponderモードが有効かどうかを設定する。
                m_isPonderEnabled = (settingValue == "true") ? true : false;
            }
        }
    }

    // 配列の読み取りを終了する。
    settings.endArray();
}

// setoptionコマンドを将棋エンジンに送信する。
void Usi::sendSetOptionCommands()
{
    // setoptionコマンドの個数分、繰り返す。
    for (int i = 0; i < m_setOptionCommandList.size(); i++) {
        // 各setoptionコマンドを取得する。
        QString command = m_setOptionCommandList.at(i);

        // 将棋エンジンにコマンドを送信する。
        sendCommand(command);
    }
}

// info行を全て削除する。
void Usi::infoRecordClear()
{
    if (m_modelThinking) m_modelThinking->clearAllItems();
}

// go ponderコマンドを将棋エンジンに送信する。
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

// stopコマンドを将棋エンジンに送信する。
void Usi::sendStopCommand()
{
    // 将棋エンジンにコマンドを送信する。
    sendCommand("stop");

    // ★ これが無いと UsiThread が wait している QEventLoop が抜けられません
    emit stopOrPonderhitCommandSent();
}

// ponderhitコマンドを将棋エンジンに送信する。
void Usi::sendPonderHitCommand()
{
    // ここから“自分の手番の思考時間”
    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();

    qDebug().nospace() << logPrefix()
                       << " PONDERHIT session=" << m_ponderSession
                       << " predicted=" << (m_predictedOpponentMove.isEmpty() ? "-" : m_predictedOpponentMove)
                       << " prevTo=(" << m_previousFileTo << "," << m_previousRankTo << ")";

    sendCommand("ponderhit");

    // ★ これも wait 中の QEventLoop を必ず抜けさせる
    emit stopOrPonderhitCommandSent();

    // 以後は通常思考フェーズ
    m_phase = SearchPhase::Main;
}

// gameoverコマンドを将棋エンジンに送信する。
void Usi::sendGameOverCommand(const QString& result)
{
    // 将棋エンジンにコマンドを送信する。
    sendCommand("gameover " + result);

    // 将棋エンジンからデータを受信して保管した行リストをクリアする。
    clearResponseData();
}

// 将棋の段（1～9）をアルファベット表記（a～i）に変換する。
QChar Usi::rankToAlphabet(const int rank) const
{
    // 'a'はASCIIで97なので、rank-1を足すことで望む文字に変換する。
    return QChar('a' + rank - 1);
}

// 将棋のアルファベット表記（a～i）を段（1～9）に変換する。
int Usi::alphabetToRank(QChar c)
{
    // 'a'を引くことで、a〜iを1〜9に変換する。
    return int(c.toLatin1() - 'a' + 1);
}

// 白（後手）の駒を表すアルファベットから持ち駒の段に変換する。
int Usi::pieceToRankWhite(QChar c)
{
    // 駒とその持ち駒段のマッピング
    static const QMap<QChar, int> m_pieceRankWhite = {{'P', 9}, {'L', 8}, {'N', 7}, {'S', 6}, {'G', 5}, {'B', 4}, {'R', 3}};

    // マップに存在するか確認し、存在すればその値を返す。
    if (m_pieceRankWhite.contains(c)) return m_pieceRankWhite[c];

    // マップに存在しない場合は0を返す。
    return 0;
}

// 黒（先手）の駒を表すアルファベットから持ち駒の段に変換する。
int Usi::pieceToRankBlack(QChar c)
{
    // 駒とその持ち駒段のマッピング
    static const QMap<QChar, int> m_pieceRankBlack = {{'P', 1}, {'L', 2}, {'N', 3}, {'S', 4}, {'G', 5}, {'B', 6}, {'R', 7}};

    // マップに存在するか確認し、存在すればその値を返す。
    if (m_pieceRankBlack.contains(c)) return m_pieceRankBlack[c];

    // マップに存在しない場合は0を返す。
    return 0;
}

// usiコマンドを将棋エンジンに送信する。
void Usi::sendUsiCommand()
{
    sendCommand("usi");
}

// isreadyコマンドを将棋エンジンに送信する。
void Usi::sendIsReadyCommand()
{
    sendCommand("isready");
}

// 将棋エンジンからusiokを受信するまで待つ。
bool Usi::waitForUsiOK(const int timeoutMilliseconds)
{
    // タイマーの作成
    QElapsedTimer timer;

    // タイマーを開始する。
    timer.start();

    // usiokを受信したかどうかのフラグをfalseにする。
    m_usiOkSignalReceived = false;

    // usiokを受信するまで繰り返す。
    while (!m_usiOkSignalReceived) {
        // この関数を呼び出すことで、長い計算やタスクを行っている間でも、アプリケーションのGUIがフリーズすることなく、
        // ユーザーからの入力を継続的に受け付けることができる。
        // GUIのメニューが反応しなくなるのを防いでいる。
        qApp->processEvents();

        // タイムアウトした時
        if (timer.elapsed() > timeoutMilliseconds) {
            // usiokを受信しなかった。
            return false;
        }

        // CPUリソースの過度な使用を防ぐためにスリープ
        QThread::msleep(10);
    }

    // usiokを受信した。
    return true;
}

// 将棋エンジンからreadyokを受信するまで待つ。
bool Usi::waitForReadyOk(const int timeoutMilliseconds)
{
    // タイマーの作成
    QElapsedTimer timer;

    // タイマーを開始する。
    timer.start();

    // readyokを受信したかどうかのフラグをfalseにする。
    m_readyOkSignalReceived = false;

    // readyokを受信するまで繰り返す。
    while (!m_readyOkSignalReceived) {
        // この関数を呼び出すことで、長い計算やタスクを行っている間でも、アプリケーションのGUIがフリーズすることなく、
        // ユーザーからの入力を継続的に受け付けることができる。
        // GUIのメニューが反応しなくなるのを防いでいる。
        qApp->processEvents();

        // タイムアウトした時
        if (timer.elapsed() > timeoutMilliseconds) {
            // readyokを受信しなかった。
            return false;
        }

        // CPUリソースの過度な使用を防ぐためにスリープ
        QThread::msleep(10);
    }

    // readyokを受信した。
    return true;
}

// goコマンドを将棋エンジンに送信する。
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

    // 計測＆フェーズ
    m_lastGoToBestmoveMs = 0;
    m_goTimer.start();
    m_phase = SearchPhase::Main;

    qDebug().nospace() << logPrefix()
                       << " start at go  btime=" << btime
                       << " wtime=" << wtime
                       << " byoyomi=" << byoyomiMilliSec << "ms";

    sendCommand(command);
}

// 棋譜解析モードで go コマンドを将棋エンジンに送信する。
// （以降の後処理は既存フローに任せる）
void Usi::sendGoCommandByAnalysys(int byoyomiMilliSec)
{
    // ★ 解析オペ開始（ctx+ID）
    const quint64 opId = beginOperationContext();

    infoRecordClear();
    sendCommand("go infinite");

    if (byoyomiMilliSec <= 0) {
        keepWaitingForBestMove(); // 無制限：従来どおり。ただし bestmoveReceived 側でIDガード
        return;
    }

    // ★ QTimer を ctx にぶら下げて stop を投げる
    auto* timer = new QTimer(m_opCtx);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, m_opCtx, [this]() {
        sendCommand("stop");
    });
    timer->start(byoyomiMilliSec);

    // stop 後の余裕時間も待つ（IDミスマッチ/終了宣言で即離脱）
    static constexpr int kPostStopGraceMs = 4000;
    const int waitBudget = qMax(byoyomiMilliSec + kPostStopGraceMs, 2500);

    if (!waitForBestMove(waitBudget)) {
        // 最終保険：短く追い待ち（ctx配下で再度stop）
        auto* chase = new QTimer(m_opCtx);
        chase->setSingleShot(true);
        connect(chase, &QTimer::timeout, m_opCtx, [this](){ sendCommand("stop"); });
        chase->start(0); // すぐ投げる

        if (!waitForBestMove(2000)) {
            qWarning() << "[USI][Analysis] bestmove timeout (non-fatal). waited(ms)="
                       << (waitBudget + 2000);
            return;
        }
    }
}

// 将棋エンジンからbestmoveを受信するまで待つ。
bool Usi::waitForBestMove(const int timeoutMilliseconds)
{
    // GUIスレッドを止めない待機方式（msleepを使わない）
    // - 10msごとのネストイベントループで通常イベント(QTimer/readyReadなど)を回す
    // - ShogiClock等のタイマ遅延を起こしにくい

    if (m_bestMoveSignalReceived) return true;           // 既に届いていれば即完了
    if (timeoutMilliseconds <= 0) return false;          // 無効なタイムアウト

    m_bestMoveSignalReceived = false;

    QElapsedTimer timer;
    timer.start();

    static constexpr int kSliceMs = 10;                  // 1スライスの長さ

    while (!m_bestMoveSignalReceived) {
        // タイムアウト判定
        if (timer.elapsed() >= timeoutMilliseconds) {
            return false; // bestmoveを受信しなかった
        }
        // 停止中は待機を打ち切る（安全弁）
        if (m_shutdownState != ShutdownState::Running) {
            return false;
        }

        // 10msだけイベントを回す（readyRead / タイマ / 入力 など）
        QEventLoop spin;
        QTimer tick;
        tick.setSingleShot(true);
        tick.setInterval(kSliceMs);
        QObject::connect(&tick, &QTimer::timeout, &spin, &QEventLoop::quit);
        tick.start();

        // ここで通常イベントが処理され、readyReadStandardOutput → readFromEngine() → bestmove処理
        // が進む。bestmove受信中でも最大で kSliceMs 程度でループに戻る。
        spin.exec(QEventLoop::AllEvents);
    }

    // bestmoveを受信した
    return true;
}

// 将棋エンジンからbestmoveを受信するまで待つ。
bool Usi::keepWaitingForBestMove()
{
    // bestmoveを受信したかどうかのフラグをfalseにする。
    m_bestMoveSignalReceived = false;

    // bestmoveを受信するまで繰り返す。
    while (!m_bestMoveSignalReceived) {
        // この関数を呼び出すことで、長い計算やタスクを行っている間でも、アプリケーションのGUIがフリーズすることなく、
        // ユーザーからの入力を継続的に受け付けることができる。
        // GUIのメニューが反応しなくなるのを防いでいる。
        qApp->processEvents();

        // CPUリソースの過度な使用を防ぐためにスリープ
        QThread::msleep(10);
    }

    // bestmoveを受信した。
    return true;
}

// 将棋エンジンからbestmoveを受信した時に最善手を取得する。
void Usi::bestMoveReceived(const QString& line)
{
    // ★ この受信が属する（と見なす）IDを入口でキャプチャ
    const quint64 observedId = m_seq;

    if (m_timeoutDeclared || m_shutdownState != ShutdownState::Running) {
        qDebug().nospace() << logPrefix() << " [drop-bestmove] (inactive-state) " << line;
        return;
    }

    const QStringList tokens = line.split(whitespaceRe(), Qt::SkipEmptyParts);
    const int bestMoveIndex  = tokens.indexOf(QStringLiteral("bestmove"));

    if (bestMoveIndex == -1 || bestMoveIndex + 1 >= tokens.size()) {
        const QString msg = tr("An error occurred in Usi::bestMoveReceived. bestmove or its succeeding string not found.");
        qWarning().noquote() << logPrefix() << " " << msg << " line=" << line;
        emit errorOccurred(msg);
        // ★ ここで“打ち切り”を確定（以降の継続処理を無効化）
        cancelCurrentOperation();
        return;
    }

    // ★ オペが切り替わっていたら旧bestmoveは採用しない
    if (observedId != m_seq) {
        qDebug().nospace() << logPrefix() << " [drop-bestmove] (op-id-mismatch) " << line;
        return;
    }

    m_bestMove = tokens.at(bestMoveIndex + 1);

    qint64 elapsed = m_goTimer.isValid() ? m_goTimer.elapsed() : -1;
    m_lastGoToBestmoveMs     = (elapsed >= 0) ? elapsed : 0;
    m_bestMoveSignalReceived = true;

    if (m_bestMove.compare(QStringLiteral("resign"), Qt::CaseInsensitive) == 0) {
        if (m_resignNotified) {
            qDebug().nospace() << logPrefix() << " [dup-resign-ignored]";
            return;
        }
        m_resignNotified = true;
        qDebug().nospace() << logPrefix() << " [TRACE] resign-detected t+" << ShogiUtils::nowMs() << "ms";
        emit bestMoveResignReceived();
        return;
    }

    const int ponderIdx = tokens.indexOf(QStringLiteral("ponder"));
    if (ponderIdx != -1 && ponderIdx + 1 < tokens.size()) {
        const QString ponderStr = tokens.at(ponderIdx + 1);
        qDebug().nospace() << logPrefix() << " [ponder] " << ponderStr;
    }
}

void Usi::startUsiThread()
{
    // 既存スレッドがあれば確実に終了させてから破棄
    if (m_usiThread) {
        emit stopOrPonderhitCommandSent();
        m_usiThread->wait();
        delete m_usiThread;
        m_usiThread = nullptr;
    }

    // 新しいスレッドを生成
    m_usiThread = new UsiThread(this, this);

    // 実行
    m_usiThread->start();
}

// GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待つ。
void Usi::waitForStopOrPonderhitCommand()
{
    QEventLoop loop;

    // GUIがstopまたはponderhitコマンドを送信したら、イベントループを終了する。
    connect(this, &Usi::stopOrPonderhitCommandSent, &loop, &QEventLoop::quit);

    // イベントループの開始（stopまたはponderhitコマンドが送られるまでブロック）
    loop.exec();
}

// 現在の局面（盤内のみ）をコピーする。
void Usi::cloneCurrentBoardData()
{
    m_clonedBoardData = m_gameController->board()->boardData();
}

// 将棋エンジンから返された「bestmove A ponder B」の情報をもとに、将棋盤内の駒配置を更新する。
void Usi::applyMovesToBoardFromBestMoveAndPonder()
{
    // info行を解析するクラス
    ShogiEngineInfoParser info;

    info.parseAndApplyMoveToClonedBoard(m_bestMove, m_clonedBoardData);

    info.parseAndApplyMoveToClonedBoard(m_predictedOpponentMove, m_clonedBoardData);
}

void Usi::setLogIdentity(const QString& engineTag, const QString& sideTag, const QString& engineName)
{
    m_logEngineTag = engineTag;
    m_logSideTag   = sideTag;
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
    // "[E1]" → "[E1/P1]" に整形
    QString within = m_logEngineTag.isEmpty() ? "[E?]" : m_logEngineTag;
    if (!m_logSideTag.isEmpty())
        within.insert(within.size()-1, "/" + m_logSideTag);

    // エンジン名を括弧外に付ける
    QString head = within + (m_logEngineName.isEmpty() ? "" : " " + m_logEngineName);

    const QString ph = phaseTag();
    return ph.isEmpty() ? head : (head + " " + ph);
}

bool Usi::shouldLogAfterQuit(const QString& line) const
{
    // 「info string」から始まる行だけを許可（完全一致の先頭判定）
    return line.startsWith(QStringLiteral("info string"));
}

void Usi::setSquelchResignLogging(bool on)
{
    m_squelchResignLogs = on;
}

bool Usi::shouldAbortWait() const
{
    // 1) GUI側で旗落ち確定などにより「もう採用しない」と宣言済み
    if (m_timeoutDeclared) return true;

    // 2) quit 送信後の受信抑止モード（info string だけ許可など）
    if (m_shutdownState != ShutdownState::Running) return true;

    // 3) プロセスが既に死んでいる/書き込み終了後など
    if (!m_process || m_process->state() != QProcess::Running) return true;

    return false;
}

void Usi::sendRaw(const QString& command) const
{
    if (!m_process) {
        qWarning() << "Usi::sendRaw called but m_process is null.";
        return;
    }
    sendCommand(command);
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

            // --- 送信後の遮断ポリシー ---
            if (m_shutdownState == ShutdownState::IgnoreAllExceptInfoString) {
                // 「info string …」だけ許可（指定行数まで）
                if (line.startsWith(QStringLiteral("info string")) &&
                    m_postQuitInfoStringLinesLeft > 0) {

                    const QString pfx = logPrefix();
                    if (m_model) {
                        m_model->appendUsiCommLog(pfx + " < " + line);
                    }
                    --m_postQuitInfoStringLinesLeft;

                    // 取り切ったら完全遮断へ
                    if (m_postQuitInfoStringLinesLeft <= 0) {
                        m_shutdownState = ShutdownState::IgnoreAll;
                    }
                }
                continue; // それ以外は捨てる
            }

            if (m_shutdownState == ShutdownState::IgnoreAll) {
                // 念のため "info string" は救済してもよい（不要なら丸ごとcontinueでOK）
                if (line.startsWith(QStringLiteral("info string"))) {
                    const QString pfx = logPrefix();
                    if (m_model) m_model->appendUsiCommLog(pfx + " < " + line);
                }
                continue;
            }

            // --- 通常時：そのままログ ---
            const QString pfx = logPrefix();
            if (m_model) {
                if (isStderr) m_model->appendUsiCommLog(pfx + " <stderr> " + line);
                else          m_model->appendUsiCommLog(pfx + " < "       + line);
            }
        }
    };

    flushTail(outTail, /*stderr=*/false);
    flushTail(errTail, /*stderr=*/true);

    // 最終的には完全遮断に移行
    m_shutdownState = ShutdownState::IgnoreAll;
}

// 【新規】詰み探索用：position→go mate
void Usi::sendPositionAndGoMate(const QString& sfen, int timeMs, bool infinite)
{
    if (!m_process || m_process->state() != QProcess::Running) {
        const QString errorMessage = tr("USI engine is not running.");
        emit errorOccurred(errorMessage);
        cancelCurrentOperation();
        return;
    }

    // "position sfen <SFEN>"
    sendCommand(QStringLiteral("position sfen %1").arg(sfen.trimmed()));

    // "go mate <ms>" または "go mate infinite"
    if (infinite || timeMs <= 0) {
        sendCommand(QStringLiteral("go mate infinite"));
    } else {
        sendCommand(QStringLiteral("go mate %1").arg(timeMs)); // 単位: ms
    }

    m_modeTsume = true;
}

// 【新規】詰み探索の停止
void Usi::sendStopForMate()
{
    if (!m_process || m_process->state() != QProcess::Running) return;
    // 既存の停止APIがあるならそれを使ってもOK（sendStopCommand() 等）
    sendCommand(QStringLiteral("stop"));
}

// 詰み探索用の実行（position → go mate）
void Usi::executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec)
{
    // 現在の局面（盤内のみ）をコピー（描画や後続処理に合わせて従来仕様を踏襲）
    cloneCurrentBoardData();

    // position → go mate の送信
    sendPositionAndGoMateCommands(mateLimitMilliSec, positionStr);
}

// "position ..." を送ってから "go mate ..." を送る
void Usi::sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr)
{
    // positionコマンドを将棋エンジンに送信
    sendPositionCommand(positionStr);

    // go mate <ms|infinite> を送信（無制限なら infinite）
    if (mateLimitMilliSec <= 0) {
        sendCommand(QStringLiteral("go mate infinite"));
    } else {
        sendCommand(QStringLiteral("go mate %1").arg(mateLimitMilliSec));
    }

    // ※解析時のような「bestmove待ち」は行わない。
    //   詰み探索は最終結果として "checkmate ..." が返ってくるので、
    //   受信側（readFromEngine）で処理する。
}

// "checkmate ..." の行を解析してシグナルを発火
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

    // 以降は USI の手列
    const QStringList pv = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (pv.isEmpty()) {
        emit checkmateUnknown();
        return;
    }
    emit checkmateSolved(pv);
}

// 【新規】オペレーション用の一時コンテキストを開始（auto-disconnect/auto-delete用）
// 返り値：このオペに対応するID（リクエストID）
quint64 Usi::beginOperationContext()
{
    // 旧オペを破棄（ぶら下がる QTimer なども一括で消える）
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    m_opCtx = new QObject(this); // この ctx を connect の受け側に使う

    // リクエストIDを進めて「旧オペのシグナルは無視」状態にする
    return ++m_seq;
}

// 【新規】現在のオペレーションをキャンセル
void Usi::cancelCurrentOperation()
{
    if (m_opCtx) {
        m_opCtx->deleteLater();
        m_opCtx = nullptr;
    }
    // ID を進めることで、旧オペ経由の bestmove/info はすべて無視される
    ++m_seq;
}
