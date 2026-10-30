/// @file csaclient.cpp
/// @brief CSAプロトコルクライアントクラスの実装

#include <QtGlobal>
#if defined(Q_OS_WIN)
#include <winsock2.h>
#include <mstcpip.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#endif

#include "csaclient.h"
#include "logcategories.h"
#include <QRegularExpression>

namespace {
constexpr int kMinPort = 1;
constexpr int kMaxPort = 65535;
constexpr int kRejectPrefixLength = 7;  // "REJECT:"
} // namespace

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
    // 受信の処理（終局・切断の知らせを含む）はソケットの readyRead の外で行う。処理の中でソケットを閉じてもよい
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
            // 行の途中の CR などの制御文字は空白にする（対局者名などに残ると、保存した棋譜を読み直したときに
            // 別の行として読まれる。セキュリティ監査 S9）
            for (char& byte : bytes) {
                if ((static_cast<unsigned char>(byte) < 0x20 && byte != '\t') || byte == 0x7F) byte = ' ';
            }
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
    m_summaryReader.reset();
    m_gameSummary = {};
    resetGameProgressState();
    m_logoutPending = false;
    m_logoutTimer->stop();
    m_keepAliveTimer->stop();
    m_lastSendTimer.invalidate();
}

// ============================================================
// 受信行の処理
// ============================================================

void CsaClient::processLine(const QString& line)
{
    qCDebug(lcNetwork).noquote() << "Recv:" << line;

    // Game_Summary を読んでいる間
    if (m_summaryReader) {
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
    case ConnectionState::GameOver:
        // 終局後もログインしたまま対局待ち状態に戻る（仕様 3.1、3.4）
        processWaitingLine(line);
        break;
    case ConnectionState::GameReady:
        if (line.startsWith(QStringLiteral("START:"))) {
            // 対局開始
            QString gameId = line.mid(6);
            resetGameProgressState();
            setConnectionState(ConnectionState::InGame);
            m_isMyTurn = m_gameSummary.movesFirst();
            emit gameStarted(gameId);
        } else if (line.startsWith(QStringLiteral("REJECT:"))) {
            // 対局拒否
            // REJECT:<GameID> by <rejector>  （Game_ID 省略時は "REJECT: by <rejector>"）
            qsizetype byIndex = line.indexOf(QStringLiteral(" by "));
            if (byIndex < kRejectPrefixLength || byIndex + 4 >= line.size()) {
                qCWarning(lcNetwork) << "Malformed REJECT line:" << line;
                emit errorOccurred(tr("不正なREJECT応答を受信しました: %1").arg(line));
                return;
            }
            QString gameId = line.mid(kRejectPrefixLength, static_cast<int>(byIndex) - kRejectPrefixLength);
            QString rejector = line.mid(static_cast<int>(byIndex) + 4);
            setConnectionState(ConnectionState::WaitingForGame);
            emit gameRejected(gameId, rejector);
        } else if (line.startsWith(QStringLiteral("LOGOUT:"))) {
            handleLogoutResponse();
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

void CsaClient::processWaitingLine(const QString& line)
{
    if (line == QStringLiteral("BEGIN Game_Summary")) {
        m_summaryReader.emplace();
    } else if (line.startsWith(QStringLiteral("LOGOUT:"))) {
        handleLogoutResponse();
    }
    // 自分が送った REJECT に対する "REJECT:<GameID> by <自分>" はここに届くが、何もしない
}

void CsaClient::processGameSummary(const QString& line)
{
    switch (m_summaryReader->read(line)) {
    case CsaGameSummaryReader::Status::Reading:
        break;
    case CsaGameSummaryReader::Status::Finished:
        m_gameSummary = m_summaryReader->summary();
        m_summaryReader.reset();
        setConnectionState(ConnectionState::GameReady);
        emit gameSummaryReceived(m_gameSummary);
        break;
    case CsaGameSummaryReader::Status::TooLarge:
        rejectOversizedInput();
        break;
    }
}

void CsaClient::processGameMessage(const QString& line)
{
    qCDebug(lcNetwork) << "processGameMessage:" << line;

    // shogi-serverの拡張コマンド応答（例: ##[WHO]）は終局通知ではない。
    // 通信ログには受信時に記録済みなので、対局状態には反映しない。
    if (line.startsWith(QStringLiteral("##"))) return;

    // #で始まる行は結果行
    if (line.startsWith(QLatin1Char('#'))) {
        qCDebug(lcNetwork) << "Result line detected";
        processResultLine(line);
        return;
    }

    // %で始まる行は特殊コマンド（投了、勝利宣言など）の確認
    if (line.startsWith(QLatin1Char('%'))) {
        qCDebug(lcNetwork) << "Special command line detected";
        // %TORYO,T4 形式。意思表示をしたのは手番側なので、その側の時間単位で換算する
        const int consumedTime = CsaGameSummary::consumedUnits(line);
        const bool moverIsBlack = m_isMyTurn ? m_gameSummary.isBlackSide() : !m_gameSummary.isBlackSide();

        // 終局手の消費時間を保存（ミリ秒に変換）
        m_endMoveConsumedTimeMs = m_gameSummary.unitsToMs(moverIsBlack, consumedTime);
        qCDebug(lcNetwork) << "End move consumed time:" << m_endMoveConsumedTimeMs << "ms";
        m_awaitingMoveConfirmation = false;
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

    // 中断通知は勝敗行を伴わない単独の終局通知。
    if (line == QStringLiteral("#CHUDAN")) {
        m_pendingFirstResultLine.clear();
        m_isMyTurn = false;
        m_awaitingMoveConfirmation = false;
        setConnectionState(ConnectionState::GameOver);
        emit gameInterrupted();
        emit gameEnded(GameResult::Chudan, GameEndCause::Chudan, m_endMoveConsumedTimeMs);
        return;
    }

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
    } else if (firstLine == QStringLiteral("#ILLEGAL_ACTION")) {
        cause = GameEndCause::IllegalAction;
    }

    qCDebug(lcNetwork) << "Game ended with result=" << static_cast<int>(result)
                       << "cause=" << static_cast<int>(cause)
                       << "consumedTimeMs=" << m_endMoveConsumedTimeMs;

    m_isMyTurn = false;
    m_awaitingMoveConfirmation = false;
    // 終局後は GameOver のまま対局待ちとして扱う。サーバーが切断しても正常終了、
    // 接続が残れば次の Game_Summary をそのまま受け付ける
    setConnectionState(ConnectionState::GameOver);
    emit gameEnded(result, cause, m_endMoveConsumedTimeMs);
}

void CsaClient::processMoveLine(const QString& line)
{
    // 形式: +7776FU,T12
    const qsizetype commaPos = line.indexOf(QLatin1Char(','));
    const QString move = (commaPos > 0) ? line.left(static_cast<int>(commaPos)) : line;
    const int consumedTime = CsaGameSummary::consumedUnits(line);

    // 手番を判定（指し手の先頭文字で判定）し、指した側の時間単位でミリ秒に変換
    const bool isBlackMove = move.startsWith(QLatin1Char('+'));
    const int consumedTimeMs = m_gameSummary.unitsToMs(isBlackMove, consumedTime);
    const bool wasMyTurn = m_isMyTurn;

    // 手番を更新（相手が指したら自分の手番に）
    m_isMyTurn = m_gameSummary.myTurn == (isBlackMove ? Turn::White : Turn::Black);
    m_awaitingMoveConfirmation = false;

    if (wasMyTurn) {
        // 自分の指し手の確認
        emit moveConfirmed(move, consumedTimeMs);
    } else {
        // 相手の指し手
        emit moveReceived(move, consumedTimeMs);
    }
}

// ============================================================
// 手番に縛られる送信（指し手・意思表示・生コマンド）
// ============================================================

bool CsaClient::ensureCanSendOnMyTurn(const QString& what)
{
    if (!m_isMyTurn) {
        emit commandRejected(tr("自分の手番ではないため送信しません: %1").arg(what));
        return false;
    }
    if (m_awaitingMoveConfirmation) {
        emit commandRejected(tr("直前の送信に対するサーバーの確認を待っているため送信しません: %1").arg(what));
        return false;
    }
    return true;
}

bool CsaClient::isTurnBoundCommand(const QString& command)
{
    if (command.startsWith(QLatin1Char('+')) || command.startsWith(QLatin1Char('-'))) {
        return true;
    }
    // %TORYO などの意思表示。shogi-server の拡張コマンド（%%）は手番に関係なく送れる
    return command.startsWith(QLatin1Char('%')) && !command.startsWith(QStringLiteral("%%"));
}

void CsaClient::sendRawCommand(const QString& command)
{
    if (command.isEmpty()) {
        // LF のみのキープアライブ（仕様 3.4）。対局中に限り、直前の送信から30秒以上空ける
        if (m_connectionState != ConnectionState::InGame) {
            emit commandRejected(tr("キープアライブ（空行）は対局中にのみ送信できます"));
            return;
        }
        if (m_lastSendTimer.isValid() && m_lastSendTimer.elapsed() < kKeepAliveMinIntervalMs) {
            emit commandRejected(tr("キープアライブは直前の送信から30秒以上空ける必要があります"));
            return;
        }
        writeLine(QString());
        return;
    }

    if (m_connectionState == ConnectionState::InGame && isTurnBoundCommand(command)) {
        if (!ensureCanSendOnMyTurn(command)) {
            return;
        }
        if (sendMessage(command)) {
            m_awaitingMoveConfirmation = true;
        }
        return;
    }

    sendMessage(command);
}

void CsaClient::resign()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }
    if (!ensureCanSendOnMyTurn(QStringLiteral("%TORYO"))) {
        return;
    }

    if (sendMessage(QStringLiteral("%TORYO"))) {
        m_awaitingMoveConfirmation = true;
    }
}

void CsaClient::declareWin()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }
    if (!ensureCanSendOnMyTurn(QStringLiteral("%KACHI"))) {
        return;
    }

    if (sendMessage(QStringLiteral("%KACHI"))) {
        m_awaitingMoveConfirmation = true;
    }
}

void CsaClient::requestChudan()
{
    if (m_connectionState != ConnectionState::InGame) {
        return;
    }
    if (!ensureCanSendOnMyTurn(QStringLiteral("%CHUDAN"))) {
        return;
    }

    if (sendMessage(QStringLiteral("%CHUDAN"))) {
        m_awaitingMoveConfirmation = true;
    }
}

// ============================================================
// 接続の維持
// ============================================================

void CsaClient::setKeepAliveEnabled(bool enabled)
{
    m_keepAliveEnabled = enabled;
    if (!enabled) {
        m_keepAliveTimer->stop();
    } else if (m_connectionState == ConnectionState::InGame) {
        m_keepAliveTimer->start();
    }
}

void CsaClient::setKeepAliveIntervalMs(int intervalMs)
{
    // 仕様 3.4: サーバーが受信してから30秒を経ずに送ってはならない
    m_keepAliveTimer->setInterval(qMax(kKeepAliveMinIntervalMs, intervalMs));
}

void CsaClient::applySocketOptions()
{
    // 小さな行をまとめて遅らせない（ShogiHome と同じく Nagle を無効化）
    m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);

    if (!m_tcpKeepAliveEnabled) {
        return;
    }
    m_socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);

    // Qt は SO_KEEPALIVE を立てるだけで間隔は OS の既定値（Linux 2時間など）のままなので、
    // NAT の無通信タイムアウトより短い間隔をソケットごとに設定する
    const qintptr descriptor = m_socket->socketDescriptor();
    if (descriptor < 0) {
        qCWarning(lcNetwork) << "TCP keep-alive: no socket descriptor";
        return;
    }
#if defined(Q_OS_WIN)
    tcp_keepalive values{};
    values.onoff = 1;
    values.keepalivetime = static_cast<ULONG>(kTcpKeepAliveIdleSec) * 1000UL;
    values.keepaliveinterval = static_cast<ULONG>(kTcpKeepAliveIntervalSec) * 1000UL;
    DWORD returned = 0;
    if (WSAIoctl(static_cast<SOCKET>(descriptor), SIO_KEEPALIVE_VALS, &values, sizeof(values),
                 nullptr, 0, &returned, nullptr, nullptr) != 0) {
        qCWarning(lcNetwork) << "TCP keep-alive: WSAIoctl failed:" << WSAGetLastError();
    }
#else
    const int fd = static_cast<int>(descriptor);
    const int idle = kTcpKeepAliveIdleSec;
    const int interval = kTcpKeepAliveIntervalSec;
    const int count = kTcpKeepAliveProbeCount;
#if defined(Q_OS_MACOS)
    const int idleOption = TCP_KEEPALIVE;
#else
    const int idleOption = TCP_KEEPIDLE;
#endif
    if (setsockopt(fd, IPPROTO_TCP, idleOption, &idle, sizeof(idle)) != 0
        || setsockopt(fd, IPPROTO_TCP, TCP_KEEPINTVL, &interval, sizeof(interval)) != 0
        || setsockopt(fd, IPPROTO_TCP, TCP_KEEPCNT, &count, sizeof(count)) != 0) {
        qCWarning(lcNetwork) << "TCP keep-alive: setsockopt failed";
    }
#endif
    qCDebug(lcNetwork) << "TCP keep-alive enabled: idle" << kTcpKeepAliveIdleSec
                       << "s interval" << kTcpKeepAliveIntervalSec << "s";
}

void CsaClient::onKeepAliveTimeout()
{
    if (m_connectionState != ConnectionState::InGame || !isConnected()) {
        m_keepAliveTimer->stop();
        return;
    }
    qCDebug(lcNetwork) << "Sending keep-alive LF";
    writeLine(QString());
}
