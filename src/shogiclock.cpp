#include "shogiclock.h"
#include <QString>
#include <QDebug>

// 将棋クロックにより対局者の持ち時間を管理するクラス
// コンストラクタ
ShogiClock::ShogiClock(QObject *parent) : QObject(parent), m_clockRunning(false),
    m_player1ConsiderationTime(0), m_player2ConsiderationTime(0),
    m_player1TotalConsiderationTime(0), m_player2TotalConsiderationTime(0)
{
    // タイマーを生成する。
    m_timer = new QTimer(this);

    // 1秒ごとにGUIの残り時間を更新する。
    connect(m_timer, &QTimer::timeout, this, &ShogiClock::updateClock);
}

// 両対局者の最初の持ち時間を設定する。
void ShogiClock::setInitialTime(int seconds)
{
    m_player1Time = m_player2Time = seconds;

    // 持ち時間が指定されたかを判断する。
    m_timeLimitSet = (seconds > 0);
}

// 両対局者の持ち時間を設定する。
void ShogiClock::setPlayerTimes(const int player1Seconds, const int player2Seconds, const int byoyomi1Seconds, const int byoyomi2Seconds, const int binc, const int winc)
{
    // 秒読み時間が0より大きい場合は加算時間を0に設定する。
    if (byoyomi1Seconds > 0 || byoyomi2Seconds > 0) {
        m_byoyomi1Time = byoyomi1Seconds;
        m_byoyomi2Time = byoyomi2Seconds;
        m_binc = 0;
        m_winc = 0;
    }
    // 加算時間が0より大きい場合は秒読み時間を0に設定する。
    else if (binc > 0 || winc > 0) {
        m_binc = binc;
        m_winc = winc;
        m_byoyomi1Time = 0;
        m_byoyomi2Time = 0;
    }

    // 両対局者の持ち時間を設定する。
    m_player1Time = player1Seconds;
    m_player2Time = player2Seconds;

    // 持ち時間が指定されたかを判断する。
    m_timeLimitSet = (player1Seconds > 0 || player2Seconds > 0);

    // 秒読み時間が適用されているかどうかのフラグを無効に設定する。
    m_byoyomi1Applied = false;
    m_byoyomi2Applied = false;

    //begin
    qDebug() << "ShogiClock::setPlayerTimes()";
    qDebug() << "m_player1Time: " << m_player1Time;
    qDebug() << "m_player2Time: " << m_player2Time;
    qDebug() << "m_timeLimitSet: " << m_timeLimitSet;
    qDebug() << "m_byoyomi1Time: " << m_byoyomi1Time;
    qDebug() << "m_byoyomi2Time: " << m_byoyomi2Time;
    qDebug() << "m_binc: " << m_binc;
    qDebug() << "m_winc: " << m_winc;
    qDebug() << "m_byoyomi1Applied: " << m_byoyomi1Applied;
    qDebug() << "m_byoyomi2Applied: " << m_byoyomi2Applied;
    //end
}

// 手番を設定する。
void ShogiClock::setCurrentPlayer(int player)
{
    m_currentPlayer = player;
}

// 考慮時間を総考慮時間に追加する。
void ShogiClock::addConsiderationTimeToTotal(int player)
{
    saveState();

    if (player == 1) {
        m_player1TotalConsiderationTime += m_player1ConsiderationTime;
    } else {
        m_player2TotalConsiderationTime += m_player2ConsiderationTime;
    }
}

// 対局者1の考慮時間を設定する。
void ShogiClock::setPlayer1ConsiderationTime(int newPlayer1ConsiderationTime)
{
    m_player1ConsiderationTime = newPlayer1ConsiderationTime;
}

// 対局者2の考慮時間を設定する。
void ShogiClock::setPlayer2ConsiderationTime(int newPlayer2ConsiderationTime)
{
    m_player2ConsiderationTime = newPlayer2ConsiderationTime;
}

// 対局者1に秒読みが適用されているかを示すフラグを取得する。
bool ShogiClock::byoyomi1Applied() const
{
    return m_byoyomi1Applied;
}

// 対局者2に秒読みが適用されているかを示すフラグを取得する。
bool ShogiClock::byoyomi2Applied() const
{
    return m_byoyomi2Applied;
}

// タイマーを開始する。
void ShogiClock::startClock()
{
    // ゲーム開始時の残り時間を初期状態として保存する。
    saveState();

    // タイマーが開始されているかどうかのフラグを開始に設定する。
    m_clockRunning = true;

    // タイマーをスタートする。
    m_timer->start(1000);
}

// タイマーを停止する。
void ShogiClock::stopClock()
{
    // タイマーが動作中の場合
    if (m_timer->isActive()) {
        // タイマーを停止する。
        m_timer->stop();

        // タイマーが開始されているかどうかのフラグを停止に設定する。
        m_clockRunning = false;
    }
}

// 対局者1に対して秒読み時間が適用された場合、その秒読み時間を持ち時間としてリセットし、
// 考慮時間を総考慮時間に追加する。
void ShogiClock::applyByoyomiAndResetConsideration1(const bool useByoyomi)
{
    // 秒読みが適用されている場合
    if (useByoyomi) {
        // すでに秒読みに入っている場合
        if (m_byoyomi2Applied) {
            // 秒読みが1度適用されているので持ち時間を秒読み時間に設定する。
            m_player2Time = m_byoyomi2Time;

            // 残り時間の更新を通知する。
            emit timeUpdated();
        }
    }
    // 秒読みが適用されていない場合
    else {
        // 持ち時間に時間加算を追加する。
        m_player2Time += m_winc;

        // 残り時間の更新を通知する。
        emit timeUpdated();
    }

    // 考慮時間を総考慮時間に追加する。
    addConsiderationTimeToTotal(1);
}

// 対局者2に対して秒読み時間が適用された場合、その秒読み時間を持ち時間としてリセットし、
// 考慮時間を総考慮時間に追加する。
void ShogiClock::applyByoyomiAndResetConsideration2(const bool useByoyomi)
{
    // 秒読みが適用されている場合
    if (useByoyomi) {
        // すでに秒読みに入っている場合
        if (m_byoyomi1Applied) {
            // 秒読みが1度適用されているので持ち時間を秒読み時間に設定する。
            m_player1Time = m_byoyomi1Time;

            // 残り時間の更新を通知する。
            emit timeUpdated();
        }
    }
    // 秒読みが適用されていない場合
    else {
        // 持ち時間に時間加算を追加する。
        m_player1Time += m_binc;

        // 残り時間の更新を通知する。
        emit timeUpdated();
    }

    // 考慮時間を総考慮時間に追加する。
    addConsiderationTimeToTotal(2);
}

// 残り時間を更新する。
void ShogiClock::updateClock()
{
    // タイマーが動作している場合
    if (m_clockRunning) {
        // 持ち時間が指定されている場合
        if (m_timeLimitSet) {
            // 手番が対局者1の場合
            if (m_currentPlayer == 1) {
                // 残り時間が0の場合
                if (m_player1Time == 0) {
                    // タイマーを停止する。
                    stopClock();

                    // 投了の処理を行う。
                    emit resignationTriggered();
                }
                // 残り時間が1秒の場合
                else if (m_player1Time == 1) {
                    // 秒読み時間が適用されている場合
                    if (m_byoyomi1Applied) {
                        // 対局者1の残り時間を1秒減らす。
                        m_player1Time--;
                    }
                    // 秒読み時間が適用されていない場合
                    else {
                        // 秒読み時間を残り時間に設定する。
                        m_player1Time = m_byoyomi1Time;

                        // 秒読み時間が適用されたことを示すフラグを有効にする。
                        m_byoyomi1Applied = true;

                        // 対局者1の残り時間が0になった場合、対局者1の残り時間の文字色を赤色に指定する。
                        emit player1TimeOut();
                    }
                }
                // 残り時間が0より大きい場合
                else if (m_player1Time > 0) {
                    // 対局者1の残り時間を1秒減らす。
                    m_player1Time--;
                }

                // 対局者1の考慮時間を更新する。
                m_player1ConsiderationTime++;
            }
            // 手番が対局者2の場合
            else {
                // 残り時間が0の場合
                if (m_player2Time == 0) {
                    // タイマーを停止する。
                    stopClock();

                    // 投了の処理を行う。
                    emit resignationTriggered();
                }
                // 残り時間が1秒の場合
                else if (m_player2Time == 1) {
                     // 秒読み時間が適用されている場合
                    if (m_byoyomi2Applied) {
                        // 対局者2の残り時間を1秒減らす。
                        m_player2Time--;
                    }
                    // 秒読み時間が適用されていない場合
                    else {
                        // 秒読み時間を残り時間に設定する。
                        m_player2Time = m_byoyomi2Time;

                        // 秒読み時間が適用されたことを示すフラグを有効にする。
                        m_byoyomi2Applied = true;

                        // 対局者2の残り時間が0になった場合、対局者2の残り時間の文字色を赤色に指定する。
                        emit player2TimeOut();
                    }
                }
                // 残り時間が0より大きい場合
                else if (m_player2Time > 0) {
                    // 対局者2の残り時間を1秒減らす。
                    m_player2Time--;
                }

                // 対局者2の考慮時間を更新する。
                m_player2ConsiderationTime++;
            }
        } else {
            // 持ち時間が指定されていない場合、考慮時間を更新する。
            if (m_currentPlayer == 1) {
                m_player1ConsiderationTime++;
                m_player1Time++;
            } else {
                m_player2ConsiderationTime++;
                m_player2Time++;
            }
        }
    }

    // 残り時間の更新を通知する。
    emit timeUpdated();
}

// 対局者1の残り時間を取得する。
QString ShogiClock::getPlayer1Time() const
{
    int hours = m_player1Time / 3600;
    int minutes = (m_player1Time % 3600) / 60;
    int seconds = m_player1Time % 60;
    return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

// 対局者2の残り時間を取得する。
QString ShogiClock::getPlayer2Time() const
{
    int hours = m_player2Time / 3600;
    int minutes = (m_player2Time % 3600) / 60;
    int seconds = m_player2Time % 60;
    return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

// 対局者1の残り時間を取得する。
int ShogiClock::getPlayer1TimeInt() const
{
    return m_player1Time;
}

// 対局者2の残り時間を取得する。
int ShogiClock::getPlayer2TimeInt() const
{
    return m_player2Time;
}

// 対局者1の考慮時間を取得する。
QString ShogiClock::getPlayer1ConsiderationTime() const
{
    int minutes = m_player1ConsiderationTime / 60;
    int seconds = m_player1ConsiderationTime % 60;

    return QString::asprintf("%02d:%02d", minutes, seconds);
}

// 対局者2の考慮時間を取得する。
QString ShogiClock::getPlayer2ConsiderationTime() const
{
    int minutes = m_player2ConsiderationTime / 60;
    int seconds = m_player2ConsiderationTime % 60;

    return QString::asprintf("%02d:%02d", minutes, seconds);
}

// 対局者1の総考慮時間を取得する。
QString ShogiClock::getPlayer1TotalConsiderationTime() const
{
    int hours = m_player1TotalConsiderationTime / 3600;
    int minutes = (m_player1TotalConsiderationTime % 3600) / 60;
    int seconds = m_player1TotalConsiderationTime % 60;

    return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

// 対局者2の総考慮時間を取得する。
QString ShogiClock::getPlayer2TotalConsiderationTime() const
{
    int hours = m_player2TotalConsiderationTime / 3600;
    int minutes = (m_player2TotalConsiderationTime % 3600) / 60;
    int seconds = m_player2TotalConsiderationTime % 60;

    return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

// 対局者1の考慮時間と総考慮時間を "MM:SS/HH:MM:SS" 形式で取得する。
QString ShogiClock::getPlayer1ConsiderationAndTotalTime() const
{
    //begin
    qDebug() << "ShogiClock::getPlayer1ConsiderationAndTotalTime()";
    qDebug() << "m_player1ConsiderationTime: " << m_player1ConsiderationTime;
    qDebug() << "m_player1TotalConsiderationTime: " << m_player1TotalConsiderationTime;
    //end

    return getPlayer1ConsiderationTime() + "/" + getPlayer1TotalConsiderationTime();
}

// 対局者2の考慮時間と総考慮時間を "MM:SS/HH:MM:SS" 形式で取得する。
QString ShogiClock::getPlayer2ConsiderationAndTotalTime() const
{
    //begin
    qDebug() << "ShogiClock::getPlayer2ConsiderationAndTotalTime()";
    qDebug() << "m_player2ConsiderationTime: " << m_player2ConsiderationTime;
    qDebug() << "m_player2TotalConsiderationTime: " << m_player2TotalConsiderationTime;
    //end

    return getPlayer2ConsiderationTime() + "/" + getPlayer2TotalConsiderationTime();
}

// 現在の状態を時間履歴に保存する。
void ShogiClock::saveState()
{
    //begin
    qDebug() << "--------------ShogiClock::saveState()---------------";
    qDebug() << "m_player1Time: " << m_player1Time;
    qDebug() << "m_player2Time: " << m_player2Time;
    qDebug() << "m_player1ConsiderationTime: " << m_player1ConsiderationTime;
    qDebug() << "m_player2ConsiderationTime: " << m_player2ConsiderationTime;
    qDebug() << "m_player1TotalConsiderationTime: " << m_player1TotalConsiderationTime;
    qDebug() << "m_player2TotalConsiderationTime: " << m_player2TotalConsiderationTime;
    qDebug() << "----------------------------------------------------";
    //end

    // 対局者1の残り時間を保存する。
    m_player1TimeHistory.push(m_player1Time);

    // 対局者2の残り時間を保存する。
    m_player2TimeHistory.push(m_player2Time);

    // 対局者1の考慮時間を保存する。
    m_player1ConsiderationHistory.push(m_player1ConsiderationTime);

    // 対局者2の考慮時間を保存する。
    m_player2ConsiderationHistory.push(m_player2ConsiderationTime);

    // 対局者1の総考慮時間を保存する。
    m_player1TotalConsiderationHistory.push(m_player1TotalConsiderationTime);

    // 対局者2の総考慮時間を保存する。
    m_player2TotalConsiderationHistory.push(m_player2TotalConsiderationTime);
}

// 「待った」をした場合、状態を2手前の残り時間、考慮時間、総考慮時間に戻す。
// 例．
// 0手目の初期状態の残り時間　先手60秒、後手60秒
// 1手目の初手後の残り時間　先手50秒、後手60秒（10秒考えて1手目指した後）
// 2手目後の残り時間　先手50秒、後手50秒
// 3手目後の残り時間　先手40秒、後手50秒（10秒考えて3手目指した後）
// 4手目後の残り時間　先手40秒、後手40秒
// 5手目を指す前に「待った」をした　→　2手目後の残り時間を先手50秒、後手50秒に戻す。
void ShogiClock::undo()
{
    // 各対局者の時間履歴が3以上存在する場合
    if (m_player1TimeHistory.size() >= 3 && m_player2TimeHistory.size() >= 3) {
        // 対局者1の残り時間を2手分戻す。
        m_player1TimeHistory.pop();
        m_player1TimeHistory.pop();
        m_player1Time = m_player1TimeHistory.top();

        // 対局者1の残り時間を2手分戻す。
        m_player2TimeHistory.pop();
        m_player2TimeHistory.pop();
        m_player2Time = m_player2TimeHistory.top();

        // 対局者1の考慮時間を2手分戻す。
        m_player1ConsiderationHistory.pop();
        m_player1ConsiderationHistory.pop();
        m_player1ConsiderationTime = m_player1ConsiderationHistory.top();

        // 対局者2の考慮時間を2手分戻す。
        m_player2ConsiderationHistory.pop();
        m_player2ConsiderationHistory.pop();
        m_player2ConsiderationTime = m_player2ConsiderationHistory.top();

        // 対局者1の総考慮時間を2手分戻す。
        m_player1TotalConsiderationHistory.pop();
        m_player1TotalConsiderationHistory.pop();
        m_player1TotalConsiderationTime = m_player1TotalConsiderationHistory.top();

        // 対局者2の総考慮時間を2手分戻す。
        m_player2TotalConsiderationHistory.pop();
        m_player2TotalConsiderationHistory.pop();
        m_player2TotalConsiderationTime = m_player2TotalConsiderationHistory.top();

        // 残り時間の更新を通知する。
        emit timeUpdated();
    }
}
