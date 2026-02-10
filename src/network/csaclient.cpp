/// @file csaclient.cpp
/// @brief CSAプロトコルクライアントクラスの実装

#include "csaclient.h"
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcNetwork, "shogi.network")

CsaClient::GameSummary::GameSummary()
{
    clear();
}

void CsaClient::GameSummary::clear()
{
    protocolVersion.clear();
    protocolMode = QStringLiteral("Server");
    format.clear();
    declaration.clear();
    gameId.clear();
    blackName.clear();
    whiteName.clear();
    myTurn.clear();
    toMove = QStringLiteral("+");
    rematchOnDraw = false;
    maxMoves = 0;

    timeUnit = QStringLiteral("1sec");
    totalTime = 0;
    byoyomi = 0;
    leastTimePerMove = 0;
    increment = 0;
    delay = 0;
    timeRoundup = false;

    positionLines.clear();
    moves.clear();

    hasIndividualTime = false;
    totalTimeBlack = 0;
    totalTimeWhite = 0;
    byoyomiBlack = 0;
    byoyomiWhite = 0;
}

int CsaClient::GameSummary::timeUnitMs() const
{
    if (timeUnit == QStringLiteral("1msec") || timeUnit == QStringLiteral("msec")) {
        return 1;
    } else if (timeUnit == QStringLiteral("1min") || timeUnit == QStringLiteral("min")) {
        return 60000;
    }
    // デフォルトは秒
    return 1000;
}

// ============================================================
// 初期化
// ============================================================

CsaClient::CsaClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QTcpSocket(this))
    , m_connectionTimer(new QTimer(this))
{
    connect(m_socket, &QTcpSocket::connected,
            this, &CsaClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &CsaClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &CsaClient::onSocketError);
    connect(m_socket, &QTcpSocket::readyRead,
            this, &CsaClient::onReadyRead);

    m_connectionTimer->setSingleShot(true);
    connect(m_connectionTimer, &QTimer::timeout,
            this, &CsaClient::onConnectionTimeout);
}

CsaClient::~CsaClient()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

void CsaClient::connectToServer(const QString& host, int port)
{
    if (m_connectionState != ConnectionState::Disconnected) {
        emit errorOccurred(tr("既に接続中です"));
        return;
    }

    qCInfo(lcNetwork) << "Connecting to" << host << ":" << port;

    setConnectionState(ConnectionState::Connecting);
    m_receiveBuffer.clear();

    m_connectionTimer->start(kConnectionTimeoutMs);
    m_socket->connectToHost(host, static_cast<quint16>(port));
}

void CsaClient::disconnectFromServer()
{
    m_connectionTimer->stop();

    if (m_connectionState == ConnectionState::LoggedIn ||
        m_connectionState == ConnectionState::WaitingForGame) {
        // ログイン済みなら切断前にログアウトを送る（サーバー側でセッションを正常終了させるため）
        logout();
    }

    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

bool CsaClient::isConnected() const
{
    return m_socket->state() == QAbstractSocket::ConnectedState;
}

void CsaClient::login(const QString& username, const QString& password)
{
    if (m_connectionState != ConnectionState::Connected) {
        emit errorOccurred(tr("サーバーに接続されていません"));
        return;
    }

    m_username = username;
    QString loginCmd = QStringLiteral("LOGIN %1 %2").arg(username, password);
    sendMessage(loginCmd);
}

void CsaClient::logout()
{
    if (m_connectionState != ConnectionState::LoggedIn &&
        m_connectionState != ConnectionState::WaitingForGame) {
        return;
    }

    sendMessage(QStringLiteral("LOGOUT"));
}

void CsaClient::agree(const QString& gameId)
{
    if (m_connectionState != ConnectionState::GameReady) {
        emit errorOccurred(tr("対局条件を受信していません"));
        return;
    }

    QString cmd = QStringLiteral("AGREE");
    if (!gameId.isEmpty()) {
        cmd += QStringLiteral(" ") + gameId;
    }
    sendMessage(cmd);
}

void CsaClient::reject(const QString& gameId)
{
    if (m_connectionState != ConnectionState::GameReady) {
        return;
    }

    QString cmd = QStringLiteral("REJECT");
    if (!gameId.isEmpty()) {
        cmd += QStringLiteral(" ") + gameId;
    }
    sendMessage(cmd);
}

void CsaClient::sendMove(const QString& move)
{
    qCDebug(lcNetwork) << "CsaClient::sendMove called with:" << move;
    qCDebug(lcNetwork) << "connectionState=" << static_cast<int>(m_connectionState)
                       << "isMyTurn=" << m_isMyTurn;

    if (m_connectionState != ConnectionState::InGame) {
        qCDebug(lcNetwork) << "Not in game, rejecting move";
        emit errorOccurred(tr("対局中ではありません"));
        return;
    }

    if (!m_isMyTurn) {
        qCDebug(lcNetwork) << "Not my turn, rejecting move";
        emit errorOccurred(tr("自分の手番ではありません"));
        return;
    }

    qCDebug(lcNetwork) << "Sending move to server:" << move;
    sendMessage(move);
}

void CsaClient::sendRawCommand(const QString& command)
{
    sendMessage(command);
}

void CsaClient::resign()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }

    sendMessage(QStringLiteral("%TORYO"));
}

void CsaClient::declareWin()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }

    sendMessage(QStringLiteral("%KACHI"));
}

void CsaClient::requestChudan()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }

    sendMessage(QStringLiteral("%CHUDAN"));
}

void CsaClient::onSocketConnected()
{
    m_connectionTimer->stop();
    qCInfo(lcNetwork) << "Connected to server";
    setConnectionState(ConnectionState::Connected);
}

void CsaClient::onSocketDisconnected()
{
    m_connectionTimer->stop();
    qCInfo(lcNetwork) << "Disconnected from server";
    setConnectionState(ConnectionState::Disconnected);
}

void CsaClient::onSocketError(QAbstractSocket::SocketError error)
{
    m_connectionTimer->stop();

    QString errorMessage = m_socket->errorString();

    // 対局終了後のリモートホスト切断は正常動作として扱う
    if (m_connectionState == ConnectionState::GameOver &&
        error == QAbstractSocket::RemoteHostClosedError) {
        qCInfo(lcNetwork) << "Connection closed by server after game end (normal)";
        setConnectionState(ConnectionState::Disconnected);
        return;
    }

    qCWarning(lcNetwork) << "Socket error:" << errorMessage;

    emit errorOccurred(errorMessage);
    setConnectionState(ConnectionState::Disconnected);
}

void CsaClient::onReadyRead()
{
    // TCP受信データはパケット境界と行境界が一致しないため、バッファに蓄積して行単位に分割する
    m_receiveBuffer += QString::fromUtf8(m_socket->readAll());

    // 行単位で処理
    qsizetype newlineIndex;
    while ((newlineIndex = m_receiveBuffer.indexOf(QLatin1Char('\n'))) != -1) {
        QString line = m_receiveBuffer.left(static_cast<int>(newlineIndex));
        m_receiveBuffer = m_receiveBuffer.mid(static_cast<int>(newlineIndex) + 1);

        // CRを除去
        if (line.endsWith(QLatin1Char('\r'))) {
            line.chop(1);
        }

        if (!line.isEmpty()) {
            emit rawMessageReceived(line);
            processLine(line);
        }
    }
}

void CsaClient::onConnectionTimeout()
{
    qCWarning(lcNetwork) << "Connection timeout";
    m_socket->abort();
    emit errorOccurred(tr("接続がタイムアウトしました"));
    setConnectionState(ConnectionState::Disconnected);
}

void CsaClient::sendMessage(const QString& message)
{
    if (!isConnected()) {
        return;
    }

    QString msg = message + QStringLiteral("\n");
    m_socket->write(msg.toUtf8());
    m_socket->flush();

    emit rawMessageSent(message);
    qCDebug(lcNetwork).noquote() << "Sent:" << message;
}

void CsaClient::processLine(const QString& line)
{
    qCDebug(lcNetwork).noquote() << "Recv:" << line;

    // Game_Summary解析中
    if (m_inGameSummary) {
        processGameSummary(line);
        return;
    }

    // 状態に応じた処理
    switch (m_connectionState) {
    case ConnectionState::Connected:
        processLoginResponse(line);
        break;
    case ConnectionState::LoggedIn:
    case ConnectionState::WaitingForGame:
        // Game_Summary開始を待つ
        if (line == QStringLiteral("BEGIN Game_Summary")) {
            m_inGameSummary = true;
            m_gameSummary.clear();
        } else if (line.startsWith(QStringLiteral("LOGOUT:"))) {
            emit logoutCompleted();
        }
        break;
    case ConnectionState::GameReady:
        if (line.startsWith(QStringLiteral("START:"))) {
            // 対局開始
            QString gameId = line.mid(6);
            setConnectionState(ConnectionState::InGame);
            m_isMyTurn = (m_gameSummary.myTurn == m_gameSummary.toMove);
            m_moveCount = 0;
            m_endMoveConsumedTimeMs = 0;  // 終局時消費時間をリセット
            emit gameStarted(gameId);
        } else if (line.startsWith(QStringLiteral("REJECT:"))) {
            // 対局拒否
            // REJECT:<GameID> by <rejector>
            qsizetype byIndex = line.indexOf(QStringLiteral(" by "));
            QString gameId = line.mid(7, static_cast<int>(byIndex) - 7);
            QString rejector = line.mid(static_cast<int>(byIndex) + 4);
            setConnectionState(ConnectionState::WaitingForGame);
            emit gameRejected(gameId, rejector);
        }
        break;
    case ConnectionState::InGame:
        processGameMessage(line);
        break;
    default:
        break;
    }
}

void CsaClient::processLoginResponse(const QString& line)
{
    if (line.startsWith(QStringLiteral("LOGIN:"))) {
        if (line.contains(QStringLiteral(" OK"))) {
            // ログイン成功
            qCInfo(lcNetwork) << "Login successful";
            setConnectionState(ConnectionState::LoggedIn);
            emit loginSucceeded();
        } else if (line.contains(QStringLiteral("incorrect"))) {
            // ログイン失敗
            qCWarning(lcNetwork) << "Login failed";
            emit loginFailed(tr("ユーザー名またはパスワードが正しくありません"));
        }
    }
}

void CsaClient::processGameSummary(const QString& line)
{
    // セクション終了チェック
    if (line == QStringLiteral("END Game_Summary")) {
        m_inGameSummary = false;
        m_inTimeSection = false;
        m_inPositionSection = false;
        setConnectionState(ConnectionState::GameReady);
        emit gameSummaryReceived(m_gameSummary);
        return;
    }

    if (line == QStringLiteral("END Time") ||
        line == QStringLiteral("END Time+") ||
        line == QStringLiteral("END Time-")) {
        m_inTimeSection = false;
        return;
    }

    if (line == QStringLiteral("END Position")) {
        m_inPositionSection = false;
        return;
    }

    // セクション開始チェック
    if (line == QStringLiteral("BEGIN Time")) {
        m_inTimeSection = true;
        m_currentTimeSection = QStringLiteral("Time");
        return;
    }
    if (line == QStringLiteral("BEGIN Time+")) {
        m_inTimeSection = true;
        m_currentTimeSection = QStringLiteral("Time+");
        m_gameSummary.hasIndividualTime = true;
        return;
    }
    if (line == QStringLiteral("BEGIN Time-")) {
        m_inTimeSection = true;
        m_currentTimeSection = QStringLiteral("Time-");
        m_gameSummary.hasIndividualTime = true;
        return;
    }
    if (line == QStringLiteral("BEGIN Position")) {
        m_inPositionSection = true;
        return;
    }

    // 時間セクション内の処理
    if (m_inTimeSection) {
        qsizetype colonPos = line.indexOf(QLatin1Char(':'));
        if (colonPos > 0) {
            QString key = line.left(static_cast<int>(colonPos));
            QString value = line.mid(static_cast<int>(colonPos) + 1);

            if (key == QStringLiteral("Time_Unit")) {
                m_gameSummary.timeUnit = value;
            } else if (key == QStringLiteral("Total_Time")) {
                int time = value.toInt();
                if (m_currentTimeSection == QStringLiteral("Time+")) {
                    m_gameSummary.totalTimeBlack = time;
                } else if (m_currentTimeSection == QStringLiteral("Time-")) {
                    m_gameSummary.totalTimeWhite = time;
                } else {
                    m_gameSummary.totalTime = time;
                }
            } else if (key == QStringLiteral("Byoyomi")) {
                int time = value.toInt();
                if (m_currentTimeSection == QStringLiteral("Time+")) {
                    m_gameSummary.byoyomiBlack = time;
                } else if (m_currentTimeSection == QStringLiteral("Time-")) {
                    m_gameSummary.byoyomiWhite = time;
                } else {
                    m_gameSummary.byoyomi = time;
                }
            } else if (key == QStringLiteral("Least_Time_Per_Move")) {
                m_gameSummary.leastTimePerMove = value.toInt();
            } else if (key == QStringLiteral("Increment")) {
                m_gameSummary.increment = value.toInt();
            } else if (key == QStringLiteral("Delay")) {
                m_gameSummary.delay = value.toInt();
            } else if (key == QStringLiteral("Time_Roundup")) {
                m_gameSummary.timeRoundup = (value == QStringLiteral("YES"));
            }
        }
        return;
    }

    // 局面セクション内の処理
    if (m_inPositionSection) {
        // P1-P9, P+, P-, +, - で始まる行は局面情報
        if (line.startsWith(QLatin1Char('P')) ||
            line == QStringLiteral("+") ||
            line == QStringLiteral("-")) {
            m_gameSummary.positionLines.append(line);
        }
        // 指し手行（+7776FU,T12 形式）
        else if ((line.startsWith(QLatin1Char('+')) || line.startsWith(QLatin1Char('-'))) &&
                 line.length() > 1) {
            // 消費時間を除去して指し手のみ保存
            qsizetype commaPos = line.indexOf(QLatin1Char(','));
            QString move = (commaPos > 0) ? line.left(static_cast<int>(commaPos)) : line;
            m_gameSummary.moves.append(move);
        }
        return;
    }

    // 一般情報の処理
    qsizetype colonPos = line.indexOf(QLatin1Char(':'));
    if (colonPos > 0) {
        QString key = line.left(static_cast<int>(colonPos));
        QString value = line.mid(static_cast<int>(colonPos) + 1);

        if (key == QStringLiteral("Protocol_Version")) {
            m_gameSummary.protocolVersion = value;
        } else if (key == QStringLiteral("Protocol_Mode")) {
            m_gameSummary.protocolMode = value;
        } else if (key == QStringLiteral("Format")) {
            m_gameSummary.format = value;
        } else if (key == QStringLiteral("Declaration")) {
            m_gameSummary.declaration = value;
        } else if (key == QStringLiteral("Game_ID")) {
            m_gameSummary.gameId = value;
        } else if (key == QStringLiteral("Name+")) {
            m_gameSummary.blackName = value;
        } else if (key == QStringLiteral("Name-")) {
            m_gameSummary.whiteName = value;
        } else if (key == QStringLiteral("Your_Turn")) {
            m_gameSummary.myTurn = value;
        } else if (key == QStringLiteral("To_Move")) {
            m_gameSummary.toMove = value;
        } else if (key == QStringLiteral("Rematch_On_Draw")) {
            m_gameSummary.rematchOnDraw = (value == QStringLiteral("YES"));
        } else if (key == QStringLiteral("Max_Moves")) {
            m_gameSummary.maxMoves = value.toInt();
        }
    }
}

void CsaClient::processGameMessage(const QString& line)
{
    qCDebug(lcNetwork) << "processGameMessage:" << line;

    // #で始まる行は結果行
    if (line.startsWith(QLatin1Char('#'))) {
        qCDebug(lcNetwork) << "Result line detected";
        processResultLine(line);
        return;
    }

    // %で始まる行は特殊コマンド（投了、勝利宣言など）の確認
    if (line.startsWith(QLatin1Char('%'))) {
        qCDebug(lcNetwork) << "Special command line detected";
        // %TORYO,T4 形式
        qsizetype commaPos = line.indexOf(QLatin1Char(','));
        QString cmd = (commaPos > 0) ? line.left(static_cast<int>(commaPos)) : line;
        int consumedTime = 0;
        if (commaPos > 0 && QStringView(line).mid(commaPos + 1).startsWith(QLatin1Char('T'))) {
            consumedTime = parseConsumedTime(line.mid(static_cast<int>(commaPos) + 2));
        }

        // 終局手の消費時間を保存（ミリ秒に変換）
        m_endMoveConsumedTimeMs = consumedTime * m_gameSummary.timeUnitMs();
        qCDebug(lcNetwork) << "End move consumed time:" << m_endMoveConsumedTimeMs << "ms";

        Q_UNUSED(cmd)
        return;
    }

    // 指し手行の処理
    if ((line.startsWith(QLatin1Char('+')) || line.startsWith(QLatin1Char('-'))) &&
        line.length() > 1) {
        qCDebug(lcNetwork) << "Move line detected, calling processMoveLine";
        processMoveLine(line);
    }
}

void CsaClient::processResultLine(const QString& line)
{
    qCDebug(lcNetwork) << "processResultLine:" << line;

    // 結果行は2行連続で来る
    // 1行目: 事象（#RESIGN, #SENNICHITE, #TIME_UP など）
    // 2行目: 結果（#WIN, #LOSE, #DRAW など）

    if (m_pendingFirstResultLine.isEmpty()) {
        // 最初の結果行を保存
        qCDebug(lcNetwork) << "First result line, saving:" << line;
        m_pendingFirstResultLine = line;
        return;
    }

    // 2行目を受信
    QString firstLine = m_pendingFirstResultLine;
    QString secondLine = line;
    m_pendingFirstResultLine.clear();

    qCDebug(lcNetwork) << "Processing result: cause=" << firstLine << "result=" << secondLine;

    GameResult result = GameResult::Unknown;
    GameEndCause cause = GameEndCause::Unknown;

    // 結果判定
    if (secondLine == QStringLiteral("#WIN")) {
        result = GameResult::Win;
    } else if (secondLine == QStringLiteral("#LOSE")) {
        result = GameResult::Lose;
    } else if (secondLine == QStringLiteral("#DRAW")) {
        result = GameResult::Draw;
    } else if (secondLine == QStringLiteral("#CENSORED")) {
        result = GameResult::Censored;
    }

    // 原因判定
    if (firstLine == QStringLiteral("#RESIGN")) {
        cause = GameEndCause::Resign;
    } else if (firstLine == QStringLiteral("#TIME_UP")) {
        cause = GameEndCause::TimeUp;
    } else if (firstLine == QStringLiteral("#ILLEGAL_MOVE")) {
        cause = GameEndCause::IllegalMove;
    } else if (firstLine == QStringLiteral("#SENNICHITE")) {
        cause = GameEndCause::Sennichite;
    } else if (firstLine == QStringLiteral("#OUTE_SENNICHITE")) {
        cause = GameEndCause::OuteSennichite;
    } else if (firstLine == QStringLiteral("#JISHOGI")) {
        cause = GameEndCause::Jishogi;
    } else if (firstLine == QStringLiteral("#MAX_MOVES")) {
        cause = GameEndCause::MaxMoves;
    } else if (firstLine == QStringLiteral("#CHUDAN")) {
        cause = GameEndCause::Chudan;
        emit gameInterrupted();
    } else if (firstLine == QStringLiteral("#ILLEGAL_ACTION")) {
        cause = GameEndCause::IllegalAction;
    }

    qCDebug(lcNetwork) << "Game ended with result=" << static_cast<int>(result)
                       << "cause=" << static_cast<int>(cause)
                       << "consumedTimeMs=" << m_endMoveConsumedTimeMs;

    setConnectionState(ConnectionState::GameOver);
    emit gameEnded(result, cause, m_endMoveConsumedTimeMs);

    // 対局終了後はGameOver状態を維持（サーバーからの切断を正常終了として扱うため）
    // 再接続や次の対局開始時に適切な状態に遷移する
}

void CsaClient::processMoveLine(const QString& line)
{
    // 形式: +7776FU,T12
    qsizetype commaPos = line.indexOf(QLatin1Char(','));
    QString move = (commaPos > 0) ? line.left(static_cast<int>(commaPos)) : line;
    int consumedTime = 0;

    if (commaPos > 0 && QStringView(line).mid(commaPos + 1).startsWith(QLatin1Char('T'))) {
        consumedTime = parseConsumedTime(line.mid(static_cast<int>(commaPos) + 2));
    }

    // 消費時間をミリ秒に変換
    int consumedTimeMs = consumedTime * m_gameSummary.timeUnitMs();

    m_moveCount++;

    // 手番を判定（指し手の先頭文字で判定）
    bool isBlackMove = move.startsWith(QLatin1Char('+'));
    bool wasMyTurn = m_isMyTurn;

    // 手番を更新（相手が指したら自分の手番に）
    if (isBlackMove) {
        m_isMyTurn = (m_gameSummary.myTurn == QStringLiteral("-"));
    } else {
        m_isMyTurn = (m_gameSummary.myTurn == QStringLiteral("+"));
    }

    if (wasMyTurn) {
        // 自分の指し手の確認
        emit moveConfirmed(move, consumedTimeMs);
    } else {
        // 相手の指し手
        emit moveReceived(move, consumedTimeMs);
    }
}

void CsaClient::setConnectionState(ConnectionState state)
{
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}

int CsaClient::parseConsumedTime(const QString& timeStr) const
{
    return timeStr.toInt();
}
