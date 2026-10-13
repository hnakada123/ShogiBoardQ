#ifndef TIMECONTROLCONTROLLER_H
#define TIMECONTROLCONTROLLER_H

/// @file timecontrolcontroller.h
/// @brief 持ち時間制御コントローラクラスの定義


#include <QObject>
#include <QDateTime>
#include <QString>
#include "gamestartcoordinator.h"
#include "kifuexportmetadata.h"

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
     * @param startSfen 開始局面SFEN（平手/手合割/現在局面など）
     * @param currentSfen 現在のSFEN（「現在の局面から」開始時に使用）
     * @param shogiView UI再描画対象（nullの場合はスキップ）
     */
    void applyTimeControl(const GameStartCoordinator::TimeSettings& tc,
                          const QString& startSfen,
                          const QString& currentSfen,
                          ShogiView* shogiView = nullptr);

    // --------------------------------------------------------
    // 時間制御設定
    // --------------------------------------------------------

    /**
     * @brief 時間制御設定を保存
     */
    void recordTimeControl(const TimeControl& timeControl);

    /**
     * @brief 時計を使わずに始める対局（CSA通信対局など）の持ち時間と開始日時を記録する
     *
     * 持ち時間・秒読み・加算がすべて0なら時間制限なしとして扱う。
     */
    void beginGameWithTimeControl(const TimeControl& timeControl);

    /**
     * @brief 時間制御設定を取得
     */
    const TimeControl& timeControl() const;

    /**
     * @brief 時間制御が有効かどうか
     */
    bool hasTimeControl() const;

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

private slots:
    /**
     * @brief ShogiClockからのタイムアウト通知（player: 時間が切れた側）
     */
    void onClockPlayerTimeOut(Turn player);

private:
    ShogiClock* m_clock = nullptr;
    MatchCoordinator* m_match = nullptr;
    TimeDisplayPresenter* m_timePresenter = nullptr;

    TimeControl m_timeControl;  ///< 対局情報と棋譜に記録する持ち時間
    QDateTime m_gameStartDateTime;
    QDateTime m_gameEndDateTime;
};

#endif // TIMECONTROLCONTROLLER_H
