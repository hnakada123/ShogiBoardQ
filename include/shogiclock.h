#ifndef SHOGICLOCK_H
#define SHOGICLOCK_H

#include <QObject>
#include <QTimer>
#include <QStack>

// 将棋クロックにより対局者の持ち時間を管理するクラス
class ShogiClock : public QObject
{
    Q_OBJECT

public:
    // コンストラクタ
    ShogiClock(QObject *parent = nullptr);

    // 両対局者の最初の持ち時間を設定する。
    void setInitialTime(int seconds);

    // 両対局者の持ち時間を設定する。
    void setPlayerTimes(const int player1Seconds, const int player2Seconds, const int byoyomi1Seconds, const int byoyomi2Seconds, const int binc, const int winc);

    // 手番を設定する。
    void setCurrentPlayer(int player);

    // タイマーを開始する。
    void startClock();

    // タイマーを停止する。
    void stopClock();

    // 残り時間を更新する。
    void updateClock();

    // 対局者1の残り時間を取得する。
    QString getPlayer1Time() const;

    // 対局者2の残り時間を取得する。
    QString getPlayer2Time() const;

    // 対局者1の残り時間を取得する。
    int getPlayer1TimeInt() const;

    // 対局者2の残り時間を取得する。
    int getPlayer2TimeInt() const;

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

    // 対局者1に対して秒読み時間が適用された場合、その秒読み時間を持ち時間としてリセットし、
    // 考慮時間を総考慮時間に追加してリセットする。
    void applyByoyomiAndResetConsideration1(const bool useByoyomi);

    // 対局者2に対して秒読み時間が適用された場合、その秒読み時間を持ち時間としてリセットし、
    // 考慮時間を総考慮時間に追加してリセットする。
    void applyByoyomiAndResetConsideration2(const bool useByoyomi);

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

    // 対局者1の残り時間
    int m_player1Time;

    // 対局者2の残り時間
    int m_player2Time;

    // 対局者1の秒読み時間
    int m_byoyomi1Time;

    // 対局者2の秒読み時間
    int m_byoyomi2Time;

    // 対局者1の1手ごとの加算時間
    int m_binc;

    // 対局者2の1手ごとの加算時間
    int m_winc;

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

    // 対局者1の考慮時間
    int m_player1ConsiderationTime;

    // 対局者2の考慮時間
    int m_player2ConsiderationTime;

    // 対局者1の総考慮時間
    int m_player1TotalConsiderationTime;

    // 対局者2の総考慮時間
    int m_player2TotalConsiderationTime;

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
