#ifndef CONSECUTIVEGAMESCONTROLLER_H
#define CONSECUTIVEGAMESCONTROLLER_H

#include <QObject>
#include <QTimer>
#include "matchcoordinator.h"
#include "gamestartcoordinator.h"

class ShogiClock;
class TimeControlController;

/**
 * @brief 連続対局の管理を行うコントローラ
 *
 * MainWindowから分離された責務:
 * - 連続対局の設定保持
 * - 次の対局への自動遷移
 * - 手番入れ替えの管理
 */
class ConsecutiveGamesController : public QObject
{
    Q_OBJECT

public:
    explicit ConsecutiveGamesController(QObject* parent = nullptr);

    /**
     * @brief 依存オブジェクトを設定
     */
    void setTimeController(TimeControlController* tc);
    void setGameStartCoordinator(GameStartCoordinator* gsc);

    /**
     * @brief 連続対局の設定を受信
     * @param totalGames 合計対局数
     * @param switchTurn 手番を入れ替えるかどうか
     */
    void configure(int totalGames, bool switchTurn);

    /**
     * @brief 対局開始時の設定を保存
     * @param opt 対局開始オプション
     * @param tc 時間制御設定
     */
    void onGameStarted(const MatchCoordinator::StartOptions& opt,
                        const GameStartCoordinator::TimeControl& tc);

    /**
     * @brief 対局終了時に次の対局を開始するかチェック
     * @return 次の対局を開始する場合true
     */
    bool shouldStartNextGame() const;

    /**
     * @brief 残り対局数を取得
     */
    int remainingGames() const { return m_remainingGames; }

    /**
     * @brief 現在の対局番号を取得（1始まり）
     */
    int currentGameNumber() const { return m_gameNumber; }

    /**
     * @brief 合計対局数を取得
     */
    int totalGames() const { return m_totalGames; }

    /**
     * @brief 連続対局をリセット
     */
    void reset();

signals:
    /**
     * @brief 次の対局を開始するリクエスト
     */
    void requestStartNextGame(const MatchCoordinator::StartOptions& opt,
                               const GameStartCoordinator::TimeControl& tc);

    /**
     * @brief 前準備クリーンアップを要求
     */
    void requestPreStartCleanup();

public slots:
    /**
     * @brief 次の連続対局を開始
     */
    void startNextGame();

private:
    void prepareNextGameOptions();

    int m_remainingGames = 0;
    int m_totalGames = 1;
    int m_gameNumber = 1;
    bool m_switchTurnEachGame = false;

    MatchCoordinator::StartOptions m_lastStartOptions;
    GameStartCoordinator::TimeControl m_lastTimeControl;

    TimeControlController* m_timeController = nullptr;
    GameStartCoordinator* m_gameStart = nullptr;

    QTimer* m_delayTimer = nullptr;
};

#endif // CONSECUTIVEGAMESCONTROLLER_H
