#ifndef ENGINELIFECYCLEMANAGER_H
#define ENGINELIFECYCLEMANAGER_H

/// @file enginelifecyclemanager.h
/// @brief エンジンライフサイクル管理クラスの定義

#include "matchtypes.h"
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
    /// MatchCoordinator の内部状態への参照群
    struct Refs {
        ShogiGameController* gc = nullptr;
    };

    /**
     * @brief エンジン管理用コールバック群
     *
     * エンジンの投了・勝ち宣言を MC に知らせる。
     *
     * @note MC::createEngineManager() で設定される。
     * @see MatchCoordinator::createEngineManager
     */
    struct Hooks {
        /// @brief エンジン投了を MC に通知する（side: 投了したエンジンの手番）
        /// @note 配線元: MC lambda → MC::handleEngineResign
        std::function<void(Turn)> onEngineResign;

        /// @brief エンジン勝ち宣言を MC に通知する（side: 宣言したエンジンの手番）
        /// @note 配線元: MC lambda → MC::handleEngineWin
        std::function<void(Turn)> onEngineWin;
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

    /// side はエンジンの手番。エンジンの枠は、先手なら usi1、後手なら usi2（空なら usi1）
    void initializeAndStartEngineFor(Turn side,
                                     const QString& enginePath,
                                     const QString& engineName);
    /// エンジンの枠（1=usi1, 2=usi2）のエンジンを破棄する
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

    // --- USI送信 ---

    void sendStopToEngine(Usi* which);

    // --- シグナル配線 ---

    /// エンジンの投了・勝ち宣言を、side（そのエンジンの手番）の知らせとして MC に送る
    void wireResignToArbiter(Usi* engine, Turn side);
    void wireWinToArbiter(Usi* engine, Turn side);
    void disconnectArbiterSignals();

signals:
    void enginesChanged();
    /// 対局用に起動したエンジンのエラー（Usi::errorOccurred の転送）
    void engineError(const QString& message);

private slots:
    void onBlackEngineResign();
    void onWhiteEngineResign();
    void onBlackEngineWin();
    void onWhiteEngineWin();

private:
    void disconnectArbiterSignals(Usi* engine);

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
