#ifndef SHOGICLOCK_H
#define SHOGICLOCK_H

#include <QObject>
#include <QTimer>
#include <QStack>
#include <QElapsedTimer>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcShogiClock)

// 将棋クロックにより対局者の持ち時間を管理するクラス
class ShogiClock : public QObject
{
    Q_OBJECT

public:
    // タイマーの刻み（ms）を一元管理
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
    void stopClock();
    void updateClock();  // QTimer から 50ms 毎に呼ばれる

    // 手が確定した瞬間に呼ぶ（その手を指した側だけ呼ぶ）
    void applyByoyomiAndResetConsideration1();
    void applyByoyomiAndResetConsideration2();

    // 「待った」
    void undo();

    // 文字列取得（HH:MM:SS / MM:SS）
    QString getPlayer1TimeString() const;
    QString getPlayer2TimeString() const;

    QString getPlayer1ConsiderationTime() const;
    QString getPlayer2ConsiderationTime() const;

    QString getPlayer1TotalConsiderationTime() const;
    QString getPlayer2TotalConsiderationTime() const;

    QString getPlayer1ConsiderationAndTotalTime() const;
    QString getPlayer2ConsiderationAndTotalTime() const;

    // 残り時間（ms）
    qint64 getPlayer1TimeIntMs() const;
    qint64 getPlayer2TimeIntMs() const;

    // 現在の手番（1=先手, 2=後手）
    int    currentPlayer() const { return m_currentPlayer; }

    // 考慮時間（ms）を直接セット（互換API）
    void setPlayer1ConsiderationTime(int newPlayer1ConsiderationTimeMs);
    void setPlayer2ConsiderationTime(int newPlayer2ConsiderationTimeMs);

signals:
    void timeUpdated();
    void player1TimeOut();
    void player2TimeOut();
    void resignationTriggered();

private:
    // 記録（履歴に積む）
    void saveState();

    // デバッグ用：不変条件を確認（Debug ビルドのみ有効）
    void debugCheckInvariants() const;

    // タイマ
    QTimer*       m_timer = nullptr;
    QElapsedTimer m_elapsedTimer;
    bool          m_clockRunning = false;
    qint64        m_lastTickMs   = 0;

    // 設定・状態
    bool   m_timeLimitSet  = false;
    bool   m_loseOnTimeout = true;
    int    m_currentPlayer = 1; // 1=先手,2=後手

    // 時間（すべて ms, qint64）
    qint64 m_player1TimeMs = 0;
    qint64 m_player2TimeMs = 0;

    qint64 m_byoyomi1TimeMs = 0;
    qint64 m_byoyomi2TimeMs = 0;

    qint64 m_bincMs = 0; // increment for player1
    qint64 m_wincMs = 0; // increment for player2

    // 考慮時間・総考慮時間
    qint64 m_player1ConsiderationTimeMs = 0;
    qint64 m_player2ConsiderationTimeMs = 0;
    qint64 m_player1TotalConsiderationTimeMs = 0;
    qint64 m_player2TotalConsiderationTimeMs = 0;

    // 秒表示キャッシュ（変化時のみ timeUpdated を出すため）
    qint64 m_prevShownSecP1 = -1;
    qint64 m_prevShownSecP2 = -1;

    // 秒読み適用中フラグ
    bool   m_byoyomi1Applied = false;
    bool   m_byoyomi2Applied = false;

    // 履歴（undo 用）
    QStack<qint64> m_player1TimeHistory;
    QStack<qint64> m_player2TimeHistory;
    QStack<qint64> m_player1ConsiderationHistory;
    QStack<qint64> m_player2ConsiderationHistory;
    QStack<qint64> m_player1TotalConsiderationHistory;
    QStack<qint64> m_player2TotalConsiderationHistory;
    QStack<bool>   m_byoyomi1AppliedHistory;
    QStack<bool>   m_byoyomi2AppliedHistory;
};

#endif // SHOGICLOCK_H
