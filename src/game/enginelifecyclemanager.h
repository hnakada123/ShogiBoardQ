#ifndef ENGINELIFECYCLEMANAGER_H
#define ENGINELIFECYCLEMANAGER_H

/// @file enginelifecyclemanager.h
/// @brief エンジンライフサイクル管理クラスの定義

#include <QObject>
#include <QString>
#include <QPoint>
#include <functional>

#include "playmode.h"

class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class ShogiGameController;

/**
 * @brief USIエンジンの生成・破棄・シグナル配線・USI送信・指し手実行を担当するマネージャ
 *
 * MatchCoordinator からエンジン管理関連のロジックを分離する。
 * エンジンの所有権はこのクラスが管理し、投了・入玉宣言勝ちのシグナル配線も行う。
 *
 * MatchCoordinator 内部状態への参照は Refs struct で受け取り、
 * UIコールバックは Hooks struct で実行する。
 */
class EngineLifecycleManager : public QObject
{
    Q_OBJECT

public:
    /// 対局者を表す列挙値（MatchCoordinator::Player と互換）
    enum Player : int {
        P1 = 1,
        P2 = 2
    };

    /// USI goコマンドに渡す時間パラメータ（MatchCoordinator::GoTimes と互換）
    struct GoTimes {
        qint64 btime = 0;
        qint64 wtime = 0;
        qint64 byoyomi = 0;
        qint64 binc = 0;
        qint64 winc = 0;
    };

    /// MatchCoordinator の内部状態への参照群
    struct Refs {
        ShogiGameController* gc = nullptr;
        PlayMode* playMode = nullptr;
    };

    /**
     * @brief エンジン管理用コールバック群
     *
     * エンジンの思考結果適用・投了/勝ち宣言検出時に MC の状態更新や GUI 操作を行う。
     * log, renderBoardFromGc, appendEvalP1/P2 は MC::Hooks からのパススルー。
     *
     * @note MC::ensureEngineManager() で設定される。
     * @see MatchCoordinator::ensureEngineManager
     */
    struct Hooks {
        /// @brief GC の盤面状態を View に反映する
        /// @note 配線元: MC→m_hooks.renderBoardFromGc (パススルー)
        std::function<void()> renderBoardFromGc;

        /// @brief P1着手確定時に評価値グラフを更新する
        /// @note 配線元: MC→m_hooks.appendEvalP1 (パススルー)
        std::function<void()> appendEvalP1;

        /// @brief P2着手確定時に評価値グラフを更新する
        /// @note 配線元: MC→m_hooks.appendEvalP2 (パススルー)
        std::function<void()> appendEvalP2;

        /// @brief エンジン投了を MC に通知する（idx: 1=usi1, 2=usi2）
        /// @note 配線元: MC lambda → MC::handleEngineResign
        std::function<void(int)> onEngineResign;

        /// @brief エンジン勝ち宣言を MC に通知する（idx: 1=usi1, 2=usi2）
        /// @note 配線元: MC lambda → MC::handleEngineWin
        std::function<void(int)> onEngineWin;

        /// @brief 現在の持ち時間情報 (GoTimes) を計算して返す
        /// @note 配線元: MC lambda (remainingMsFor/incrementMsFor/byoyomiMs から算出)
        std::function<GoTimes()> computeGoTimes;
    };

    /// エンジンモデルペア
    struct EngineModelPair {
        UsiCommLogModel* comm = nullptr;
        ShogiEngineThinkingModel* think = nullptr;
    };

    explicit EngineLifecycleManager(QObject* parent = nullptr);

    void setRefs(const Refs& refs);
    void setHooks(const Hooks& hooks);

    // --- ライフサイクル ---

    void initializeAndStartEngineFor(Player side,
                                     const QString& enginePath,
                                     const QString& engineName);
    void destroyEngine(int idx, bool clearThinking = true);
    void destroyEngines(bool clearModels = true);
    void initEnginesForEvE(const QString& engineName1,
                           const QString& engineName2);

    // --- エンジンポインタ ---

    Usi* usi1() const;
    Usi* usi2() const;
    void setUsi1(Usi* u);
    void setUsi2(Usi* u);
    void updateUsiPtrs(Usi* e1, Usi* e2);
    bool isShutdownInProgress() const;
    void setShutdownInProgress(bool v);

    // --- モデル ---

    EngineModelPair ensureEngineModels(int engineIndex);
    void setModelPtrs(UsiCommLogModel* comm1, ShogiEngineThinkingModel* think1,
                      UsiCommLogModel* comm2, ShogiEngineThinkingModel* think2);

    // --- 指し手実行 ---

    bool engineThinkApplyMove(Usi* engine,
                              QString& positionStr, QString& ponderStr,
                              QPoint* outFrom, QPoint* outTo);
    bool engineMoveOnce(Usi* eng,
                        QString& positionStr, QString& ponderStr,
                        bool useSelectedField2, int engineIndex,
                        QPoint* outTo);

    // --- USI送信 ---

    void sendGoToEngine(Usi* which, const GoTimes& t);
    void sendStopToEngine(Usi* which);
    void sendRawToEngine(Usi* which, const QString& cmd);

    // --- シグナル配線 ---

    void wireResignToArbiter(Usi* engine, bool asP1);
    void wireWinToArbiter(Usi* engine, bool asP1);

private slots:
    void onEngine1Resign();
    void onEngine2Resign();
    void onEngine1Win();
    void onEngine2Win();

private:
    void sendRawTo(Usi* which, const QString& cmd);

    Refs m_refs;
    Hooks m_hooks;

    Usi* m_usi1 = nullptr;
    Usi* m_usi2 = nullptr;
    UsiCommLogModel*          m_comm1  = nullptr;
    ShogiEngineThinkingModel* m_think1 = nullptr;
    UsiCommLogModel*          m_comm2  = nullptr;
    ShogiEngineThinkingModel* m_think2 = nullptr;
    bool m_engineShutdownInProgress = false;
};

#endif // ENGINELIFECYCLEMANAGER_H
