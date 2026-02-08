#ifndef SHOGICLOCK_H
#define SHOGICLOCK_H

/// @file shogiclock.h
/// @brief 将棋対局用タイマー管理クラスの定義

#include <QObject>
#include <QTimer>
#include <QStack>
#include <QElapsedTimer>
#include <QLoggingCategory>

Q_DECLARE_LOGGING_CATEGORY(lcShogiClock)

/**
 * @brief 将棋対局の持ち時間・秒読み・インクリメント・考慮時間を管理するクラス
 *
 * 先手/後手それぞれの残り時間をミリ秒精度で追跡し、
 * 秒読み・フィッシャー加算の自動適用、時間切れ判定、
 * 「待った」による状態巻き戻しを提供する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class ShogiClock : public QObject
{
    Q_OBJECT
public:
    static constexpr int kTickMs = 50;  ///< タイマー刻み間隔（ms）

    explicit ShogiClock(QObject *parent = nullptr);

    // --- 設定 ---

    void setLoseOnTimeout(bool v);

    /**
     * @brief 両対局者の持ち時間・秒読み・加算を一括設定する
     * @param player1Seconds 先手の持ち時間（秒）
     * @param player2Seconds 後手の持ち時間（秒）
     * @param byoyomi1Seconds 先手の秒読み（秒、0で無効）
     * @param byoyomi2Seconds 後手の秒読み（秒、0で無効）
     * @param bincSeconds 先手のインクリメント（秒、0で無効）
     * @param wincSeconds 後手のインクリメント（秒、0で無効）
     * @param isLimitedTime 持ち時間制限ありの場合true
     *
     * 秒読みとインクリメントは排他: どちらかが正の場合、他方は0になる。
     */
    void setPlayerTimes(int player1Seconds, int player2Seconds,
                        int byoyomi1Seconds, int byoyomi2Seconds,
                        int bincSeconds, int wincSeconds,
                        bool isLimitedTime);
    void setCurrentPlayer(int player);

    // --- クロック制御 ---

    void startClock();
    void stopClock();
    void updateClock();

    // --- 着手確定時処理 ---

    /// 先手の着手確定後に秒読み/加算を適用し、考慮時間を確定する
    void applyByoyomiAndResetConsideration1();

    /// 後手の着手確定後に秒読み/加算を適用し、考慮時間を確定する
    void applyByoyomiAndResetConsideration2();

    /// 2手分の状態を巻き戻す（「待った」用）
    void undo();

    // --- GUI表示API ---

    /// 残り時間文字列（HH:MM:SS、残りmsを秒に切り上げ）
    QString getPlayer1TimeString() const;
    QString getPlayer2TimeString() const;

    /// 直近考慮時間文字列（MM:SS）
    QString getPlayer1ConsiderationTime() const;
    QString getPlayer2ConsiderationTime() const;

    /// 総考慮時間文字列（HH:MM:SS、実測msを秒に切り捨て）
    QString getPlayer1TotalConsiderationTime() const;
    QString getPlayer2TotalConsiderationTime() const;

    /// 棋譜欄用の考慮/総考慮文字列（"MM:SS/HH:MM:SS"）
    QString getPlayer1ConsiderationAndTotalTime() const;
    QString getPlayer2ConsiderationAndTotalTime() const;

    // --- 状態取得 ---

    qint64 getPlayer1TimeIntMs() const { return m_player1TimeMs; }  ///< 先手残り時間(ms)
    qint64 getPlayer2TimeIntMs() const { return m_player2TimeMs; }  ///< 後手残り時間(ms)
    int currentPlayer() const { return m_currentPlayer; }           ///< 現在の手番（1=先手, 2=後手）

    /// 考慮時間を外部から直接設定する（HvEでエンジン手のelapsed反映用）
    void setPlayer1ConsiderationTime(int ms);
    void setPlayer2ConsiderationTime(int ms);

    bool hasByoyomi1() const { return m_byoyomi1TimeMs > 0; }    ///< 先手に秒読みが設定されているか
    bool hasByoyomi2() const { return m_byoyomi2TimeMs > 0; }    ///< 後手に秒読みが設定されているか
    bool byoyomi1Applied() const { return m_byoyomi1Applied; }   ///< 先手が秒読みに入っているか
    bool byoyomi2Applied() const { return m_byoyomi2Applied; }   ///< 後手が秒読みに入っているか

    bool isGameOver() const { return m_gameOver; }                ///< 終局状態か
    void markGameOver() { m_gameOver = true; }                    ///< 終局状態に設定する
    bool isRunning() const { return m_clockRunning; }             ///< タイマー動作中か

    qint64 player1ConsiderationMs() const { return m_player1ConsiderationTimeMs; } ///< 先手の直近考慮(ms)
    qint64 player2ConsiderationMs() const { return m_player2ConsiderationTimeMs; } ///< 後手の直近考慮(ms)
    int getPlayer1ConsiderationTimeMs() const;
    int getPlayer2ConsiderationTimeMs() const;

    // --- USIパラメータ用 ---

    qint64 getBincMs() const { return m_bincMs; }   ///< 先手のインクリメント(ms)
    qint64 getWincMs() const { return m_wincMs; }   ///< 後手のインクリメント(ms)

    /// 両者共通の秒読み値を返す（不一致なら0: USIのbyoyomiパラメータに使えない）
    qint64 getCommonByoyomiMs() const {
        return (m_byoyomi1TimeMs > 0 && m_byoyomi2TimeMs > 0 && m_byoyomi1TimeMs == m_byoyomi2TimeMs)
        ? m_byoyomi1TimeMs : 0;
    }

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
    // --- 内部ヘルパ ---
    void saveState();
    void debugCheckInvariants() const;
    int remainingDisplaySecP1() const;
    int remainingDisplaySecP2() const;
    void updateShownConsiderationForPlayer(int player);

    // --- タイマー ---
    QTimer*       m_timer = nullptr;           ///< 定期更新タイマー（所有、this親）
    QElapsedTimer m_elapsedTimer;              ///< 経過時間計測用（モノトニック）
    bool          m_clockRunning = false;      ///< タイマー動作中フラグ
    qint64        m_lastTickMs   = 0;          ///< 前回tickの時刻(ms)

    // --- 設定・状態 ---
    bool   m_timeLimitSet  = false;            ///< 持ち時間制限が有効か
    bool   m_loseOnTimeout = true;             ///< 時間切れで負けとするか
    int    m_currentPlayer = 1;                ///< 現在の手番（1=先手, 2=後手）

    // --- 残り時間(ms) ---
    qint64 m_player1TimeMs = 0;                ///< 先手の残り時間
    qint64 m_player2TimeMs = 0;                ///< 後手の残り時間

    // --- 秒読み/インクリメント(ms) ---
    qint64 m_byoyomi1TimeMs = 0;               ///< 先手の秒読み時間
    qint64 m_byoyomi2TimeMs = 0;               ///< 後手の秒読み時間
    qint64 m_bincMs = 0;                       ///< 先手のインクリメント
    qint64 m_wincMs = 0;                       ///< 後手のインクリメント

    // --- 考慮時間(ms) ---
    qint64 m_player1ConsiderationTimeMs = 0;        ///< 先手の直近手の考慮時間
    qint64 m_player2ConsiderationTimeMs = 0;        ///< 後手の直近手の考慮時間
    qint64 m_player1TotalConsiderationTimeMs = 0;   ///< 先手の総考慮時間
    qint64 m_player2TotalConsiderationTimeMs = 0;   ///< 後手の総考慮時間

    // --- GUI表示用キャッシュ ---
    int m_p1LastMoveShownSec = 0;              ///< 先手の直近手表示秒数
    int m_p2LastMoveShownSec = 0;              ///< 後手の直近手表示秒数
    int m_p1PrevShownTotalSec = 0;             ///< 先手の前回総考慮表示秒数
    int m_p2PrevShownTotalSec = 0;             ///< 後手の前回総考慮表示秒数
    int m_prevShownSecP1 = -1;                 ///< 先手の前回残り秒表示（変化検出用）
    int m_prevShownSecP2 = -1;                 ///< 後手の前回残り秒表示（変化検出用）

    // --- 秒読み適用状態 ---
    bool m_byoyomi1Applied = false;            ///< 先手が秒読みに入った
    bool m_byoyomi2Applied = false;            ///< 後手が秒読みに入った

    // --- undo用の状態履歴スタック ---
    QStack<qint64> m_player1TimeHistory;                ///< 先手残り時間の履歴
    QStack<qint64> m_player2TimeHistory;                ///< 後手残り時間の履歴
    QStack<qint64> m_player1ConsiderationHistory;       ///< 先手考慮時間の履歴
    QStack<qint64> m_player2ConsiderationHistory;       ///< 後手考慮時間の履歴
    QStack<qint64> m_player1TotalConsiderationHistory;  ///< 先手総考慮の履歴
    QStack<qint64> m_player2TotalConsiderationHistory;  ///< 後手総考慮の履歴
    QStack<bool>   m_byoyomi1AppliedHistory;            ///< 先手秒読み適用の履歴
    QStack<bool>   m_byoyomi2AppliedHistory;            ///< 後手秒読み適用の履歴
    QStack<int>    m_p1LastMoveShownSecHistory;          ///< 先手直近手秒数の履歴
    QStack<int>    m_p2LastMoveShownSecHistory;          ///< 後手直近手秒数の履歴
    QStack<int>    m_p1PrevShownTotalSecHistory;         ///< 先手前回総考慮の履歴
    QStack<int>    m_p2PrevShownTotalSecHistory;         ///< 後手前回総考慮の履歴

    bool m_gameOver = false;                   ///< 終局フラグ
};

#endif // SHOGICLOCK_H
