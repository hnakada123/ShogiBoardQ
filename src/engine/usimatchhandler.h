#ifndef USIMATCHHANDLER_H
#define USIMATCHHANDLER_H

/// @file usimatchhandler.h
/// @brief 対局通信フロー・盤面データ管理を担当するハンドラクラスの定義

#include <QObject>
#include <QTimer>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

#include "usitimingparams.h"
#include "usigotime.h"
#include "usimove.h"

class ThinkingInfoPresenter;
class UsiProtocolHandler;
class ShogiClock;

/**
 * @brief 対局通信フロー・盤面データ管理を担当するハンドラクラス
 *
 * Usiファサードクラスから対局通信処理（ポンダー制御含む）と
 * 盤面データ管理（クローン・SFEN計算）を分離したもの。
 *
 * エラー・着手通知はHooksコールバック経由で行う。
 */
class UsiMatchHandler : public QObject
{
public:
    struct SearchTiming {
        UsiGoTimeInput input;
        int timeoutMs = 30 * 60 * 1000;
        bool expired = false;
    };

    /// コールバック定義（Usiファサードからの注入用）
    struct Hooks {
        std::function<void()> onBestmoveTimeout; ///< bestmoveタイムアウト時の処理
        std::function<void(const UsiMove&, const QString&, const QString&)> onMoveReady = {};
        /// 時計から探索の時間を読む（sideToMove: エンジンの手番。先読みでも自分の手番）
        std::function<SearchTiming(const UsiTimingParams&, Turn sideToMove, bool pondering)> readTiming = {};
        std::function<bool()> finishTurn = {}; ///< 時計を確定し、有効な応答ならtrue

    };

    explicit UsiMatchHandler(UsiProtocolHandler* protocolHandler);
    void setHooks(const Hooks& hooks);
    void onBestMoveReceived();
    void requestMove(const QString& position, const QString& ponder, const UsiTimingParams& timing);
    void cancelAsync();

    // --- 最終指し手管理 ---

    QString lastUsiMove() const;
    void setLastUsiMove(const QString& move);

private:
    void startAsyncSearch();
    void onSearchTimeout();
    SearchTiming searchTiming(const UsiTimingParams& timing, bool pondering = false) const;
    static constexpr int kUnlimitedSearchTimeoutMs = 30 * 60 * 1000; ///< 無制限対局の安全上限
    enum class Pending { None, PonderStop, Move };
    Pending m_pending = Pending::None;
    QString m_position;
    QString m_ponder;
    UsiTimingParams m_timing;
    QTimer m_responseTimer;

    void startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr,
                                   const UsiTimingParams& timing);
    void appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr,
                                        const UsiTimingParams& timing);

    UsiProtocolHandler* m_protocolHandler = nullptr;

    // --- 状態 ---

    QString m_lastUsiMove;
    Hooks m_hooks;
};

#endif // USIMATCHHANDLER_H
