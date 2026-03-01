#ifndef REPLAYCONTROLLER_H
#define REPLAYCONTROLLER_H

/// @file replaycontroller.h
/// @brief 棋譜再生コントローラクラスの定義


#include <QObject>
#include <QPointer>

class ShogiClock;
class ShogiView;
class ShogiGameController;
class MatchCoordinator;
class RecordPane;

/**
 * @brief ReplayController - リプレイモードの管理クラス
 *
 * MainWindowからリプレイモード関連の処理を分離したクラス。
 * 以下の責務を担当:
 * - リプレイモードの状態管理
 * - ライブ追記モードの状態管理
 * - モード遷移時の各コンポーネントへの通知・同期
 */
class ReplayController : public QObject
{
    Q_OBJECT

public:
    explicit ReplayController(QObject* parent = nullptr);
    ~ReplayController() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------

    /**
     * @brief ShogiClockを設定（モード切替時の時計制御用）
     */
    void setClock(ShogiClock* clock);

    /**
     * @brief ShogiViewを設定（UIミュート制御用）
     */
    void setShogiView(ShogiView* view);

    /**
     * @brief ShogiGameControllerを設定（現在手番取得用）
     */
    void setGameController(ShogiGameController* gc);

    /**
     * @brief MatchCoordinatorを設定（時間更新通知用）
     */
    void setMatchCoordinator(MatchCoordinator* match);

    /**
     * @brief RecordPaneを設定（棋譜ビュー制御用）
     */
    void setRecordPane(RecordPane* pane);

    // --------------------------------------------------------
    // リプレイモード管理
    // --------------------------------------------------------

    /**
     * @brief リプレイモードを設定
     * @param on true=リプレイモードON, false=OFF
     *
     * リプレイモードON時:
     * - 時計を停止し表示を更新
     * - UIをミュート状態に
     * - 手番ハイライトをクリア
     *
     * リプレイモードOFF時:
     * - UIミュートを解除
     * - 現在手番のハイライトを復元
     * - 時計のUrgency表示を更新
     */
    void setReplayMode(bool on);

    /**
     * @brief 現在リプレイモードかどうか
     */
    bool isReplayMode() const;

    // --------------------------------------------------------
    // ライブ追記モード管理
    // --------------------------------------------------------

    /**
     * @brief ライブ追記モードを開始
     *
     * 対局進行中に棋譜が追加されるモード。
     * 棋譜ビューの選択モードをNoSelectionに設定。
     */
    void enterLiveAppendMode();

    /**
     * @brief ライブ追記モードを終了
     *
     * 棋譜ビューの選択モードをSingleSelectionに戻す。
     */
    void exitLiveAppendMode();

    /**
     * @brief 現在ライブ追記モードかどうか
     */
    bool isLiveAppendMode() const;

    // --------------------------------------------------------
    // 中断からの再開モード
    // --------------------------------------------------------

    /**
     * @brief 途中局面からの再開フラグを設定
     */
    void setResumeFromCurrent(bool on);

    /**
     * @brief 途中局面からの再開フラグを取得
     */
    bool isResumeFromCurrent() const;

private:
    ShogiClock* m_clock = nullptr;
    ShogiView* m_view = nullptr;
    ShogiGameController* m_gc = nullptr;
    QPointer<MatchCoordinator> m_match;       ///< 非所有（再生成追跡）
    RecordPane* m_recordPane = nullptr;

    bool m_isReplayMode = false;
    bool m_isLiveAppendMode = false;
    bool m_isResumeFromCurrent = false;
};

#endif // REPLAYCONTROLLER_H
