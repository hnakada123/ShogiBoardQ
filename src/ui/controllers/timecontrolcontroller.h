#ifndef TIMECONTROLCONTROLLER_H
#define TIMECONTROLCONTROLLER_H

#include <QObject>
#include <QDateTime>
#include <QString>
#include "gamestartcoordinator.h"

class ShogiClock;
class MatchCoordinator;
class TimeDisplayPresenter;
class ShogiView;
class GameStartCoordinator;

/**
 * @brief TimeControlController - 時間制御の管理クラス
 *
 * MainWindowから時間制御関連の処理を分離したクラス。
 * 以下の責務を担当:
 * - ShogiClockの所有と初期化
 * - 時間制御設定値の保持（CSA出力等で使用）
 * - タイムアウトシグナルのハンドリング
 * - 残り時間・増加時間・秒読み時間の提供
 */
class TimeControlController : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 時間制御設定を保持する構造体
     */
    struct TimeControlSettings {
        bool    enabled = false;
        qint64  baseMs = 0;
        qint64  byoyomiMs = 0;
        qint64  incrementMs = 0;
    };

    explicit TimeControlController(QObject* parent = nullptr);
    ~TimeControlController() override;

    // --------------------------------------------------------
    // ShogiClockの管理
    // --------------------------------------------------------

    /**
     * @brief ShogiClockを確保・初期化（未作成なら作成）
     */
    void ensureClock();

    /**
     * @brief ShogiClockへのポインタを取得
     */
    ShogiClock* clock() const;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------

    /**
     * @brief MatchCoordinator（タイムアウト時の通知先）を設定
     */
    void setMatchCoordinator(MatchCoordinator* match);

    /**
     * @brief TimeDisplayPresenter（表示用）を設定
     */
    void setTimeDisplayPresenter(TimeDisplayPresenter* presenter);

    /**
     * @brief 時間設定を時計および司令塔へ適用するヘルパー
     * @param tc        GameStartCoordinatorから渡される時間設定
     * @param match     対局司令塔（null可）
     * @param startSfen 開始局面SFEN（平手/手合割/現在局面など）
     * @param currentSfen 現在のSFEN（「現在の局面から」開始時に使用）
     * @param shogiView UI再描画対象（nullの場合はスキップ）
     */
    void applyTimeControl(const GameStartCoordinator::TimeControl& tc,
                          MatchCoordinator* match,
                          const QString& startSfen,
                          const QString& currentSfen,
                          ShogiView* shogiView = nullptr);

    // --------------------------------------------------------
    // 時間制御設定
    // --------------------------------------------------------

    /**
     * @brief 時間制御設定を保存
     */
    void saveTimeControlSettings(bool enabled, qint64 baseMs, qint64 byoyomiMs, qint64 incrementMs);

    /**
     * @brief 時間制御設定を取得
     */
    const TimeControlSettings& settings() const;

    /**
     * @brief 時間制御が有効かどうか
     */
    bool hasTimeControl() const;

    /**
     * @brief 基本持ち時間を取得（ミリ秒）
     */
    qint64 baseTimeMs() const;

    /**
     * @brief 秒読み時間を取得（ミリ秒）
     */
    qint64 byoyomiMs() const;

    /**
     * @brief 加算時間を取得（ミリ秒）
     */
    qint64 incrementMs() const;

    // --------------------------------------------------------
    // 対局開始時刻
    // --------------------------------------------------------

    /**
     * @brief 対局開始時刻を記録
     */
    void recordGameStartTime();

    /**
     * @brief 対局開始時刻を取得
     */
    QDateTime gameStartDateTime() const;

    /**
     * @brief 対局開始時刻をクリア
     */
    void clearGameStartTime();

    // --------------------------------------------------------
    // 対局終了時刻
    // --------------------------------------------------------

    /**
     * @brief 対局終了時刻を記録
     */
    void recordGameEndTime();

    /**
     * @brief 対局終了時刻を取得
     */
    QDateTime gameEndDateTime() const;

    /**
     * @brief 対局終了時刻をクリア
     */
    void clearGameEndTime();

    // --------------------------------------------------------
    // 時間取得ヘルパー（MatchCoordinator hooks用）
    // --------------------------------------------------------

    /**
     * @brief 指定プレイヤーの残り時間を取得（ミリ秒）
     * @param player 1=先手, 2=後手
     */
    qint64 getRemainingMs(int player) const;

    /**
     * @brief 指定プレイヤーの加算時間を取得（ミリ秒）
     * @param player 1=先手, 2=後手
     */
    qint64 getIncrementMs(int player) const;

    /**
     * @brief 秒読み時間を取得（ミリ秒）
     */
    qint64 getByoyomiMs() const;

Q_SIGNALS:
    /**
     * @brief プレイヤー1（先手）がタイムアウトした
     */
    void player1TimedOut();

    /**
     * @brief プレイヤー2（後手）がタイムアウトした
     */
    void player2TimedOut();

private Q_SLOTS:
    /**
     * @brief ShogiClockからのタイムアウト通知（先手）
     */
    void onClockPlayer1TimeOut();

    /**
     * @brief ShogiClockからのタイムアウト通知（後手）
     */
    void onClockPlayer2TimeOut();

private:
    ShogiClock* m_clock = nullptr;
    MatchCoordinator* m_match = nullptr;
    TimeDisplayPresenter* m_timePresenter = nullptr;

    TimeControlSettings m_settings;
    QDateTime m_gameStartDateTime;
    QDateTime m_gameEndDateTime;
};

#endif // TIMECONTROLCONTROLLER_H
