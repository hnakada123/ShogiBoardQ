#ifndef SHOGICLOCK_H
#define SHOGICLOCK_H

#include <QObject>
#include <QTimer>
#include <QStack>
#include <QElapsedTimer>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcShogiClock)

// 将棋クロック：持ち時間/秒読み/インクリメント/考慮時間の管理
class ShogiClock : public QObject
{
    Q_OBJECT
public:
    static constexpr int kTickMs = 50;

    explicit ShogiClock(QObject *parent = nullptr);

    // 設定
    void setLoseOnTimeout(bool v);
    void setPlayerTimes(int player1Seconds, int player2Seconds,
                        int byoyomi1Seconds, int byoyomi2Seconds,
                        int bincSeconds, int wincSeconds,
                        bool isLimitedTime);
    void setCurrentPlayer(int player);

    // クロック制御
    void startClock();
    void stopClock();          // 進行中の手番側の残時間/考慮msに経過分を反映し停止
    void updateClock();        // QTimer から kTickMs ごとに呼ばれる

    // 着手確定時（その手を指した側だけ呼ぶ）
    void applyByoyomiAndResetConsideration1();  // 先手が指した直後
    void applyByoyomiAndResetConsideration2();  // 後手が指した直後

    // 「待った」
    void undo();

    // ---- GUI表示API（仕様固定） ----
    // 残り時間（実残りms→秒は切り上げ）
    QString getPlayer1TimeString() const;
    QString getPlayer2TimeString() const;

    // 直近考慮（MM:SS）＝ 総考慮(秒)の差分
    QString getPlayer1ConsiderationTime() const;
    QString getPlayer2ConsiderationTime() const;

    // 総考慮（HH:MM:SS）＝ 実測総考慮ms → 秒は切り捨て
    QString getPlayer1TotalConsiderationTime() const;
    QString getPlayer2TotalConsiderationTime() const;

    // 棋譜欄用 "MM:SS/HH:MM:SS"
    QString getPlayer1ConsiderationAndTotalTime() const;
    QString getPlayer2ConsiderationAndTotalTime() const;

    // 残り時間（ms）
    qint64 getPlayer1TimeIntMs() const { return m_player1TimeMs; }
    qint64 getPlayer2TimeIntMs() const { return m_player2TimeMs; }

    // 現在の手番（1=先手, 2=後手）
    int currentPlayer() const { return m_currentPlayer; }

    // 考慮時間（ms）を外部から直接セット（HvE でエンジン手のelapsedを反映など）
    void setPlayer1ConsiderationTime(int ms);
    void setPlayer2ConsiderationTime(int ms);

    // byoyomi が設定されているか／適用中か
    bool hasByoyomi1() const { return m_byoyomi1TimeMs > 0; }
    bool hasByoyomi2() const { return m_byoyomi2TimeMs > 0; }
    bool byoyomi1Applied() const { return m_byoyomi1Applied; }
    bool byoyomi2Applied() const { return m_byoyomi2Applied; }

    // 終局管理
    bool isGameOver() const { return m_gameOver; }
    void markGameOver() { m_gameOver = true; }

    // デバッグ/ログ用
    qint64 player1ConsiderationMs() const { return m_player1ConsiderationTimeMs; }
    qint64 player2ConsiderationMs() const { return m_player2ConsiderationTimeMs; }
    int getPlayer1ConsiderationTimeMs() const;
    int getPlayer2ConsiderationTimeMs() const;

signals:
    void timeUpdated();
    void player1TimeOut();
    void player2TimeOut();
    void resignationTriggered();

private:
    // ---- 内部ヘルパ ----
    void saveState();                // undo 用に状態を積む（startClockで呼ぶ）
    void debugCheckInvariants() const;

    // 残り秒（切り上げ）計算
    int remainingDisplaySecP1() const;
    int remainingDisplaySecP2() const;

    // 直近考慮(秒)を更新（着手確定時）※総考慮ms→秒切り捨ての差分
    void updateShownConsiderationForPlayer(int player);

    // タイマ
    QTimer*       m_timer = nullptr;
    QElapsedTimer m_elapsedTimer;
    bool          m_clockRunning = false;
    qint64        m_lastTickMs   = 0;

    // 設定・状態
    bool   m_timeLimitSet  = false;
    bool   m_loseOnTimeout = true;
    int    m_currentPlayer = 1;   // 1=先手, 2=後手

    // 残り時間（ms）
    qint64 m_player1TimeMs = 0;
    qint64 m_player2TimeMs = 0;

    // 秒読み/インクリメント（ms）
    qint64 m_byoyomi1TimeMs = 0;
    qint64 m_byoyomi2TimeMs = 0;
    qint64 m_bincMs = 0;  // 先手の increment
    qint64 m_wincMs = 0;  // 後手の increment

    // 考慮時間（直近手の実測ms）と総計（実測msの合計）
    qint64 m_player1ConsiderationTimeMs = 0;
    qint64 m_player2ConsiderationTimeMs = 0;
    qint64 m_player1TotalConsiderationTimeMs = 0;
    qint64 m_player2TotalConsiderationTimeMs = 0;

    // GUI表示用：直近考慮(秒) と「前回指した時点の総考慮(秒)」
    int m_p1LastMoveShownSec = 0;
    int m_p2LastMoveShownSec = 0;
    int m_p1PrevShownTotalSec = 0;
    int m_p2PrevShownTotalSec = 0;

    // 秒表示キャッシュ（残り時間の秒が変わったときだけ通知）
    int m_prevShownSecP1 = -1;
    int m_prevShownSecP2 = -1;

    // 秒読みの適用状態
    bool m_byoyomi1Applied = false;
    bool m_byoyomi2Applied = false;

    // undo 用の履歴
    QStack<qint64> m_player1TimeHistory;
    QStack<qint64> m_player2TimeHistory;
    QStack<qint64> m_player1ConsiderationHistory;
    QStack<qint64> m_player2ConsiderationHistory;
    QStack<qint64> m_player1TotalConsiderationHistory;
    QStack<qint64> m_player2TotalConsiderationHistory;
    QStack<bool>   m_byoyomi1AppliedHistory;
    QStack<bool>   m_byoyomi2AppliedHistory;
    QStack<int>    m_p1LastMoveShownSecHistory;
    QStack<int>    m_p2LastMoveShownSecHistory;
    QStack<int>    m_p1PrevShownTotalSecHistory;
    QStack<int>    m_p2PrevShownTotalSecHistory;

    bool m_gameOver = false;  // 終局フラグ（時間切れ/投了後など）
};

#endif // SHOGICLOCK_H
