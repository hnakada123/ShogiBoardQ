/// @file csaclient_parser.cpp
/// @brief CSAプロトコルクライアント - 受信行解析処理

#include "csaclient.h"
#include "logcategories.h"

namespace {
bool parseNonNegativeIntField(const QString& key, const QString& value, int& out)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok || parsed < 0) {
        qCWarning(lcNetwork) << "Invalid numeric value for" << key << ":" << value;
        return false;
    }
    out = parsed;
    return true;
}
} // namespace

// ============================================================
// 受信行解析
// ============================================================

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
            if (byIndex <= 7 || byIndex + 4 >= line.size()) {
                qCWarning(lcNetwork) << "Malformed REJECT line:" << line;
                emit errorOccurred(tr("不正なREJECT応答を受信しました: %1").arg(line));
                return;
            }
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
    if (handleGameSummarySectionBoundary(line)) return;
    if (handleGameSummaryTimeSectionLine(line)) return;
    if (handleGameSummaryPositionSectionLine(line)) return;
    processGameSummaryGeneralInfoLine(line);
}

bool CsaClient::handleGameSummarySectionBoundary(const QString& line)
{
    if (line == QStringLiteral("END Game_Summary")) {
        m_inGameSummary = false;
        m_inTimeSection = false;
        m_inPositionSection = false;
        setConnectionState(ConnectionState::GameReady);
        emit gameSummaryReceived(m_gameSummary);
        return true;
    }

    if (line == QStringLiteral("END Time") ||
        line == QStringLiteral("END Time+") ||
        line == QStringLiteral("END Time-")) {
        m_inTimeSection = false;
        return true;
    }

    if (line == QStringLiteral("END Position")) {
        m_inPositionSection = false;
        return true;
    }

    if (line == QStringLiteral("BEGIN Time")) {
        m_inTimeSection = true;
        m_currentTimeSection = QStringLiteral("Time");
        return true;
    }
    if (line == QStringLiteral("BEGIN Time+")) {
        m_inTimeSection = true;
        m_currentTimeSection = QStringLiteral("Time+");
        m_gameSummary.hasIndividualTime = true;
        return true;
    }
    if (line == QStringLiteral("BEGIN Time-")) {
        m_inTimeSection = true;
        m_currentTimeSection = QStringLiteral("Time-");
        m_gameSummary.hasIndividualTime = true;
        return true;
    }
    if (line == QStringLiteral("BEGIN Position")) {
        m_inPositionSection = true;
        return true;
    }

    return false;
}

bool CsaClient::handleGameSummaryTimeSectionLine(const QString& line)
{
    if (!m_inTimeSection) return false;

    const qsizetype colonPos = line.indexOf(QLatin1Char(':'));
    if (colonPos <= 0) return true;

    const QString key = line.left(static_cast<int>(colonPos));
    const QString value = line.mid(static_cast<int>(colonPos) + 1);

    if (key == QStringLiteral("Time_Unit")) {
        m_gameSummary.timeUnit = value;
        return true;
    }

    if (key == QStringLiteral("Total_Time")) {
        int time = 0;
        if (!parseNonNegativeIntField(key, value, time)) return true;
        if (m_currentTimeSection == QStringLiteral("Time+")) {
            m_gameSummary.totalTimeBlack = time;
        } else if (m_currentTimeSection == QStringLiteral("Time-")) {
            m_gameSummary.totalTimeWhite = time;
        } else {
            m_gameSummary.totalTime = time;
        }
        return true;
    }

    if (key == QStringLiteral("Byoyomi")) {
        int time = 0;
        if (!parseNonNegativeIntField(key, value, time)) return true;
        if (m_currentTimeSection == QStringLiteral("Time+")) {
            m_gameSummary.byoyomiBlack = time;
        } else if (m_currentTimeSection == QStringLiteral("Time-")) {
            m_gameSummary.byoyomiWhite = time;
        } else {
            m_gameSummary.byoyomi = time;
        }
        return true;
    }

    if (key == QStringLiteral("Least_Time_Per_Move")) {
        (void)parseNonNegativeIntField(key, value, m_gameSummary.leastTimePerMove);
    } else if (key == QStringLiteral("Increment")) {
        (void)parseNonNegativeIntField(key, value, m_gameSummary.increment);
    } else if (key == QStringLiteral("Delay")) {
        (void)parseNonNegativeIntField(key, value, m_gameSummary.delay);
    } else if (key == QStringLiteral("Time_Roundup")) {
        m_gameSummary.timeRoundup = (value == QStringLiteral("YES"));
    }
    return true;
}

bool CsaClient::handleGameSummaryPositionSectionLine(const QString& line)
{
    if (!m_inPositionSection) return false;

    // P1-P9, P+, P-, +, - で始まる行は局面情報
    if (line.startsWith(QLatin1Char('P')) ||
        line == QStringLiteral("+") ||
        line == QStringLiteral("-")) {
        m_gameSummary.positionLines.append(line);
        return true;
    }

    // 指し手行（+7776FU,T12 形式）
    if ((line.startsWith(QLatin1Char('+')) || line.startsWith(QLatin1Char('-'))) &&
        line.length() > 1) {
        const qsizetype commaPos = line.indexOf(QLatin1Char(','));
        const QString move = (commaPos > 0) ? line.left(static_cast<int>(commaPos)) : line;
        m_gameSummary.moves.append(move);
    }
    return true;
}

void CsaClient::processGameSummaryGeneralInfoLine(const QString& line)
{
    const qsizetype colonPos = line.indexOf(QLatin1Char(':'));
    if (colonPos <= 0) return;

    const QString key = line.left(static_cast<int>(colonPos));
    const QString value = line.mid(static_cast<int>(colonPos) + 1);

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
        (void)parseNonNegativeIntField(key, value, m_gameSummary.maxMoves);
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

int CsaClient::parseConsumedTime(const QString& timeStr) const
{
    bool ok = false;
    const int value = timeStr.toInt(&ok);
    if (!ok || value < 0) {
        qCWarning(lcNetwork) << "Invalid consumed time token:" << timeStr;
        return 0;
    }
    return value;
}
