#ifndef SHOGICLOCK_H
#define SHOGICLOCK_H

/// @file shogiclock.h
/// @brief 将棋対局用タイマー管理クラスの定義

#include <QElapsedTimer>
#include <QList>
#include <QObject>
#include <QTimer>

#include "timecontrol.h"

/**
 * @brief 将棋対局の持ち時間・秒読み・加算・考慮時間を管理するクラス
 *
 * 先手（1）と後手（2）の状態を SideState にまとめ、ミリ秒精度で追跡する。
 * 着手を確定する（commitMove）たびに両側の状態を履歴へ積み、「待った」（undo）は
 * 履歴の末尾を取り除いて戻す。時間切れの判定は、loseOnTimeout が有効なときだけ行う。
 */
class ShogiClock : public QObject
{
    Q_OBJECT
public:
    static constexpr int kTickMs = 50;  ///< タイマー刻み間隔（ms）

    explicit ShogiClock(QObject *parent = nullptr);

    /// 片側の時計の状態
    struct SideState {
        qint64 remainingMs = 0;           ///< 残り時間（秒読みに入った後は秒読みの残り）
        qint64 considerationMs = 0;       ///< 指している（または直前に指した）手の考慮時間
        qint64 totalConsiderationMs = 0;  ///< 確定した手の考慮時間の合計
        bool byoyomiApplied = false;      ///< 秒読みに入っているか
        int lastMoveShownSec = 0;         ///< 棋譜欄に出す直近の手の考慮秒数
        int prevShownTotalSec = 0;        ///< 直近の手の秒数を求めるための、前回の総考慮秒数
    };

    /// 着手を確定した時点の両側の状態（「待った」で戻す単位）
    struct HistoryEntry {
        SideState side[2];
    };

    /// 中断した対局の時計。決まり・各側の状態・待った用の履歴を保持する
    struct Snapshot {
        TimeControl timeControl;
        bool timeLimitSet = false;
        bool loseOnTimeout = true;
        bool considerationCommitted = false;
        int currentPlayer = 1;
        SideState side[2];
        int prevShownSec[2] = {-1, -1};
        QList<HistoryEntry> history;

        const SideState& of(int player) const { return side[player == 2 ? 1 : 0]; }
        SideState& of(int player) { return side[player == 2 ? 1 : 0]; }
    };
    Snapshot pauseAndSnapshot();
    void restoreSnapshot(const Snapshot& state);
    /// 再開前の消費時間を含めて、外部計測による一手の消費時間を確定する。
    void setMeasuredConsiderationTime(int player, qint64 elapsedMs);

    // --- 設定 ---

    void setLoseOnTimeout(bool v);

    /**
     * @brief 持ち時間の決まりを設定し、時計を対局開始の状態にする
     *
     * 秒読みと加算が両方あるときは秒読みを使う。初手にも加算を与えるので、残り時間は
     * 持ち時間＋加算で始まる。持ち時間がなく秒読みだけの側は、初めから秒読みに入る。
     * 決まりに制限がなければ、消費時間だけを数える時計になる。「待った」の履歴は空から積み直す。
     */
    void setTimeControl(const TimeControl& timeControl);
    const TimeControl& timeControl() const { return m_timeControl; }

    /**
     * @brief 残り時間を外部の値に合わせる（CSA のサーバーが示す残り時間など）
     *
     * 決まり・考慮時間・履歴はそのまま。残りが0以下で秒読みがある側は秒読みに入る。
     */
    void setRemainingTimes(qint64 blackMs, qint64 whiteMs);

    void setCurrentPlayer(int player);

    // --- クロック制御 ---

    void startClock();
    void stopClock();
    void updateClock();
    /// 応答受理時に精算し、次の手番への切替まで課金を止める。
    void finishTurn();
    /// 秒読みを除いた残り時間（未精算の経過時間を含めて計算）
    qint64 remainingMainTimeMs(int player) const;
    /// 秒読みを含めた、今の手に使える残り時間
    qint64 remainingTurnTimeMs(int player) const;
    bool isUnlimited() const { return !m_timeLimitSet; }
    bool enforcesTimeout() const { return m_timeLimitSet && m_loseOnTimeout; }

    // --- 着手確定時処理 ---

    /// 着手を確定する。考慮時間を合計へ加え、秒読みを戻すか加算を足して、状態を履歴へ積む
    void commitMove(int player);

    /// 2手分の状態を巻き戻す（「待った」用）。取り消す自分の手を指し始めたときの
    /// 残り時間・消費時間に戻し、考慮時間は呼び出した時点から数え直す。
    /// 履歴は setTimeControl() の開始状態と、着手確定ごとの状態で、戻せないときは何もしない。
    void undo();

    // --- GUI表示API ---

    /// 時計表示（HH:MM:SS）。制限ありは残り時間を切り上げ、無制限は累積消費時間を切り捨て。
    QString timeString(int player) const;
    /// 直近の手の考慮時間（MM:SS）
    QString considerationTimeString(int player) const;
    /// 総考慮時間（HH:MM:SS、実測msを秒に切り捨て）
    QString totalConsiderationTimeString(int player) const;
    /// 棋譜欄用の考慮/総考慮文字列（"MM:SS/HH:MM:SS"）
    QString considerationAndTotalTime(int player) const;

    // --- 状態取得 ---

    qint64 remainingMs(int player) const { return side(player).remainingMs; }  ///< 残り時間(ms)
    int currentPlayer() const { return m_currentPlayer; }                        ///< 手番（1=先手, 2=後手）

    /// 考慮時間を外部から直接設定する（エンジンの手の実測値の反映など）
    void setConsiderationTimeMs(int player, qint64 ms);
    qint64 considerationMs(int player) const { return side(player).considerationMs; }

    qint64 byoyomiMs(int player) const { return rules(player).byoyomiMs; }
    qint64 incrementMs(int player) const { return rules(player).incrementMs; }
    bool hasByoyomi(int player) const { return byoyomiMs(player) > 0; }
    bool byoyomiApplied(int player) const { return side(player).byoyomiApplied; }

    bool isGameOver() const { return m_gameOver; }                ///< 終局状態か
    void markGameOver() { m_gameOver = true; }                    ///< 終局状態に設定する
    bool isRunning() const { return m_clockRunning; }             ///< タイマー動作中か

signals:
    /// 時間表示の更新通知（→ TimeDisplayPresenter）
    void timeUpdated();

    /// 先手の時間切れ通知（→ MatchCoordinator）
    void player1TimeOut();

    /// 後手の時間切れ通知（→ MatchCoordinator）
    void player2TimeOut();

    /// 時間切れによる投了トリガー（→ MatchCoordinator）
    void resignationTriggered();

private:
    static int index(int player) { return player == 2 ? 1 : 0; }
    SideState& side(int player) { return m_side[index(player)]; }
    const SideState& side(int player) const { return m_side[index(player)]; }
    const TimeControlSide& rules(int player) const
    {
        return player == 2 ? m_timeControl.white : m_timeControl.black;
    }

    void pushHistory();
    void enterByoyomiIfExhausted(int player);
    void checkInvariants() const;
    int remainingDisplaySec(int player) const;
    void updateShownConsideration(int player);
    void resetDisplayCache();
    /// 手番側の経過時間を残り時間へ反映し、時間切れなら true
    bool chargeElapsed(int player, qint64 elapsed);
    void declareTimeout(int player);

    // --- タイマー ---
    QTimer*       m_timer = nullptr;           ///< 定期更新タイマー（所有、this親）
    QElapsedTimer m_elapsedTimer;              ///< 経過時間計測用（モノトニック）
    bool          m_clockRunning = false;      ///< タイマー動作中フラグ
    bool          m_turnFinished = false;      ///< 応答受理後、手番切替まで課金を止める
    qint64        m_lastTickMs   = 0;          ///< 前回tickの時刻(ms)

    // --- 決まりと状態 ---
    TimeControl m_timeControl;                 ///< 持ち時間の決まり
    bool m_timeLimitSet  = false;              ///< 持ち時間制限が有効か
    bool m_loseOnTimeout = true;               ///< 時間切れで負けとするか
    int  m_currentPlayer = 1;                  ///< 現在の手番（1=先手, 2=後手）
    SideState m_side[2];                       ///< 先手・後手の状態
    bool m_currentConsiderationCommitted = false; ///< 手番側の考慮時間を合計へ加え済みか
    int  m_prevShownSec[2] = {-1, -1};         ///< 前回通知した残り秒（表示の変化検出用）

    QList<HistoryEntry> m_history;             ///< 着手確定ごとの状態（「待った」用）

    qint64 m_resumedConsiderationMs[2] = {0, 0}; ///< 再開前に消費していた考慮時間
    bool m_resumingTurn = false;
    bool m_gameOver = false;                   ///< 終局フラグ
};

#endif // SHOGICLOCK_H
