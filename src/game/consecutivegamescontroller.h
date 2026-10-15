#ifndef CONSECUTIVEGAMESCONTROLLER_H
#define CONSECUTIVEGAMESCONTROLLER_H

/// @file consecutivegamescontroller.h
/// @brief エンジン同士の連続対局（局数・手番の入れ替え・次局の準備）

#include <QObject>
#include <QTimer>

#include "gamestartcoordinator.h"

/**
 * @brief エンジン同士の連続対局の局数と、次局の開始の設定を受け持つ
 *
 * 対局が終わるたびに残りの局数を数え、次局の設定（手番を入れ替えるならエンジンと持ち時間も入れ替える）を
 * 作って nextGameReady で知らせる。次局を始めるのは受け手（GameSession）。
 */
class ConsecutiveGamesController : public QObject
{
    Q_OBJECT

public:
    explicit ConsecutiveGamesController(QObject* parent = nullptr);

    /// 連続対局の局数と、1局ごとに手番を入れ替えるか（1局目の開始時）
    void configure(int totalGames, bool switchTurn);
    /// 対局の時間の決まり（次局にも使う）
    void setTimeSettings(const GameStartCoordinator::TimeSettings& tc) { m_lastTimeControl = tc; }
    /// 対局が始まった（次局はこの設定から作る）
    void onGameStarted(const MatchStartOptions& opt) { m_lastStartOptions = opt; }

    bool shouldStartNextGame() const { return m_remainingGames > 0; }
    int currentGameNumber() const { return m_gameNumber; }  ///< 現在の対局番号（1始まり）
    int totalGames() const { return m_totalGames; }         ///< 合計対局数
    void reset();

public slots:
    /// 次局を準備する（いまの終局処理を抜けてから nextGameReady を出す）
    void startNextGame();

signals:
    /// 次局の開始の設定ができた
    void nextGameReady(const GameStartCoordinator::StartParams& params);

private slots:
    void launchPreparedNextGame();

private:
    int m_remainingGames = 0;          ///< 残り対局数
    int m_totalGames = 1;              ///< 合計対局数
    int m_gameNumber = 1;              ///< 現在の対局番号（1始まり）
    bool m_switchTurnEachGame = false; ///< 1局ごとに手番を入れ替えるか
    MatchStartOptions m_lastStartOptions;                  ///< 直前の対局開始オプション
    GameStartCoordinator::TimeSettings m_lastTimeControl;  ///< 直前の時間の決まり
    QTimer m_delayTimer;               ///< 次局開始の遅延
};

#endif // CONSECUTIVEGAMESCONTROLLER_H
