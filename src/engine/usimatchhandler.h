#ifndef USIMATCHHANDLER_H
#define USIMATCHHANDLER_H

/// @file usimatchhandler.h
/// @brief 対局通信フロー・盤面データ管理を担当するハンドラクラスの定義

#include <QChar>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>
#include <functional>

class ShogiGameController;
class ThinkingInfoPresenter;
class UsiProtocolHandler;

/**
 * @brief 対局通信フロー・盤面データ管理を担当するハンドラクラス
 *
 * Usiファサードクラスから対局通信処理（ポンダー制御含む）と
 * 盤面データ管理（クローン・SFEN計算）を分離したもの。
 *
 * 非QObjectクラス。エラー通知はHooksコールバック経由で行う。
 */
class UsiMatchHandler
{
public:
    /// コールバック定義（Usiファサードからの注入用）
    struct Hooks {
        std::function<void()> onBestmoveTimeout; ///< bestmoveタイムアウト時の処理
    };

    UsiMatchHandler(UsiProtocolHandler* protocolHandler,
                    ThinkingInfoPresenter* presenter,
                    ShogiGameController* gameController);

    void setHooks(const Hooks& hooks);

    // --- 盤面データ管理 ---

    /// ゲームコントローラから現在の盤面データをクローンする
    void cloneCurrentBoardData();

    /// 解析用の盤面データを初期化する
    void prepareBoardDataForAnalysis();

    /// 盤面データを直接設定する
    void setClonedBoardData(const QVector<QChar>& boardData);

    /// 現在の盤面からSFEN文字列を計算する
    QString computeBaseSfenFromBoard() const;

    // --- 最終指し手管理 ---

    QString lastUsiMove() const;
    void setLastUsiMove(const QString& move);

    // --- 対局通信 ---

    void handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                          QPoint& outFrom, QPoint& outTo,
                                          int byoyomiMilliSec, const QString& btime,
                                          const QString& wtime, QStringList& positionStrList,
                                          int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                          bool useByoyomi);

    void handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
                                                       QString& positionPonderStr,
                                                       QPoint& outFrom, QPoint& outTo,
                                                       int byoyomiMilliSec, const QString& btime,
                                                       const QString& wtime,
                                                       int addEachMoveMilliSec1,
                                                       int addEachMoveMilliSec2, bool useByoyomi);

    QString convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote);

private:
    void executeEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                    QPoint& outFrom, QPoint& outTo, int byoyomiMilliSec,
                                    const QString& btime, const QString& wtime,
                                    int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                    bool useByoyomi);

    void processEngineResponse(QString& positionStr, QString& positionPonderStr,
                               int byoyomiMilliSec, const QString& btime, const QString& wtime,
                               int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);

    void sendCommandsAndProcess(int byoyomiMilliSec, QString& positionStr,
                                const QString& btime, const QString& wtime,
                                QString& positionPonderStr, int addEachMoveMilliSec1,
                                int addEachMoveMilliSec2, bool useByoyomi);

    void startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr);
    void appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr);

    void waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec, const QString& btime,
                                              const QString& wtime, bool useByoyomi);

    void applyMovesToBoardFromBestMoveAndPonder();
    void updateBaseSfenForPonder();

    // --- 内部参照（非所有）---

    UsiProtocolHandler* m_protocolHandler = nullptr;
    ThinkingInfoPresenter* m_presenter = nullptr;
    ShogiGameController* m_gameController = nullptr;

    // --- 状態 ---

    QVector<QChar> m_clonedBoardData;
    QString m_lastUsiMove;
    Hooks m_hooks;
};

#endif // USIMATCHHANDLER_H
