#ifndef CSAMOVEPROGRESSHANDLER_H
#define CSAMOVEPROGRESSHANDLER_H

/// @file csamoveprogresshandler.h
/// @brief CSA対局の指し手進行ハンドラの定義

#include <QCoreApplication>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QPoint>
#include <functional>

#include "csaclient.h"
#include "csagamecoordinator.h"

class ShogiGameController;
class ShogiView;
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
 */
class CsaMoveProgressHandler
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
        // GUI コンポーネント
        QPointer<ShogiGameController>* gameController = nullptr;
        QPointer<ShogiView>* view = nullptr;
        QPointer<ShogiClock>* clock = nullptr;

        // エンジン・クライアント
        CsaEngineController** engineController = nullptr;
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
        QStringList** sfenHistory = nullptr;
        QString* positionStr = nullptr;

        // 対局情報
        CsaClient::GameSummary* gameSummary = nullptr;
    };

    /**
     * @brief CsaGameCoordinator のシグナル発火・副作用コールバック群
     *
     * @note CsaGameCoordinator::ensureMoveProgressHandler() で lambda 経由で設定される。
     */
    struct Hooks {
        /// @brief ゲーム状態を変更する
        std::function<void(GameState)> setGameState;

        /// @brief ログメッセージを出力する
        std::function<void(const QString&, bool)> logMessage;

        /// @brief エラーメッセージを出力する
        std::function<void(const QString&)> errorOccurred;

        /// @brief 指し手を通知する (csaMove, usiMove, prettyMove, consumedTimeMs)
        std::function<void(const QString&, const QString&, const QString&, int)> moveMade;

        /// @brief 手番変更を通知する
        std::function<void(bool)> turnChanged;

        /// @brief 指し手ハイライトを要求する
        std::function<void(const QPoint&, const QPoint&)> moveHighlightRequested;

        /// @brief エンジン評価値を通知する
        std::function<void(int, int)> engineScoreUpdated;

        /// @brief 投了を実行する
        std::function<void()> performResign;
    };

    void setRefs(const Refs& refs);
    void setHooks(const Hooks& hooks);

    // --- 指し手処理 API ---

    void handleMoveReceived(const QString& move, int consumedTimeMs);
    void handleMoveConfirmed(const QString& move, int consumedTimeMs);
    void startEngineThinking();

private:
    void updateTimeTracking(bool isBlackMove, int consumedTimeMs);
    void syncClockAfterMove(bool startMyTurnClock);

    Refs m_refs;
    Hooks m_hooks;
};

#endif // CSAMOVEPROGRESSHANDLER_H
