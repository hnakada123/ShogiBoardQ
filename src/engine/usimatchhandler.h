#ifndef USIMATCHHANDLER_H
#define USIMATCHHANDLER_H

/// @file usimatchhandler.h
/// @brief 対局通信フロー・盤面データ管理を担当するハンドラクラスの定義

#include <QChar>
#include <QObject>
#include <QTimer>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

#include "usitimingparams.h"

class ShogiGameController;
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
    /// コールバック定義（Usiファサードからの注入用）
    struct Hooks {
        std::function<void()> onBestmoveTimeout; ///< bestmoveタイムアウト時の処理
        std::function<void(const QPoint&, const QPoint&, const QString&, const QString&)> onMoveReady = {};
    };

    UsiMatchHandler(UsiProtocolHandler* protocolHandler,
                    ThinkingInfoPresenter* presenter,
                    ShogiGameController* gameController);

    void setHooks(const Hooks& hooks);
    void setClock(ShogiClock* clock) { m_clock = clock; }
    void onBestMoveReceived();
    void requestMove(const QString& position, const QString& ponder, const UsiTimingParams& timing);
    void cancelAsync();

    // --- 盤面データ管理 ---

    /// ゲームコントローラから現在の盤面データをクローンする
    void cloneCurrentBoardData();

    /// 解析用の盤面データを初期化する
    void prepareBoardDataForAnalysis();

    /// 盤面データを直接設定する
    void setClonedBoardData(const QList<QChar>& boardData);

    /// 現在の盤面からSFEN文字列を計算する
    QString computeBaseSfenFromBoard() const;

    // --- 最終指し手管理 ---

    QString lastUsiMove() const;
    void setLastUsiMove(const QString& move);

    // --- 対局通信 ---

    QString convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote);

private:
    void startAsyncSearch();
    void onSearchTimeout();
    int remainingTimeMs(const UsiTimingParams& timing) const;
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

    UsiTimingParams timingForSearch(const UsiTimingParams& timing, bool pondering) const;

    void applyMovesToBoardFromBestMoveAndPonder();
    void updateBaseSfenForPonder();

    // --- 内部参照（非所有）---

    UsiProtocolHandler* m_protocolHandler = nullptr;
    ThinkingInfoPresenter* m_presenter = nullptr;
    ShogiGameController* m_gameController = nullptr;

    // --- 状態 ---

    QList<QChar> m_clonedBoardData;
    QString m_lastUsiMove;
    Hooks m_hooks;
    ShogiClock* m_clock = nullptr;
    bool m_acceptBestMove = false;
};

#endif // USIMATCHHANDLER_H
