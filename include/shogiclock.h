#ifndef SHOGICLOCK_H
#define SHOGICLOCK_H

#include <QObject>
#include <QTimer>
#include <QStack>
#include <QElapsedTimer>

// 将棋クロックにより対局者の持ち時間を管理するクラス
class ShogiClock : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    ShogiClock(QObject *parent = nullptr);

    // 両対局者の持ち時間を設定する。
    void setPlayerTimes(const int player1Seconds, const int player2Seconds, const int byoyomi1Seconds, const int byoyomi2Seconds,
                        const int binc, const int winc, const bool isLimitedTime);

    // 手番を設定する。
    void setCurrentPlayer(int player);

    // タイマーを開始する。
    void startClock();

    // タイマーを停止する。
    void stopClock();

    // 残り時間を更新する。
    void updateClock();

    // 対局者1の残り時間を取得する。
    QString getPlayer1TimeString() const;

    // 対局者2の残り時間を取得する。
    QString getPlayer2TimeString() const;

    // 対局者1の残り時間を取得する。
    int getPlayer1TimeIntMs() const;

    // 対局者2の残り時間を取得する。
    int getPlayer2TimeIntMs() const;

    // 対局者1の総考慮時間を取得する。
    QString getPlayer1TotalConsiderationTime() const;

    // 対局者2の総考慮時間を取得する。
    QString getPlayer2TotalConsiderationTime() const;

    // 対局者1の考慮時間と総考慮時間を "MM:SS/HH:MM:SS" 形式で取得する。
    QString getPlayer1ConsiderationAndTotalTime() const;

    // 対局者2の考慮時間と総考慮時間を "MM:SS/HH:MM:SS" 形式で取得する。
    QString getPlayer2ConsiderationAndTotalTime() const;

    // 「待った」をした場合、状態を2手前の残り時間、考慮時間、総考慮時間に戻す。
    void undo();

    void applyByoyomiAndResetConsideration1();

    void applyByoyomiAndResetConsideration2();

    // 考慮時間を総考慮時間に追加する。
    void addConsiderationTimeToTotal(int player);

    // 対局者1の残り時間を設定する。
    void setPlayer1ConsiderationTime(int newPlayer1ConsiderationTime);

    // 対局者2の残り時間を設定する。
    void setPlayer2ConsiderationTime(int newPlayer2ConsiderationTime);

    // 対局者1に秒読みが適用されているかを示すフラグを取得する。
    bool byoyomi1Applied() const;

    // 対局者2に秒読みが適用されているかを示すフラグを取得する。
    bool byoyomi2Applied() const;

    void setLoseOnTimeout(bool v);

    qint64 player1RemainingMs() const { return m_player1TimeMs; }
    qint64 player2RemainingMs() const { return m_player2TimeMs; }
    int    currentPlayer() const      { return m_currentPlayer; }

signals:
    // 残り時間が更新された場合に発生するシグナル
    void timeUpdated();

    // 対局者1の残り時間が0になった場合に発生するシグナル
    void player1TimeOut();

    // 対局者2の残り時間が0になった場合に発生するシグナル
    void player2TimeOut();

    // 残り時間が0になった場合に投了処理を行うシグナル
    void resignationTriggered();

private:
    // タイマー
    QTimer* m_timer;

    // tick精度向上用
    QElapsedTimer m_elapsedTimer;

    // 最後にタイマーが更新された時刻（ミリ秒単位）
    qint64 m_lastTickMs;

    // 追加: 「秒が変わったか」検出用
    qint64 m_prevShownSecP1 = -1;
    qint64 m_prevShownSecP2 = -1;

    // 対局者1の残り時間
    int m_player1TimeMs;

    // 対局者2の残り時間
    int m_player2TimeMs;

    // 対局者1の秒読み時間
    int m_byoyomi1TimeMs;

    // 対局者2の秒読み時間
    int m_byoyomi2TimeMs;

    // 対局者1の1手ごとの加算時間
    int m_bincMs;

    // 対局者2の1手ごとの加算時間
    int m_wincMs;

    // 秒読みが適用されているかを示すフラグ
    bool m_byoyomi1Applied;

    // 秒読みが適用されているかを示すフラグ
    bool m_byoyomi2Applied;

    // 現在の手番
    int m_currentPlayer;

    // タイマーが開始されているかどうかを示すフラグ
    bool m_clockRunning;

    // 持ち時間が指定されているかを示すフラグ
    bool m_timeLimitSet;

    // タイムアウト時に投了するかどうかを示すフラグ
    bool m_loseOnTimeout;

    // 対局者1の考慮時間
    int m_player1ConsiderationTimeMs;

    // 対局者2の考慮時間
    int m_player2ConsiderationTimeMs;

    // 対局者1の総考慮時間
    int m_player1TotalConsiderationTimeMs;

    // 対局者2の総考慮時間
    int m_player2TotalConsiderationTimeMs;

    // 対局者1の残り時間の履歴
    QStack<int> m_player1TimeHistory;

    // 対局者2の残り時間の履歴
    QStack<int> m_player2TimeHistory;

    // 対局者1の考慮時間の履歴
    QStack<int> m_player1ConsiderationHistory;

    // 対局者2の考慮時間の履歴
    QStack<int> m_player2ConsiderationHistory;

    // 対局者1の総考慮時間の履歴
    QStack<int> m_player1TotalConsiderationHistory;

    // 対局者2の総考慮時間の履歴
    QStack<int> m_player2TotalConsiderationHistory;

    // 現在の状態を時間履歴に保存する。
    void saveState();

    // 対局者1の考慮時間を取得する。
    QString getPlayer1ConsiderationTime() const;

    // 対局者2の考慮時間を取得する。
    QString getPlayer2ConsiderationTime() const;
};

#endif // SHOGICLOCK_H
