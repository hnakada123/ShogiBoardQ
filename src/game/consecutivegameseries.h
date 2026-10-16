#ifndef CONSECUTIVEGAMESERIES_H
#define CONSECUTIVEGAMESERIES_H

/// @file consecutivegameseries.h
/// @brief エンジン同士の連続対局（局数・手番の入れ替え・次局の計画）

#include <QObject>
#include <QTimer>

#include "gamestartoptionsbuilder.h"

/**
 * @brief エンジン同士の連続対局の局数と、次局の開始の計画を受け持つ
 *
 * 対局が終わるたびに残りの局数を数え、次局の計画（手番を入れ替えるならエンジンと持ち時間も入れ替える）を
 * 作って nextGameReady で知らせる。次局を始めるのは受け手（GameSession）。
 */
class ConsecutiveGameSeries : public QObject
{
    Q_OBJECT

public:
    explicit ConsecutiveGameSeries(QObject* parent = nullptr);

    /// 連続対局の1局目を始めた（局数・手番の入れ替えと、次局の元にする計画を覚える）
    void beginSeries(const GameStartPlan& firstGame);
    bool shouldStartNextGame() const { return m_remainingGames > 0; }
    int currentGameNumber() const { return m_gameNumber; }  ///< 現在の対局番号（1始まり）
    int totalGames() const { return m_totalGames; }         ///< 合計対局数
    void reset();

public slots:
    /// 次局を準備する（いまの終局処理を抜けてから nextGameReady を出す）
    void startNextGame();

signals:
    /// 次局の開始の計画ができた
    void nextGameReady(const GameStartPlan& plan);

private slots:
    void launchPreparedNextGame();

private:
    int m_remainingGames = 0;          ///< 残り対局数
    int m_totalGames = 1;              ///< 合計対局数
    int m_gameNumber = 1;              ///< 現在の対局番号（1始まり）
    GameStartPlan m_nextGame;          ///< 次局の元にする計画
    QTimer m_delayTimer;               ///< 次局開始の遅延
};

#endif // CONSECUTIVEGAMESERIES_H
