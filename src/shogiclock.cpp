#include "shogiclock.h"
#include <QString>
#include <QDebug>

// 将棋クロックにより対局者の持ち時間を管理するクラス
// コンストラクタ
ShogiClock::ShogiClock(QObject *parent) : QObject(parent), m_clockRunning(false),
    m_player1ConsiderationTimeMs(0), m_player2ConsiderationTimeMs(0),
    m_player1TotalConsiderationTimeMs(0), m_player2TotalConsiderationTimeMs(0)
{
    // タイマーを生成する。
    m_timer = new QTimer(this);
    m_timer->setTimerType(Qt::PreciseTimer);
    m_timer->setInterval(50); // 50ms刻み（GUIは後述の秒変化時のみemit）

    // 1秒ごとにGUIの残り時間を更新する。
    connect(m_timer, &QTimer::timeout, this, &ShogiClock::updateClock);
}

// タイムアウト時に負けにするかどうかのフラグを設定する。
void ShogiClock::setLoseOnTimeout(bool v)
{
    m_loseOnTimeout = v;
}

// 両対局者の持ち時間を設定する。
void ShogiClock::setPlayerTimes(const int player1Seconds, const int player2Seconds, const int byoyomi1Seconds, const int byoyomi2Seconds,
                                const int binc, const int winc, const bool isLimitedTime)
{
    //begin
    qDebug() << "----in ShogiClock::setPlayerTimes ----";
    qDebug() << "player1Seconds: " << player1Seconds;
    qDebug() << "player2Seconds: " << player2Seconds;
    qDebug() << "byoyomi1Seconds: " << byoyomi1Seconds;
    qDebug() << "byoyomi2Seconds: " << byoyomi2Seconds;
    qDebug() << "binc: " << binc;
    qDebug() << "winc: " << winc;
    //end

    // 秒読み時間が0より大きい場合は、加算時間を0に設定する。
    if (byoyomi1Seconds > 0 || byoyomi2Seconds > 0) {
        m_byoyomi1TimeMs = byoyomi1Seconds * 1000;
        m_byoyomi2TimeMs = byoyomi2Seconds * 1000;
        m_bincMs = 0;
        m_wincMs = 0;
    }
    // 加算時間が0より大きい場合は、秒読み時間を0に設定する。
    else if (binc > 0 || winc > 0) {
        m_byoyomi1TimeMs = 0;
        m_byoyomi2TimeMs = 0;
        m_bincMs = binc * 1000;
        m_wincMs = winc * 1000;
    }
    // 秒読み時間と加算時間が両方とも0の場合は、秒読み時間と加算時間を0に設定する。
    else {
        m_byoyomi1TimeMs = 0;
        m_byoyomi2TimeMs = 0;
        m_bincMs = 0;
        m_wincMs = 0;
    }

    // 両対局者の持ち時間を設定する。
    m_player1TimeMs = player1Seconds * 1000;
    m_player2TimeMs = player2Seconds * 1000;

    // 持ち時間が指定されたかを判断する。
    m_timeLimitSet = isLimitedTime;

    // 秒読み時間が適用されているかどうかのフラグを無効に設定する。
    m_byoyomi1Applied = false;
    m_byoyomi2Applied = false;

    //begin
    qDebug() << "ShogiClock::setPlayerTimes()";
    qDebug() << "m_player1Time: " << m_player1TimeMs;
    qDebug() << "m_player2Time: " << m_player2TimeMs;
    qDebug() << "m_timeLimitSet: " << m_timeLimitSet;
    qDebug() << "m_byoyomi1Time: " << m_byoyomi1TimeMs;
    qDebug() << "m_byoyomi2Time: " << m_byoyomi2TimeMs;
    qDebug() << "m_binc: " << m_bincMs;
    qDebug() << "m_winc: " << m_wincMs;
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
        m_player1TotalConsiderationTimeMs += m_player1ConsiderationTimeMs;
    } else {
        m_player2TotalConsiderationTimeMs += m_player2ConsiderationTimeMs;
    }
}

// 対局者1の考慮時間を設定する。
void ShogiClock::setPlayer1ConsiderationTime(int newPlayer1ConsiderationTime)
{
    m_player1ConsiderationTimeMs = newPlayer1ConsiderationTime;
}

// 対局者2の考慮時間を設定する。
void ShogiClock::setPlayer2ConsiderationTime(int newPlayer2ConsiderationTime)
{
    m_player2ConsiderationTimeMs = newPlayer2ConsiderationTime;
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
    // タイマーがすでに動作中の場合は何もしない。
    if (m_clockRunning) return;

    // タイマーが動作していない場合、対局者の持ち時間と考慮時間を初期状態として保存する。
    saveState();

    // 経過時間を計測するためのタイマーを開始する。
    m_elapsedTimer.restart();

    // 最後にタイマーが更新された時間を設定する。
    m_lastTickMs = m_elapsedTimer.elapsed();

    // 現在の表示秒（切り上げ）で初期化して即表示する。
    m_prevShownSecP1 = qMax<qint64>(0, (m_player1TimeMs + 999) / 1000);
    m_prevShownSecP2 = qMax<qint64>(0, (m_player2TimeMs + 999) / 1000);

    // 残り時間の更新を通知する。
    emit timeUpdated();

    // タイマーを開始する。
    // m_timerは50ms刻みで設定されていると想定
    m_timer->start();

    // タイマーが開始されているかどうかのフラグを開始に設定する。
    m_clockRunning = true;
}

// タイマーを停止する。
void ShogiClock::stopClock()
{
    if (!m_clockRunning) return;

    // 最後の差分を反映してから止める
    qint64 now = m_elapsedTimer.elapsed();
    int elapsed = static_cast<int>(now - m_lastTickMs);
    if (elapsed > 0) {
        m_lastTickMs = now;
        // 手番側に経過時間を反映（下のupdateClock本体と同じ処理）
        if (m_timeLimitSet) {
            if (m_currentPlayer == 1) {
                m_player1TimeMs -= elapsed;
                m_player1ConsiderationTimeMs += elapsed;
                if (m_player1TimeMs <= 0) m_player1TimeMs = 0;
            } else {
                m_player2TimeMs -= elapsed;
                m_player2ConsiderationTimeMs += elapsed;
                if (m_player2TimeMs <= 0) m_player2TimeMs = 0;
            }
        } else {
            if (m_currentPlayer == 1) m_player1ConsiderationTimeMs += elapsed;
            else                     m_player2ConsiderationTimeMs += elapsed;
        }
    }

    m_timer->stop();
    m_clockRunning = false;

    // 最後に一回だけGUI更新（秒が変わっていれば発火）
    emit timeUpdated();
}

// 対局者1に対して秒読み時間が適用された場合、その秒読み時間を持ち時間としてリセットし、
// 考慮時間を総考慮時間に追加する。
void ShogiClock::applyByoyomiAndResetConsideration1(const bool useByoyomi)
{
    // 秒読みが適用されている場合
    if (useByoyomi) {
        //
        if (m_byoyomi1TimeMs > 0) m_player1TimeMs = m_byoyomi1TimeMs;

        // すでに秒読みに入っている。
        m_byoyomi1Applied = true;
    } else {
        // 秒読みが適用されていない場合、持ち時間に加算時間を追加する。
        m_player1TimeMs += m_bincMs;
    }

    // 考慮時間を総考慮時間に追加する。
    addConsiderationTimeToTotal(1);

    // 残り時間の更新を通知する。
    emit timeUpdated();
}

// 対局者2に対して秒読み時間が適用された場合、その秒読み時間を持ち時間としてリセットし、
// 考慮時間を総考慮時間に追加する。
void ShogiClock::applyByoyomiAndResetConsideration2(const bool useByoyomi)
{
    // 秒読みが適用されている場合
    if (useByoyomi) {
        //
        if (m_byoyomi2TimeMs > 0) m_player2TimeMs = m_byoyomi2TimeMs;

        // すでに秒読みに入っている。
        m_byoyomi2Applied = true;
    } else {
        // 秒読みが適用されていない場合、持ち時間に加算時間を追加する。
        m_player2TimeMs += m_wincMs;
    }

    // 考慮時間を総考慮時間に追加する。
    addConsiderationTimeToTotal(2);

    // 残り時間の更新を通知する。
    emit timeUpdated();
}

// 残り時間を更新する。
void ShogiClock::updateClock()
{
    // 動作していなければ何もしない
    if (!m_clockRunning) return;

    const qint64 now = m_elapsedTimer.elapsed();
    const int elapsed = static_cast<int>(now - m_lastTickMs);
    if (elapsed <= 0) return;
    m_lastTickMs = now;

    if (m_timeLimitSet) {
        if (m_currentPlayer == 1) {
            m_player1TimeMs -= elapsed;
            m_player1ConsiderationTimeMs += elapsed;

            if (m_player1TimeMs <= 0) {
                m_player1TimeMs = 0;
                if (m_loseOnTimeout) {
                    // ここで即座に終了・通知
                    stopClock();
                    emit player1TimeOut();
                    emit resignationTriggered();
                    emit timeUpdated(); // UIを確実に0表示へ
                    return;
                }
            }
        } else { // 後手
            m_player2TimeMs -= elapsed;
            m_player2ConsiderationTimeMs += elapsed;

            if (m_player2TimeMs <= 0) {
                m_player2TimeMs = 0;
                if (m_loseOnTimeout) {
                    stopClock();
                    emit player2TimeOut();
                    emit resignationTriggered();
                    emit timeUpdated();
                    return;
                }
            }
        }
    } else {
        // 持ち時間制でない場合は考慮時間のみ積算
        if (m_currentPlayer == 1) {
            m_player1ConsiderationTimeMs += elapsed;
        } else {
            m_player2ConsiderationTimeMs += elapsed;
        }
    }

    // ★ 秒が変わったときだけGUI更新（切り上げで合わせる）
    const qint64 sec1 = qMax<qint64>(0, (m_player1TimeMs + 999) / 1000);
    const qint64 sec2 = qMax<qint64>(0, (m_player2TimeMs + 999) / 1000);
    if (sec1 != m_prevShownSecP1 || sec2 != m_prevShownSecP2) {
        m_prevShownSecP1 = sec1;
        m_prevShownSecP2 = sec2;
        emit timeUpdated();
    }
}

// 対局者1の残り時間を文字列形式で取得する。
QString ShogiClock::getPlayer1TimeString() const
{
    int totalSeconds = qMax(0, (m_player1TimeMs + 999) / 1000); // ★切り上げ
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

// 対局者2の残り時間を文字列形式で取得する。
QString ShogiClock::getPlayer2TimeString() const
{
    int totalSeconds = qMax(0, (m_player2TimeMs + 999) / 1000); // ★切り上げ
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

// 対局者1の残り時間を取得する。
int ShogiClock::getPlayer1TimeIntMs() const
{
    return m_player1TimeMs;
}

// 対局者2の残り時間を取得する。
int ShogiClock::getPlayer2TimeIntMs() const
{
    return m_player2TimeMs;
}

// 対局者1の考慮時間を文字列形式で取得する。
QString ShogiClock::getPlayer1ConsiderationTime() const
{
    int totalSeconds = qMax(0, m_player1ConsiderationTimeMs) / 1000;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    return QString::asprintf("%02d:%02d", minutes, seconds);
}

// 対局者2の考慮時間を文字列形式で取得する。
QString ShogiClock::getPlayer2ConsiderationTime() const
{
    int totalSeconds = qMax(0, m_player2ConsiderationTimeMs) / 1000;
    int minutes = totalSeconds / 60;
    int seconds = totalSeconds % 60;
    return QString::asprintf("%02d:%02d", minutes, seconds);
}

// 対局者1の総考慮時間を文字列形式で取得する。
QString ShogiClock::getPlayer1TotalConsiderationTime() const
{
    int totalSeconds = qMax(0, m_player1TotalConsiderationTimeMs) / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

// 対局者2の総考慮時間を文字列形式で取得する。
QString ShogiClock::getPlayer2TotalConsiderationTime() const
{
    int totalSeconds = qMax(0, m_player2TotalConsiderationTimeMs) / 1000;
    int hours = totalSeconds / 3600;
    int minutes = (totalSeconds % 3600) / 60;
    int seconds = totalSeconds % 60;
    return QString::asprintf("%02d:%02d:%02d", hours, minutes, seconds);
}

// 対局者1の考慮時間と総考慮時間を "MM:SS/HH:MM:SS" 形式で取得する。
QString ShogiClock::getPlayer1ConsiderationAndTotalTime() const
{
    //begin
    qDebug() << "ShogiClock::getPlayer1ConsiderationAndTotalTime()";
    qDebug() << "m_player1ConsiderationTime: " << m_player1ConsiderationTimeMs;
    qDebug() << "m_player1TotalConsiderationTime: " << m_player1TotalConsiderationTimeMs;
    //end

    return getPlayer1ConsiderationTime() + "/" + getPlayer1TotalConsiderationTime();
}

// 対局者2の考慮時間と総考慮時間を "MM:SS/HH:MM:SS" 形式で取得する。
QString ShogiClock::getPlayer2ConsiderationAndTotalTime() const
{
    //begin
    qDebug() << "ShogiClock::getPlayer2ConsiderationAndTotalTime()";
    qDebug() << "m_player2ConsiderationTime: " << m_player2ConsiderationTimeMs;
    qDebug() << "m_player2TotalConsiderationTime: " << m_player2TotalConsiderationTimeMs;
    //end

    return getPlayer2ConsiderationTime() + "/" + getPlayer2TotalConsiderationTime();
}

// 現在の状態を時間履歴に保存する。
void ShogiClock::saveState()
{
    //begin
    qDebug() << "--------------ShogiClock::saveState()---------------";
    qDebug() << "m_player1Time: " << m_player1TimeMs;
    qDebug() << "m_player2Time: " << m_player2TimeMs;
    qDebug() << "m_player1ConsiderationTime: " << m_player1ConsiderationTimeMs;
    qDebug() << "m_player2ConsiderationTime: " << m_player2ConsiderationTimeMs;
    qDebug() << "m_player1TotalConsiderationTime: " << m_player1TotalConsiderationTimeMs;
    qDebug() << "m_player2TotalConsiderationTime: " << m_player2TotalConsiderationTimeMs;
    qDebug() << "----------------------------------------------------";
    //end

    // 対局者1の残り時間を保存する。
    m_player1TimeHistory.push(m_player1TimeMs);

    // 対局者2の残り時間を保存する。
    m_player2TimeHistory.push(m_player2TimeMs);

    // 対局者1の考慮時間を保存する。
    m_player1ConsiderationHistory.push(m_player1ConsiderationTimeMs);

    // 対局者2の考慮時間を保存する。
    m_player2ConsiderationHistory.push(m_player2ConsiderationTimeMs);

    // 対局者1の総考慮時間を保存する。
    m_player1TotalConsiderationHistory.push(m_player1TotalConsiderationTimeMs);

    // 対局者2の総考慮時間を保存する。
    m_player2TotalConsiderationHistory.push(m_player2TotalConsiderationTimeMs);
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
        m_player1TimeMs = m_player1TimeHistory.top();

        // 対局者1の残り時間を2手分戻す。
        m_player2TimeHistory.pop();
        m_player2TimeHistory.pop();
        m_player2TimeMs = m_player2TimeHistory.top();

        // 対局者1の考慮時間を2手分戻す。
        m_player1ConsiderationHistory.pop();
        m_player1ConsiderationHistory.pop();
        m_player1ConsiderationTimeMs = m_player1ConsiderationHistory.top();

        // 対局者2の考慮時間を2手分戻す。
        m_player2ConsiderationHistory.pop();
        m_player2ConsiderationHistory.pop();
        m_player2ConsiderationTimeMs = m_player2ConsiderationHistory.top();

        // 対局者1の総考慮時間を2手分戻す。
        m_player1TotalConsiderationHistory.pop();
        m_player1TotalConsiderationHistory.pop();
        m_player1TotalConsiderationTimeMs = m_player1TotalConsiderationHistory.top();

        // 対局者2の総考慮時間を2手分戻す。
        m_player2TotalConsiderationHistory.pop();
        m_player2TotalConsiderationHistory.pop();
        m_player2TotalConsiderationTimeMs = m_player2TotalConsiderationHistory.top();

        // 残り時間の更新を通知する。
        emit timeUpdated();
    }
}
