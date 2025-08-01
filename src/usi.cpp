﻿#include "qdebug.h"
#include "qtimer.h"
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

#include "usi.h"
#include "shogiengineinfoparser.h"
#include "shogigamecontroller.h"
#include "shogiboard.h"
#include "shogienginethinkingmodel.h"
#include "playmode.h"
#include "enginesettingsconstants.h"
#include "shogiutils.h"

using namespace EngineSettingsConstants;

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
}

// デストラクタ
Usi::~Usi()
{
    cleanupEngineProcessAndThread();
}

// 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
void Usi::cleanupEngineProcessAndThread()
{
    if (m_process) {
        // プロセスが起動している場合にのみquitコマンドを送信する。
        if (m_process->state() == QProcess::Running) {
            // 将棋エンジンにquitコマンドを送信する。
            sendQuitCommand();

            // プロセスが指定時間内に終了しなかった場合、強制終了する。
            if (!m_process->waitForFinished(3000)) {
                // プロセスが指定時間内に終了しなかった場合、強制終了する。
                m_process->terminate();
                if (!m_process->waitForFinished(1000)) {
                    // 強制終了できなかった場合、killする。
                    m_process->kill();

                    // kill後の終了を確実に待つ。
                    m_process->waitForFinished();
                }
            }
        }

        // 全ての接続を切断
        disconnect(m_process, nullptr, this, nullptr);
        delete m_process;
        m_process = nullptr;
    }

    // 別スレッドの終了処理を行う。
    if (m_usiThread) {
        m_usiThread->quit();

        // スレッドの応答がない場合
        if (!m_usiThread->wait(3000)) {
            // エラーメッセージを表示する。
            const QString errorMessage = "An error occurred in Usi::cleanupEngineProcessAndThread. USI thread did not finish properly.";

            ShogiUtils::logAndThrowError(errorMessage);
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

    // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
    cleanupEngineProcessAndThread();

    // エラーメッセージを表示する。
    ShogiUtils::logAndThrowError(errorMessage);
}

// 将棋エンジンプロセスを起動し、対局を開始するUSIコマンドを送受信する。
void Usi::initializeAndStartEngineCommunication(QString& engineFile, QString& enginename)
{
    // 将棋エンジンのファイルパスが空の場合
    if (engineFile.isEmpty()) {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::initializeAndStartEngineCommunication. Engine file path is empty.");

        ShogiUtils::logAndThrowError(errorMessage);
    }

    // カレントディレクトリをエンジンファイルのあるディレクトリに移動する。
    changeDirectoryToEnginePath(engineFile);

    // 将棋エンジンを起動し、対局開始に関するコマンドを送信する。
    startAndInitializeEngine(engineFile, enginename);
}

// カレントディレクトリをエンジンファイルのあるディレクトリに移動する。
void Usi::changeDirectoryToEnginePath(const QString& engineFile)
{
    // 将棋エンジンのファイルパスからファイル情報を取得する。
    QFileInfo fileInfo(engineFile);

    // カレントディレクトリをエンジンファイルのあるディレクトリに移動できなかった場合
    if (!QDir::setCurrent(fileInfo.path())) {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::changeDirectoryToEnginePath. Failed to move to %1. Cannot launch the shogi engine %2.").arg(fileInfo.path(), fileInfo.baseName());

        ShogiUtils::logAndThrowError(errorMessage);
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
    // usiコマンドを将棋エンジンに送信する。
    sendUsiCommand();

    // usiokを待機する。5000ms = 5秒をタイムアウトとして設定する。
    if (!waitForUsiOK(5000)) {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // usiokを受信できなかった場合、エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::sendUsiCommandAndWaitForUsiOk. Timeout waiting for usiok.");

        ShogiUtils::logAndThrowError(errorMessage);
    }
}

// isreadyコマンドを将棋エンジンに送り、readyokを待機する。
void Usi::sendIsReadyCommandAndWaitForReadyOk()
{
    // isreadyコマンドを将棋エンジンに送信する。
    sendIsReadyCommand();

    // readyokを待機する。5000ms = 5秒をタイムアウトとして設定する。
    if (!waitForReadyOk(5000)) {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // readyokを受信できなかった場合、エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::sendIsReadyCommandAndWaitForReadyOk. Timeout waiting for readyok.");

        ShogiUtils::logAndThrowError(errorMessage);
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
    // QPointの変数から列番号と段番号を取り出す。
    int fileFrom = outFrom.x();
    int rankFrom = outFrom.y();
    int fileTo = outTo.x();
    int rankTo = outTo.y();

    // bestmove文字列
    QString bestMove;

    // 指し手の筋が1～9の場合（すなわち、盤上の駒を動かす場合）
    if ((fileFrom >= 1) && (fileFrom <= 9)) {
        // 盤上の駒を動かす場合の指し手をUSI形式に変換する。
        bestMove = convertBoardMoveToUsi(fileFrom, rankFrom, fileTo, rankTo, promote);
    }
    // 持ち駒を打つ場合
    else if (fileFrom == 10 || fileFrom == 11) {
        // 持ち駒を打つ場合の指し手をUSI形式に変換する。
        bestMove = convertDropMoveToUsi(fileFrom, rankFrom, fileTo, rankTo);
    } else {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // 駒台の筋番号が不正な場合、エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::convertHumanMoveToUsiFormat. Invalid fileFrom.");

        ShogiUtils::logAndThrowError(errorMessage);
    }

    return bestMove;
}

// bestmove文字列（例: "7g7f", "P*5e"など）から移動元の座標（盤上の駒の場合）または持ち駒の種類（持ち駒を打つ場合）を解析する。
void Usi::parseMoveFrom(const QString& move, int& fileFrom, int& rankFrom)
{
    // 最初の文字が123456789のいずれかの場合
    if (QString("123456789").contains(move[0])) {
        // 移動元の筋と段を取得する。
        fileFrom = move[0].digitValue();
        rankFrom = alphabetToRank(move[1]);
    }
    // 最初の文字がPLNSGBRのいずれかで、次が'*'の場合
    else if (QString("PLNSGBR").contains(move[0]) && move[1] == '*') {
        // 現在の手番が先手の場合
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            // fileFromに先手の駒台を示す番号10を代入する。
            fileFrom = 10;

            // rankFromに先手の持ち駒の駒番号を代入する。
            rankFrom = pieceToRankBlack(move[0]);
        }
        // 現在の手番が後手の場合
        else if (m_gameController->currentPlayer() == ShogiGameController::Player2) {
            // fileFromに後手の駒台を示す番号11を代入する。
            fileFrom = 11;

            // rankFromに後手の持ち駒の駒番号を代入する。
            rankFrom = pieceToRankWhite(move[0]);
        }
    }
    // それ以外の場合、エラーとして例外を投げる。
    else {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // 移動元の文字列が不正な場合、エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::parseMoveFrom. Invalid move format in moveFrom.");

        ShogiUtils::logAndThrowError(errorMessage);
    }
}

// bestmove文字列（例: "7g7f", "P*5e"など）から移動先の座標を解析する。
void Usi::parseMoveTo(const QString& move, int& fileTo, int& rankTo)
{
    // 第3の文字が数字でない場合と第4の文字がアルファベットの文字でない場合、エラーになる。
    if (!move[2].isDigit() || !move[3].isLetter()) {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // 移動先の文字列が不正な場合、エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::parseMoveTo. Invalid move format in moveTo.");

        ShogiUtils::logAndThrowError(errorMessage);
    }

    // 移動先の筋と段を取得する。
    fileTo = move[2].digitValue();
    rankTo = alphabetToRank(move[3]);
}

// 将棋エンジンがbestmove文字列で返した最善手から移動元の筋と段、移動先の筋と段を取得する。
void Usi::parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo)
{
    QString move = m_bestMove;

    // bestmove文字列の長さが最低限の長さを満たしているかチェックする。
    if (move.length() < 4) {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // bestmove文字列が最低限の長さを満たしていない場合、エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::parseMoveCoordinates. Invalid bestmove format.");

        ShogiUtils::logAndThrowError(errorMessage);
    }

    // 移動元の座標を解析
    parseMoveFrom(move, fileFrom, rankFrom);

    // 移動先の座標を解析
    parseMoveTo(move, fileTo, rankTo);

    // 成り駒の場合、promoteフラグを設定
    m_gameController->setPromote(move.size() > 4 && move[4] == '+');
}

// bestmoveを指定した時間内に受信するまで待機する。
// 指定した時間内に受信できなかった場合、エラーメッセージを表示して例外をスローする。
void Usi::waitAndCheckForBestMove(const int time)
{
    if (!waitForBestMove(time)) {
        // bestmoveを指定した時間内に受信できなかった場合、エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::waitAndCheckForBestMove. Timeout waiting for bestmove.");

        ShogiUtils::logAndThrowError(errorMessage);
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
void Usi::sendCommandsAndProcess(int byoyomiMilliSec, QString& positionStr, const QString& btime, const QString& wtime,
                                 QString& positionPonderStr, int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // positionコマンドを将棋エンジンに送信する。
    sendPositionCommand(positionStr);

    // 現在の局面（盤内のみ）をコピーする。
    cloneCurrentBoardData();

    // goコマンドを将棋エンジンに送信する。
    sendGoCommand(byoyomiMilliSec, btime, wtime, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

    // 秒読み時間が0の場合
    if (byoyomiMilliSec == 0) {
        // 将棋エンジンからbestmoveを受信するまで待ち続ける。
        keepWaitingForBestMove();
    } else {
        // 残り時間になるまでbestmoveを待機する。
        waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
    }

    // bestmove resignを受信した場合
    if (m_isResignMove) return;

    // 指定された時間内にbestmoveを受信し、その後の手順に従い処理を行う。
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
void Usi::waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                               int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // 残り時間（ミリ秒）
    int remainingTime;

    // 秒読みを使用する場合
    if (useByoyomi) {
        // 現在の手番が対局者1の場合
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            // マージン1秒を追加して対局者1の残り時間を計算する。
            remainingTime = btime.toInt() + byoyomiMilliSec + 1000;
        }
        // 現在の手番が対局者2の場合
        else {
            // マージン1秒を追加して対局者2の残り時間を計算する。
            remainingTime = wtime.toInt() + byoyomiMilliSec + 1000;
        }
    }
    // 秒読みを使用しない場合
    else {
        // 現在の手番が対局者1の場合
        if (m_gameController->currentPlayer() == ShogiGameController::Player1) {
            remainingTime = btime.toInt() + addEachMoveMilliSec1 + 1000;
        }
        // 現在の手番が対局者2の場合
        else {
            remainingTime = wtime.toInt() + addEachMoveMilliSec2 + 1000;
        }
    }

    //begin
    qDebug() << "Usi::waitAndCheckForBestMoveRemainingTime";
    qDebug() << "m_gameController->currentPlayer(): " << m_gameController->currentPlayer();
    qDebug() << "ShogiGameController::Player1: " << ShogiGameController::Player1;
    qDebug() << "ShogiGameController::Player2: " << ShogiGameController::Player2;
    qDebug() << "byoyomiMilliSec: " << byoyomiMilliSec;
    qDebug() << "btime: " << btime;
    qDebug() << "wtime: " << wtime;
    qDebug() << "addEachMoveMilliSec1: " << addEachMoveMilliSec1;
    qDebug() << "addEachMoveMilliSec2: " << addEachMoveMilliSec2;
    qDebug() << "remainingTime: " << remainingTime;
    //end

    // bestmoveを指定した時間内に受信するまで待機する。
    waitAndCheckForBestMove(remainingTime);

}

// 将棋エンジンからのレスポンスに基づいて、適切なコマンドを送信し、必要に応じて処理を行う。
void Usi::processEngineResponse(QString& positionStr, QString& positionPonderStr, int byoyomiMilliSec, const QString& btime, const QString& wtime,
                                int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // 将棋エンジンが予想した対局相手の手の文字列が空の場合
    if (m_predictedOpponentMove.isEmpty() || !m_isPonderEnabled) {
        // positionコマンドを送信し、ponderモードをオフにして、goコマンドを送信し、
        // bestmoveを受信して予想手を考慮した処理を開始する。
        sendCommandsAndProcess(byoyomiMilliSec, positionStr, btime, wtime, positionPonderStr, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

        // bestmove resignを受信した場合
        if (m_isResignMove) return;
    }
    // bestmove行にponder以降の文字列があった場合
    else {
        // bestmoveで返された最善手と自分の予想した相手の指し手が同じ場合
        if (m_bestMove == m_predictedOpponentMove) {
            // 現在の局面（盤内のみ）をコピーする。
            cloneCurrentBoardData();

            // ponderhitコマンドを将棋エンジンに送信する。
            sendPonderHitCommand();

            // 秒読み時間が0の場合
            if (byoyomiMilliSec == 0) {
                // 将棋エンジンからbestmoveを受信するまで待ち続ける。
                keepWaitingForBestMove();
            } else {
                // 残り時間になるまでbestmoveを待機する。
                waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
            }

            // bestmove resignを受信した場合
            if (m_isResignMove) return;

            // 将棋エンジンからの最善手をposition文字列に追加し、予想手を考慮した処理を開始する。
            appendBestMoveAndStartPondering(positionStr, positionPonderStr);
        }
        // bestmoveで返された最善手と自分の予想した相手の指し手が異なる場合
        else {
            // stopコマンドを将棋エンジンに送信する。
            sendStopCommand();

            // 秒読み時間が0の場合
            if (byoyomiMilliSec == 0) {
                // 将棋エンジンからbestmoveを受信するまで待ち続ける。
                keepWaitingForBestMove();
            } else {
                // 残り時間になるまでbestmoveを待機する。
                waitAndCheckForBestMoveRemainingTime(byoyomiMilliSec, btime, wtime, addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);
            }

            // bestmove resignを受信した場合
            if (m_isResignMove) return;

            // positionコマンドを送信し、ponderモードをオフにして、goコマンドを送信し、
            // bestmoveを受信して予想手を考慮した処理を開始する。
            sendCommandsAndProcess(byoyomiMilliSec, positionStr, btime, wtime, positionPonderStr,
                                   addEachMoveMilliSec1, addEachMoveMilliSec2, useByoyomi);

            // bestmove resignを受信した場合
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
void Usi::handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr, QPoint& outFrom, QPoint& outTo,
                                          int byoyomiMilliSec, const QString& btime, const QString& wtime, QStringList& positionStrList,
                                          int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // bestmove文字列の作成
    m_bestMove = convertHumanMoveToUsiFormat(outFrom, outTo, m_gameController->promote());

    // position文字列にbestmoveの文字列を追加する。
    positionStr += " " + m_bestMove;

    // position文字列をリストに追加する。
    positionStrList.append(positionStr);

    // 共通のエンジン通信処理を実行する。
    executeEngineCommunication(positionStr, positionPonderStr, outFrom, outTo, byoyomiMilliSec, btime, wtime,
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
    // エンジンによる現在の評価値
    // 初期化する。
    int scoreInt = 0;

    // エンジンは、infoコマンドによって思考中の情報を返す。
    // info行を解析するクラス
    ShogiEngineInfoParser* info = new ShogiEngineInfoParser;

    // 前回の指し手のマスの筋、段をセットする。
    info->setPreviousFileTo(m_previousFileTo);
    info->setPreviousRankTo(m_previousRankTo);

    // 将棋エンジンから受信したinfo行の読み筋を日本語に変換する。info行であることは確約されている。
    // 例．
    // 「7g7h 2f2e 8e8f 2e2d 2c2d 8i7g 8f8g+」
    // 「△７八馬(77)▲２五歩(26)△８六歩(85)▲２四歩(25)△同歩(23)▲７七桂(89)△８七歩成(86)」
    info->parseEngineOutputAndUpdateState(line, m_gameController, m_clonedBoardData, m_isPonderEnabled);

    // GUIの「探索手」欄を更新する。
    updateSearchedHand(info);

    // GUIの「深さ」欄を更新する。
    updateDepth(info);

    // GUIの「ノード数」欄を更新する。
    updateNodes(info);

    // GUIの「局面探索数」欄を更新する。
    updateNps(info);

    // GUIの「ハッシュ使用率」欄を更新する。
    updateHashfull(info);

    // 現在の評価値（scorecp）が存在するかどうかに基づき、詰み手数（scoremate）と最終評価値（lastScore）を更新する。
    updateEvaluationInfo(info, scoreInt);

    // 「時間」「深さ」「ノード数」「評価値」「読み筋」のうち、何かしらの情報が更新された場合
    if (!info->time().isEmpty() || !info->depth().isEmpty() || !info->nodes().isEmpty() || !info->score().isEmpty() || !info->pvKanjiStr().isEmpty()) {
        // GUIの思考タブの表に「時間」「深さ」「ノード数」「評価値」「読み筋」をセットして、思考タブのモデルにinfo情報を追加する。
        m_modelThinking->prependItem(new ShogiInfoRecord(info->time(), info->depth(), info->nodes(), info->score(), info->pvKanjiStr()));
    }

    // info行を解析するクラスのインスタンスを削除する。
    delete info;
}

// エンジンにgameover loseコマンドとquitコマンドを送信し、手番を変更する。
void Usi::sendGameOverLoseAndQuitCommands()
{
    // GUIがbestmove resignを受信したかどうかのフラグをtrueにする。
    m_isResignMove = true;

    if(m_process->state() == QProcess::Running) {
        // 投了をクリックした場合、エンジンにgameover loseコマンドを送る。
        sendGameOverCommand("lose");

        // エンジンにquitコマンドを送る。
        sendQuitCommand();
    }
}

// エンジンにgameover winコマンドとquitコマンドを送信する。
void Usi::sendGameOverWinAndQuitCommands()
{
    if(m_process->state() == QProcess::Running) {
        // 投了をクリックした場合、エンジンにgameover winコマンドを送る。
        sendGameOverCommand("win");

        // エンジンにquitコマンドを送る。
        sendQuitCommand();
    }
}

// 将棋エンジンから受信したデータを1行ごとにm_linesに貯え、GUIに受信データをログ出力する。
void Usi::readFromEngine()
{
    if (!m_process) {
        const QString errorMessage = tr("An error occurred in Usi::readFromEngine. m_process is null. Cannot read data from the engine.");

        ShogiUtils::logAndThrowError(errorMessage);
    }

    // 将棋エンジンの出力から読み取ることができる完全な行がある間
    while (m_process && m_process->canReadLine()) {
        // 文字列の先頭と末尾から空白文字（スペース、タブ、改行など）を取り除く。
        QByteArray data = m_process->readLine();

        QString line = QString::fromUtf8(data).trimmed();

        // 行リストに追加する。
        m_lines.append(line);

        // 各行をログに出力する。
        m_model->appendUsiCommLog("< " + line);

        // bestmoveで始まる行の場合
        if (line.startsWith("bestmove")) {
            // bestmoveを受信したかどうかのフラグをtrueにする。
            m_bestMoveSignalReceived = true;

            // 将棋エンジンからbestmoveを受信した時に最善手を取得する。
            bestMoveReceived(line);
        }
        // infoで始まる行の場合
        else if (line.startsWith("info")) {
            // info行を受信したかどうかのフラグをtrueにする。
            m_infoSignalReceived = true;

            // 将棋エンジンからinfoを受信した時にinfo行を解析し、GUIの「思考」タブに表示する。
            infoReceived(line);
        }
        // readyokを含む行の場合
        else if (line.contains("readyok")) {
            // readyokを受信したかどうかのフラグをtrueにする。
            m_readyOkSignalReceived = true;
        }
        // usiokを含む行の場合
        else if (line.contains("usiok")) {
            // usiokを受信したかどうかのフラグをtrueにする。
            m_usiOkSignalReceived = true;
        }
    }
}

// 将棋エンジンを起動する。
void Usi::startEngine(const QString& engineFile)
{
    // エンジンファイルの存在を確認する。
    if (engineFile.isEmpty() || !QFile::exists(engineFile)) {
        // エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::startEngine. The specified engine file does not exist: %1").arg(engineFile);

        ShogiUtils::logAndThrowError(errorMessage);
    }

    // 将棋エンジンの起動引数を設定（必要に応じて）
    QStringList args;

    // 旧プロセスが存在している場合、適切に終了させる。
    if (m_process) {
        // 全ての接続を切断する。
        disconnect(m_process, nullptr, this, nullptr);

        if (m_process->state() != QProcess::NotRunning) {
            // プロセスが実行中の場合は、終了を要求し、最大3秒間終了を待つ。
            m_process->terminate();
            m_process->waitForFinished(3000);

            if (m_process->state() != QProcess::NotRunning) {
                // 終了しない場合は、強制終了する。
                m_process->kill();
            }
        }

        // プロセスオブジェクトを削除する。
        delete m_process;
    }

    // 新しい将棋エンジンプロセスを作成する。
    m_process = new QProcess;

    // m_processからの標準出力が読み取り可能になったとき、readFromEngine関数を呼び出す。
    connect(m_process, &QProcess::readyReadStandardOutput, this, &Usi::readFromEngine);

    // QProcessのエラー発生時にonProcessError関数を呼び出す。
    connect(m_process, &QProcess::errorOccurred, this, &Usi::onProcessError);

    // 将棋エンジンプロセスを起動する。
    m_process->start(engineFile, args, QIODevice::ReadWrite);

    // 将棋エンジンプロセスが正常に起動しなかった場合
    if (!m_process->waitForStarted()) {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // エラーメッセージを表示する。
        QString errorMessage = tr("An error occurred in Usi::startEngine. Failed to start the engine: %1").arg(engineFile);

        ShogiUtils::logAndThrowError(errorMessage);
    }
}

// 将棋エンジンにコマンドを送信し、GUIのUSIプロトコル通信ログに表示する。
void Usi::sendCommand(const QString& command) const
{
    m_process->write((command + "\n").toUtf8());

    // 送信したコマンドをGUIのUSIプロトコル通信ログに表示する。
    m_model->appendUsiCommLog("> " + command);

    qDebug() << "usidebug> " << command;
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

        ShogiUtils::logAndThrowError(errorMessage);
    }
}

// quitコマンドを将棋エンジンに送信する。
void Usi::sendQuitCommand()
{
    // 将棋エンジンにコマンドを送信する。
    sendCommand("quit");
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
    m_modelThinking->clearAllItems();
}

// go ponderコマンドを将棋エンジンに送信する。
void Usi::sendGoPonderCommand()
{
    // GUIの「思考」タブの内容をクリアする。
    infoRecordClear();

    // 将棋エンジンにコマンドを送信する。
    sendCommand("go ponder");

    // 将棋エンジンからデータを受信して保管した行リストをクリアする。
    clearResponseData();
}

// stopコマンドを将棋エンジンに送信する。
void Usi::sendStopCommand()
{
    // 将棋エンジンにコマンドを送信する。
    sendCommand("stop");
}

// ponderhitコマンドを将棋エンジンに送信する。
void Usi::sendPonderHitCommand()
{
    // 将棋エンジンにコマンドを送信する。
    sendCommand("ponderhit");
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
void Usi::sendGoCommand(int byoyomiMilliSec, const QString& btime, const QString& wtime,  int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi)
{
    // GUIの「思考」タブの内容をクリアする。
    infoRecordClear();

    // goコマンドの文字列を生成する。
    QString command;

    // 秒読みを使う場合
    if (useByoyomi) {
        command = "go btime " + btime + " wtime " + wtime + " byoyomi " + QString::number(byoyomiMilliSec);
    }
    // 秒読みを使わない場合
    else {
        command = "go btime " + btime + " wtime " + wtime + " binc " + QString::number(addEachMoveMilliSec1) + " winc " + QString::number(addEachMoveMilliSec2);
    }

    // 将棋エンジンにコマンドを送信する。
    sendCommand(command);
}

// 棋譜解析モードでgoコマンドを将棋エンジンに送信する。
void Usi::sendGoCommandByAnalysys(int byoyomiMilliSec)
{
    // GUIの「思考」タブの内容をクリアする。
    infoRecordClear();

    // goコマンドの文字列を生成する。
    QString command = "go infinite";

    // 将棋エンジンにコマンドを送信する。
    sendCommand(command);

    // 検討時間が無制限の場合
    if (byoyomiMilliSec == 0) {
        // 将棋エンジンからbestmoveを受信するまで待ち続ける。
        keepWaitingForBestMove();
    }
    else {
        // 検討時間が0より大きい場合
        // 指定されたミリ秒後に "stop" コマンドを送信する。
        QTimer::singleShot(byoyomiMilliSec, this, [this]() {
            sendCommand("stop");
        });

        // 将棋エンジンからbestmoveを受信するまで最大10秒待つ。
        // 10秒よりもっと少なくても良いが4秒以上はあった方が良いと思われる。
        if (!waitForBestMove(10000)) {
            // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
            cleanupEngineProcessAndThread();

            // bestmoveを指定した時間内に受信できなかった場合、エラーメッセージを表示する。
            QString errorMessage = tr("An error occurred in Usi::sendGoCommandByAnalysys. Failed to receive bestmove.");

            ShogiUtils::logAndThrowError(errorMessage);
        }
    }
}

// 将棋エンジンからbestmoveを受信するまで待つ。
bool Usi::waitForBestMove(const int timeoutMilliseconds)
{
    // タイマーの作成
    QElapsedTimer timer;

    // タイマーを開始する。
    timer.start();

    // bestmoveを受信したかどうかのフラグをfalseにする。
    m_bestMoveSignalReceived = false;

    // bestmoveを受信するまで繰り返す。
    while (!m_bestMoveSignalReceived) {
        // この関数を呼び出すことで、長い計算やタスクを行っている間でも、アプリケーションのGUIがフリーズすることなく、
        // ユーザーからの入力を継続的に受け付けることができる。
        // GUIのメニューが反応しなくなるのを防いでいる。
        qApp->processEvents();

        // タイムアウトした時
        if (timer.elapsed() > timeoutMilliseconds) {
            // bestmoveを受信しなかった。
            return false;
        }

        // CPUリソースの過度な使用を防ぐためにスリープ
        QThread::msleep(10);
    }

    // bestmoveを受信した。
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
    // 最善手の文字列をクリアする。
    m_bestMove.clear();

    // bestmove行を空白で分割する
    QStringList tokens = line.split(" ");

    // bestmoveの次の文字列を取得する
    int bestMoveIndex = tokens.indexOf("bestmove");

    // bestmoveが含まれている時
    if (bestMoveIndex != -1 && bestMoveIndex + 1 < tokens.size()) {
        // bestmoveの次の文字列を取得する。
        m_bestMove = tokens[bestMoveIndex + 1];

        // bestmoveの次の文字列がresignの場合
        if (m_bestMove == "resign") {
            // 投了の処理を行う。
            sendGameOverLoseAndQuitCommands();

            emit bestMoveResignReceived();

            return;
        }
    }
    // bestmoveが含まれていないか、その後の文字列が存在しない場合
    else {
        // 将棋エンジンプロセスを終了し、プロセスとスレッドを削除する。
        cleanupEngineProcessAndThread();

        // bestmoveまたはその後の文字列が存在しない場合のエラーを表示する。
        QString errorMessage = tr("An error occurred in Usi::bestMoveReceived. bestmove or its succeeding string not found.");

        ShogiUtils::logAndThrowError(errorMessage);
    }

    // ponderが含まれている時
    int ponderIndex = tokens.indexOf("ponder");

    // ponderが含まれている時
    if (ponderIndex != -1 && ponderIndex + 1 < tokens.size()) {
        // ponderの次の文字列を取得する。
        m_predictedOpponentMove = tokens[ponderIndex + 1];

        // info行を解析するクラス
        ShogiEngineInfoParser info;

        QString kanjiMoveStr = info.convertPredictedMoveToKanjiString(m_gameController, m_predictedOpponentMove, m_clonedBoardData);

        // GUIの「予想手」欄に対局相手の予想手を表示する。
        m_model->setPredictiveMove(kanjiMoveStr);
    }
    // ponderが含まれていないか、その後の文字列が存在しない場合
    else {
        // 対局相手の予想手の文字列をクリアする。
        m_predictedOpponentMove.clear();

        // GUIの「予想手」欄をクリアする。
        m_model->setPredictiveMove("");
    }

    // 行リストをクリアする
    clearResponseData();
}

// 新しいスレッドを生成し、GUIが将棋エンジンにstopまたはponderhitコマンドを送信するまで待つ。
// start関数が実行されることでrun関数が実行され、その中でwaitForStopOrPonderhitCommand関数が実行される。
void Usi::startUsiThread()
{
    // 既存のスレッドが存在し、実行中の場合は終了を待つ。
    if (m_usiThread) {
        m_usiThread->quit();
        m_usiThread->wait();
    }

    // 新しいスレッドを生成する。
    m_usiThread = new UsiThread(this, this);

    // UsiThreadオブジェクトの実行が終了した（runメソッドが終了した）ときに、m_usiThreadオブジェクトが適切に削除されるようにする。
    connect(m_usiThread, &UsiThread::finished, m_usiThread, &QObject::deleteLater);

    // 新しいスレッドを実行する。
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
