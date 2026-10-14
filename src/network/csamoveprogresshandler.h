#ifndef CSAMOVEPROGRESSHANDLER_H
#define CSAMOVEPROGRESSHANDLER_H

/// @file csamoveprogresshandler.h
/// @brief CSA対局の指し手進行ハンドラの定義

#include <QCoreApplication>
#include <QObject>
#include "csaenginecontroller.h"
#include <QString>
#include <QStringList>
#include <QPoint>
#include <QTimer>
#include <functional>

#include "csaclient.h"
#include "csagamecoordinator.h"
#include "usimove.h"

class ShogiGameController;
class ShogiClock;
class CsaEngineController;
class CsaClient;

/**
 * @brief CSA対局の指し手受信・確認・エンジン思考・時間管理を担当するハンドラ
 *
 * CsaGameCoordinator から指し手進行に関わるロジックを分離し、
 * 相手の指し手受信、自分の指し手確認、エンジン思考、時間追跡を統合的に扱う。
 *
 * CsaGameCoordinator 内部状態への参照は Refs struct で受け取り、
 * シグナル発火や副作用は Hooks struct のコールバックで実行する。
 *
 * 解釈できない指し手行（不正な書式・盤面に適用できない手）は、サーバーが続けて送る
 * #ILLEGAL_MOVE などの裁定を待つ（仕様 3.4）。裁定が届かなければ中断として終局させる。
 */
class CsaMoveProgressHandler : public QObject
{
    Q_DECLARE_TR_FUNCTIONS(CsaMoveProgressHandler)

public:
    using GameState = CsaGameCoordinator::GameState;
    using PlayerType = CsaGameCoordinator::PlayerType;

    /**
     * @brief CsaGameCoordinator の内部状態への参照群
     *
     * @note CsaGameCoordinator::ensureMoveProgressHandler() で設定される。
     */
    struct Refs {
        // 後から設定・生成される部品（呼ばれた時点の値を返す。未設定なら nullptr）
        std::function<ShogiGameController*()> gameController;
        std::function<ShogiClock*()> clock;
        std::function<CsaEngineController*()> engineController;
        std::function<QStringList*()> sfenHistory;

        // クライアント
        CsaClient* client = nullptr;

        // ゲーム状態
        GameState* gameState = nullptr;
        PlayerType* playerType = nullptr;
        bool* isBlackSide = nullptr;
        bool* isMyTurn = nullptr;
        int* moveCount = nullptr;
        int* prevToFile = nullptr;
        int* prevToRank = nullptr;

        // 時間管理
        int* blackTotalTimeMs = nullptr;
        int* whiteTotalTimeMs = nullptr;
        int* blackRemainingMs = nullptr;
        int* whiteRemainingMs = nullptr;

        // 指し手データ
        QStringList* usiMoves = nullptr;
        QString* positionStr = nullptr;

        // 対局情報
        CsaClient::GameSummary* gameSummary = nullptr;
        /// エンジンに渡す持時間から差し引く通信の余裕（ミリ秒）
        const int* thinkingMarginMs = nullptr;
    };

    /**
     * @brief CsaGameCoordinator のシグナル発火・副作用コールバック群
     *
     * @note CsaGameCoordinator::ensureMoveProgressHandler() で lambda 経由で設定される。
     */
    struct Hooks {
        /// @brief ログメッセージを出力する
        std::function<void(const QString&, bool)> logMessage;

        /// @brief 続行できないエラーを通知し、対局中なら中断として終局させる
        std::function<void(const QString&)> failGame;

        /// @brief 指し手を通知する (csaMove, usiMove, prettyMove, consumedTimeMs)
        std::function<void(const QString&, const QString&, const QString&, int)> moveMade;

        /// @brief 手番変更を通知する
        std::function<void(bool)> turnChanged;

        /// @brief 指し手ハイライトを要求する
        std::function<void(const QPoint&, const QPoint&)> moveHighlightRequested;

        /// @brief 盤面に着手が反映されたことを通知する（相手の指し手・自エンジンの指し手）
        /// @note 人間の指し手は ShogiGameController::moveCommitted で通知されるため対象外
        std::function<void()> moveAppliedToBoard;

        /// @brief エンジン評価値を通知する
        std::function<void(int, int, const QString&)> engineScoreUpdated;

        /// @brief 投了を実行する
        std::function<void()> performResign;
    };

    CsaMoveProgressHandler();

    void setRefs(const Refs& refs);
    void setHooks(const Hooks& hooks);

    // --- 指し手処理 API ---

    void handleMoveReceived(const QString& move, int consumedTimeMs);
    void handleMoveConfirmed(const QString& move, int consumedTimeMs);
    void startEngineThinking();

    /// 解釈できなかった指し手の裁定待ちを取り消す（終局・切断時）
    void clearPendingVerdict();

    /**
     * @brief 自分がサーバーへ送った指し手を控える
     *
     * サーバーの指し手確認は CSA 形式なので、成った手か成駒を動かした手かを駒種だけでは
     * 区別できない。確認が届いたときは控えた指し手の成り・不成を使う。
     */
    void noteSentMove(const UsiMove& move) { m_sentMove = move; }

    /**
     * @brief 対局開始時に表示用時計を CSA の持時間に合わせる
     * @param clock 時計
     * @param summary 対局条件
     * @param blackRemainingMs 先手の残り時間（ミリ秒、再開局面の消費時間と加算を反映済み）
     * @param whiteRemainingMs 後手の残り時間（ミリ秒）
     *
     * 双方とも時間無制限なら消費時間を数える時計にする。以後は着手ごとに残り時間だけを合わせる。
     */
    static void startDisplayClock(ShogiClock* clock, const CsaClient::GameSummary& summary,
                                  qint64 blackRemainingMs, qint64 whiteRemainingMs);

    /// 裁定が届かないときに中断とみなすまでの時間
    static constexpr int kVerdictTimeoutMs = 5000;

private:
    void onEngineThinkingFinished(const CsaEngineController::ThinkingResult& result);
    void onVerdictTimeout();
    void updateTimeTracking(bool isBlackMove, int consumedTimeMs);
    void syncClockAfterMove(bool startMyTurnClock);

    /// 指し手行の書式・座標を確認し、問題があれば通知文を返す（問題なければ空）
    QString validateMoveText(const QString& move, QPoint* from, QPoint* to) const;
    /// 解釈できない指し手行を記録し、サーバーの裁定を待つ
    void deferVerdict(const QString& logText, const QString& failureText);
    /// 裁定待ちのまま次の行が来たら、続行できないとして終局させる
    bool failIfVerdictPending();

    ShogiGameController* currentGameController() const
    {
        return m_refs.gameController ? m_refs.gameController() : nullptr;
    }
    ShogiClock* currentClock() const { return m_refs.clock ? m_refs.clock() : nullptr; }
    CsaEngineController* currentEngine() const
    {
        return m_refs.engineController ? m_refs.engineController() : nullptr;
    }
    QStringList* currentSfenHistory() const { return m_refs.sfenHistory ? m_refs.sfenHistory() : nullptr; }

    Refs m_refs;
    Hooks m_hooks;
    QTimer m_verdictTimer;             ///< 裁定待ちのタイムアウト
    QString m_pendingFailureText;      ///< 裁定が届かなかったときの終局理由（空なら待ちなし）
    UsiMove m_sentMove;                ///< 最後にサーバーへ送った自分の指し手
};

#endif // CSAMOVEPROGRESSHANDLER_H
