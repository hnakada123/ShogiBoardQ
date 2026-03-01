/// @file csaclient.cpp
/// @brief CSAプロトコルクライアントクラスの実装

#include "csaclient.h"
#include "logcategories.h"

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
    if (m_connectionState != ConnectionState::Connecting) {
        return;  // 既に接続済み or 切断済みなら無視
    }
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
    if (m_socket->write(msg.toUtf8()) == -1) {
        qCWarning(lcNetwork) << "Failed to write message:" << m_socket->errorString();
        emit errorOccurred(tr("メッセージの送信に失敗しました: %1").arg(m_socket->errorString()));
        return;
    }
    if (!m_socket->flush()) {
        qCWarning(lcNetwork) << "Failed to flush socket:" << m_socket->errorString();
    }

    emit rawMessageSent(message);
    qCDebug(lcNetwork).noquote() << "Sent:" << message;
}

void CsaClient::setConnectionState(ConnectionState state)
{
    if (m_connectionState != state) {
        m_connectionState = state;
        emit connectionStateChanged(state);
    }
}
