/// @file csaclient.cpp
/// @brief CSAプロトコルクライアントクラスの実装

#include "csaclient.h"
#include "logcategories.h"
#include <QRegularExpression>

namespace {
constexpr int kMsPerSecond = 1000;
constexpr int kMsPerMinute = 60000;
constexpr int kMinPort = 1;
constexpr int kMaxPort = 65535;
} // namespace

int CsaClient::TimeControl::unitMs() const
{
    if (timeUnit == QStringLiteral("1msec") || timeUnit == QStringLiteral("msec")) {
        return 1;
    } else if (timeUnit == QStringLiteral("1min") || timeUnit == QStringLiteral("min")) {
        return kMsPerMinute;
    }
    // デフォルトは秒
    return kMsPerSecond;
}

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
    myTurn.reset();
    toMove = Turn::Black;
    rematchOnDraw = false;
    maxMoves = 0;

    timeBlack = TimeControl();
    timeWhite = TimeControl();
    hasIndividualTime = false;

    positionLines.clear();
    moves.clear();
    moveConsumedTimes.clear();
}

::TimeControl CsaClient::GameSummary::timeControl() const
{
    ::TimeControl tc;
    if (isTimeUnlimited()) return tc;
    for (const bool black : {true, false}) {
        const TimeControl& t = timeFor(black);
        TimeControlSide& side = black ? tc.black : tc.white;
        side.mainMs = t.totalTimeMs();
        side.byoyomiMs = t.byoyomiMs();
        side.incrementMs = t.incrementMs();
        side.delayMs = t.delayMs();
    }
    return tc;
}

int CsaClient::GameSummary::positionMoveConsumedMs(int index) const
{
    if (index < 0 || index >= moves.size()) return 0;
    const bool black = moves.at(index).startsWith(QLatin1Char('+'));
    return moveConsumedTimes.value(index, 0) * timeUnitMs(black);
}

bool CsaClient::isWaitingState(ConnectionState state)
{
    return state == ConnectionState::LoggedIn
        || state == ConnectionState::WaitingForGame
        || state == ConnectionState::GameOver;
}

// ============================================================
// 初期化
// ============================================================

CsaClient::CsaClient(QObject* parent)
    : QObject(parent)
    , m_socket(new QSslSocket(this))
    , m_connectionTimer(new QTimer(this))
    , m_logoutTimer(new QTimer(this))
    , m_keepAliveTimer(new QTimer(this))
{
    m_socket->setReadBufferSize(kMaxLineBytes + 1);
    m_socket->setPeerVerifyMode(QSslSocket::VerifyPeer);
    m_socket->setProtocol(QSsl::TlsV1_2OrLater);
    connect(m_socket, &QSslSocket::encrypted,
            this, &CsaClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::connected,
            this, &CsaClient::onSocketConnected);
    connect(m_socket, &QTcpSocket::disconnected,
            this, &CsaClient::onSocketDisconnected);
    connect(m_socket, &QTcpSocket::errorOccurred,
            this, &CsaClient::onSocketError);
    // USI待機の入れ子イベントループでも受信する。readyRead自体は再帰的に発火しない。
    connect(m_socket, &QTcpSocket::readyRead,
            this, &CsaClient::onReadyRead, Qt::QueuedConnection);

    m_connectionTimer->setSingleShot(true);
    connect(m_connectionTimer, &QTimer::timeout,
            this, &CsaClient::onConnectionTimeout);

    m_logoutTimer->setSingleShot(true);
    connect(m_logoutTimer, &QTimer::timeout,
            this, &CsaClient::onLogoutTimeout);

    m_keepAliveTimer->setInterval(kKeepAliveIntervalMs);
    connect(m_keepAliveTimer, &QTimer::timeout,
            this, &CsaClient::onKeepAliveTimeout);
}

CsaClient::~CsaClient()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->abort();
    }
}

void CsaClient::connectToServer(const QString& host, int port, Transport transport)
{
    if (m_connectionState != ConnectionState::Disconnected) {
        if (!m_logoutPending) {
            emit errorOccurred(tr("既に接続中です"));
            return;
        }
        // LOGOUT は送ってあるので、応答を待たずに前の接続を捨てて新しい接続を始める
        qCInfo(lcNetwork) << "Abandoning pending LOGOUT to reconnect";
        abortConnection();
    }

    if (port < kMinPort || port > kMaxPort) {
        emit errorOccurred(tr("ポート番号が不正です: %1").arg(port));
        return;
    }

    if (transport == Transport::Tls && !QSslSocket::supportsSsl()) {
        emit errorOccurred(tr("TLSを利用できません。SSLライブラリを確認してください。"));
        return;
    }
    m_transport = transport;
    qCInfo(lcNetwork) << "Connecting to" << host << ":" << port;

    resetSessionState();
    setConnectionState(ConnectionState::Connecting);

    m_connectionTimer->start(kConnectionTimeoutMs);
    if (m_transport == Transport::Tls)
        m_socket->connectToHostEncrypted(host, static_cast<quint16>(port));
    else
        m_socket->connectToHost(host, static_cast<quint16>(port));
}

void CsaClient::disconnectFromServer()
{
    m_connectionTimer->stop();

    if (m_logoutPending) {
        return;  // LOGOUT:completed かタイムアウトで切断する
    }

    if (m_connectionState == ConnectionState::GameReady && isConnected()) {
        // 同意していない対局条件は拒否してから離れる
        reject(m_gameSummary.gameId);
    }

    if (isWaitingState(m_connectionState) && isConnected()) {
        // ログイン済みなら LOGOUT を送り、サーバーの LOGOUT:completed を待ってから切る
        logout();
        return;
    }

    closeSocket();
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

    if (m_transport == Transport::Tls && !m_socket->isEncrypted()) {
        abortConnection();
        emit errorOccurred(tr("安全な接続が確立されていないためログインを中止しました。"));
        return;
    }
    QString loginCmd = QStringLiteral("LOGIN %1 %2").arg(username, password);
    sendMessage(loginCmd);
}

void CsaClient::logout()
{
    if (!isWaitingState(m_connectionState) || m_logoutPending) {
        return;
    }

    if (!sendMessage(QStringLiteral("LOGOUT"))) {
        closeSocket();
        return;
    }
    m_logoutPending = true;
    m_logoutTimer->start(kLogoutTimeoutMs);
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
    // 仕様 3.3: 拒否した双方は対局待ち状態に戻る。サーバーの REJECT: 通知は待たない
    setConnectionState(ConnectionState::WaitingForGame);
}

bool CsaClient::sendMove(const QString& move)
{
    qCDebug(lcNetwork) << "CsaClient::sendMove called with:" << move;
    qCDebug(lcNetwork) << "connectionState=" << static_cast<int>(m_connectionState)
                       << "isMyTurn=" << m_isMyTurn
                       << "awaitingConfirmation=" << m_awaitingMoveConfirmation;

    if (m_connectionState != ConnectionState::InGame) {
        qCDebug(lcNetwork) << "Not in game, rejecting move";
        emit errorOccurred(tr("対局中ではありません"));
        return false;
    }

    // 手番外の送信は #ILLEGAL_ACTION で反則負けになる。サーバーの確認が届くまで次を送らない
    if (!ensureCanSendOnMyTurn(move)) {
        return false;
    }

    qCDebug(lcNetwork) << "Sending move to server:" << move;
    if (sendMessage(move)) {
        m_awaitingMoveConfirmation = true;
        return true;
    }
    return false;
}


void CsaClient::onSocketConnected()
{
    // TCP接続だけではLOGINを送らない。TLS証明書検証とハンドシェイク完了を待つ。
    if (m_transport == Transport::Tls && !m_socket->isEncrypted()) return;
    m_connectionTimer->stop();
    qCInfo(lcNetwork) << "Connected to server";
    applySocketOptions();
    setConnectionState(ConnectionState::Connected);
}

void CsaClient::onSocketDisconnected()
{
    // abort() 直後に再接続した場合、古い切断通知が新しい接続に届くことがある
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        return;
    }
    m_connectionTimer->stop();
    resetSessionState();
    qCInfo(lcNetwork) << "Disconnected from server";
    setConnectionState(ConnectionState::Disconnected);
}

void CsaClient::onSocketError(QAbstractSocket::SocketError error)
{
    m_connectionTimer->stop();

    QString errorMessage = m_socket->errorString();

    // 対局終了後やログアウト中のリモートホスト切断は正常動作として扱う
    const bool expectedClose = (m_connectionState == ConnectionState::GameOver || m_logoutPending);
    if (expectedClose && error == QAbstractSocket::RemoteHostClosedError) {
        qCInfo(lcNetwork) << "Connection closed by server (expected)";
        resetSessionState();
        setConnectionState(ConnectionState::Disconnected);
        return;
    }

    qCWarning(lcNetwork) << "Socket error:" << errorMessage;

    emit errorOccurred(errorMessage);
    resetSessionState();
    setConnectionState(ConnectionState::Disconnected);
}

void CsaClient::onReadyRead()
{
    // Bytes are bounded before decoding; UTF-8 characters may span TCP packets.
    while (m_socket->bytesAvailable() > 0) {
        const qint64 capacity = kMaxLineBytes + 1 - m_receiveBuffer.size();
        m_receiveBuffer += m_socket->read(capacity);
        qsizetype newlineIndex;
        while ((newlineIndex = m_receiveBuffer.indexOf('\n')) >= 0) {
            if (newlineIndex > kMaxLineBytes) { rejectOversizedInput(); return; }
            QByteArray bytes = m_receiveBuffer.left(newlineIndex);
            m_receiveBuffer.remove(0, newlineIndex + 1);
            if (bytes.endsWith('\r')) bytes.chop(1);
            if (bytes.isEmpty()) continue;
            const QString line = QString::fromUtf8(bytes);
            emit rawMessageReceived(line);
            processLine(line);
            if (!isConnected()) return;
        }
        if (m_receiveBuffer.size() > kMaxLineBytes) { rejectOversizedInput(); return; }
    }
}

void CsaClient::rejectOversizedInput()
{
    // Clear and disconnect before reporting the error (UI callbacks can re-enter).
    abortConnection();
    emit connectionStateChanged(ConnectionState::Disconnected);
    emit errorOccurred(tr("CSA受信データが上限を超えたため切断しました。"));
}

void CsaClient::onConnectionTimeout()
{
    if (m_connectionState != ConnectionState::Connecting) {
        return;  // 既に接続済み or 切断済みなら無視
    }
    qCWarning(lcNetwork) << "Connection timeout";
    m_socket->abort();
    emit errorOccurred(tr("接続がタイムアウトしました"));
    resetSessionState();
    setConnectionState(ConnectionState::Disconnected);
}

void CsaClient::onLogoutTimeout()
{
    if (!m_logoutPending) {
        return;
    }
    qCWarning(lcNetwork) << "LOGOUT:completed not received; closing the connection";
    m_logoutPending = false;
    closeSocket();
}

void CsaClient::handleLogoutResponse()
{
    qCInfo(lcNetwork) << "Logout completed";
    emit logoutCompleted();
    if (m_logoutPending) {
        m_logoutPending = false;
        m_logoutTimer->stop();
        closeSocket();
    }
}

void CsaClient::closeSocket()
{
    if (m_socket->state() != QAbstractSocket::UnconnectedState) {
        m_socket->disconnectFromHost();
    }
}

void CsaClient::abortConnection()
{
    m_connectionTimer->stop();
    m_logoutTimer->stop();
    m_logoutPending = false;
    // 直後に Connecting へ遷移するので、abort() が出す切断通知で Disconnected を発行させない
    m_connectionState = ConnectionState::Disconnected;
    m_socket->abort();
    resetSessionState();
}

bool CsaClient::writeLine(const QString& line)
{
    if (!isConnected() || (m_transport == Transport::Tls && !m_socket->isEncrypted())) {
        return false;
    }

    const QString msg = line + QStringLiteral("\n");
    if (m_socket->write(msg.toUtf8()) == -1) {
        qCWarning(lcNetwork) << "Failed to write message:" << m_socket->errorString();
        emit errorOccurred(tr("メッセージの送信に失敗しました: %1").arg(m_socket->errorString()));
        return false;
    }
    if (!m_socket->flush()) {
        qCWarning(lcNetwork) << "Failed to flush socket:" << m_socket->errorString();
    }

    m_lastSendTimer.start();
    // キープアライブは直前の送信から数える
    if (m_keepAliveEnabled && m_connectionState == ConnectionState::InGame) {
        m_keepAliveTimer->start();
    }
    return true;
}

bool CsaClient::sendMessage(const QString& message)
{
    if (!writeLine(message)) {
        return false;
    }

    QString displayMessage = message;
    if (message.startsWith(QStringLiteral("LOGIN "))) {
        static const QRegularExpression password(QStringLiteral("^(LOGIN\\s+\\S+\\s+)\\S+"));
        displayMessage.replace(password, QStringLiteral("\\1*****"));
    }
    emit rawMessageSent(displayMessage);
    qCDebug(lcNetwork).noquote() << "Sent:" << displayMessage;
    return true;
}

void CsaClient::setConnectionState(ConnectionState state)
{
    if (m_connectionState == state) {
        return;
    }
    const bool wasInGame = (m_connectionState == ConnectionState::InGame);
    m_connectionState = state;
    if (state == ConnectionState::InGame) {
        if (m_keepAliveEnabled) m_keepAliveTimer->start();
    } else if (wasInGame) {
        m_keepAliveTimer->stop();
    }
    emit connectionStateChanged(state);
}

void CsaClient::resetGameProgressState()
{
    m_isMyTurn = false;
    m_awaitingMoveConfirmation = false;
    m_pendingFirstResultLine.clear();
    m_endMoveConsumedTimeMs = 0;
}

void CsaClient::resetSessionState()
{
    m_receiveBuffer.clear();
    m_summaryBytes = 0;
    m_summaryLines = 0;
    m_gameSummary.clear();
    resetGameProgressState();
    m_logoutPending = false;
    m_logoutTimer->stop();
    m_keepAliveTimer->stop();
    m_lastSendTimer.invalidate();
    m_summarySection = SummarySection::Outside;
}
