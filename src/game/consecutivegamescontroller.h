#ifndef CONSECUTIVEGAMESCONTROLLER_H
#define CONSECUTIVEGAMESCONTROLLER_H

/// @file consecutivegamescontroller.h
/// @brief 連続対局の進行管理と手番入れ替えを制御するコントローラ
/// @todo remove コメントスタイルガイド適用済み


#include <QObject>
#include <QTimer>
#include "matchcoordinator.h"
#include "gamestartcoordinator.h"

class ShogiClock;
class TimeControlController;

/**
 * @brief 連続対局の進行管理を行うコントローラ
 * @todo remove コメントスタイルガイド適用済み
 *
 * 連続対局の設定保持、次の対局への自動遷移、手番入れ替えを担当する。
 */
class ConsecutiveGamesController : public QObject
{
    Q_OBJECT

public:
    explicit ConsecutiveGamesController(QObject* parent = nullptr);

    // --- 依存オブジェクトの設定 ---
    void setTimeController(TimeControlController* tc);
    void setGameStartCoordinator(GameStartCoordinator* gsc);

    /**
     * @brief 連続対局の設定を受け取る
     * @param totalGames 合計対局数
     * @param switchTurn 1局ごとに手番を入れ替えるか
     */
    void configure(int totalGames, bool switchTurn);

    /**
     * @brief 対局開始時のオプションを保存する
     * @param opt 対局開始オプション
     * @param tc 時間制御設定
     */
    void onGameStarted(const MatchCoordinator::StartOptions& opt,
                        const GameStartCoordinator::TimeControl& tc);

    /// @return 次の対局を開始すべきなら true
    bool shouldStartNextGame() const;

    int remainingGames() const { return m_remainingGames; } ///< 残り対局数
    int currentGameNumber() const { return m_gameNumber; }  ///< 現在の対局番号（1始まり）
    int totalGames() const { return m_totalGames; }         ///< 合計対局数

    void reset();

signals:
    /// 次の対局開始を要求（→ MainWindow 側で処理）
    void requestStartNextGame(const MatchCoordinator::StartOptions& opt,
                               const GameStartCoordinator::TimeControl& tc);

    /// 前準備クリーンアップを要求（→ PrestartCleanupHandler::cleanup）
    void requestPreStartCleanup();

public slots:
    void startNextGame();

private:
    void prepareNextGameOptions();

    int m_remainingGames = 0;        ///< 残り対局数
    int m_totalGames = 1;            ///< 合計対局数
    int m_gameNumber = 1;            ///< 現在の対局番号（1始まり）
    bool m_switchTurnEachGame = false; ///< 1局ごとに手番を入れ替えるか

    MatchCoordinator::StartOptions m_lastStartOptions;     ///< 直前の対局開始オプション
    GameStartCoordinator::TimeControl m_lastTimeControl;   ///< 直前の時間制御設定

    TimeControlController* m_timeController = nullptr; ///< 非所有
    GameStartCoordinator* m_gameStart = nullptr;       ///< 非所有

    QTimer* m_delayTimer = nullptr; ///< 次局開始遅延用タイマー（所有: this）
};

#endif // CONSECUTIVEGAMESCONTROLLER_H
