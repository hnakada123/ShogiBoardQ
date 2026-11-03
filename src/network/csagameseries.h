#ifndef CSAGAMESERIES_H
#define CSAGAMESERIES_H

/// @file csagameseries.h
/// @brief 1つの接続（再接続を含む）で続ける通信対局の数と、終局後の再接続

#include <QObject>

class QTimer;

/**
 * @brief 1つの接続（再接続を含む）で続ける通信対局の数と、終局後の再接続の待ち
 *
 * 対局数の上限（0 は制限なし）と終えた対局数から、まだ対局を続けるかを決める。shogi-server のように
 * 終局のたびに切断するサーバーでは、終局がサーバーの裁定によるもので対局数が残っていれば、少し待ってから
 * 接続し直す（reconnectDue）。接続とログイン・ログの文は司令塔（CsaGameCoordinator）が持つ。
 */
class CsaGameSeries : public QObject
{
    Q_OBJECT

public:
    /// 終局後の再接続までの待ち時間
    static constexpr int kReconnectDelayMs = 3000;

    explicit CsaGameSeries(QObject* parent = nullptr);

    /// 新しい接続を始める（対局数を数え直す。gameLimit は 0 で制限なし）
    void begin(int gameLimit, bool reconnectAfterGame);
    /// まだ対局を続けるか（対局数の上限に達していない）
    bool moreGamesWanted() const { return m_gameLimit <= 0 || m_completedGames < m_gameLimit; }
    int completedGames() const { return m_completedGames; }
    /// 対局数の上限（0 は制限なし）
    int gameLimit() const { return m_gameLimit; }

    /// 1局を終えた（対局数を数える）
    void finishGame() { ++m_completedGames; }
    /// 直前の終局がサーバーの裁定によるものか（切断・こちらの失敗による終局なら false。再接続の条件）
    void setLastEndedByServer(bool byServer) { m_lastEndedByServer = byServer; }
    /**
     * @brief 終局の後にサーバーが切断した。接続し直すなら再接続の待ちを始めて true
     *
     * 再接続の設定があり、直前の終局がサーバーの裁定によるもので、対局数が残っているときだけ接続し直す。
     */
    bool scheduleReconnect();
    /// 再接続を待っているか
    bool isReconnectPending() const;
    /// 再接続の待ちをやめる（通信対局をやめるとき）
    void stop();

signals:
    /// 再接続の待ちが終わった（次の対局のために接続し直す）
    void reconnectDue();

private:
    QTimer* m_reconnectTimer;
    int m_gameLimit = 0;
    int m_completedGames = 0;
    bool m_reconnectAfterGame = false;
    bool m_lastEndedByServer = false;
};

#endif // CSAGAMESERIES_H
