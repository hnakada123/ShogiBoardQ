#ifndef MATCHCOORDINATOR_H
#define MATCHCOORDINATOR_H

#include <QObject>
#include <QString>
#include <functional>

class ShogiGameController;
class ShogiClock;
class ShogiView;
class Usi;

// 対局進行/終局/時計/USI送受のハブ（寿命は Main 側で管理）
class MatchCoordinator : public QObject {
    Q_OBJECT
public:
    enum Player : int { P1 = 1, P2 = 2 };

    enum class Cause : int { Resignation = 0, Timeout = 1 };

    struct GoTimes {
        qint64 btime = 0;   // 先手 残り(ms)
        qint64 wtime = 0;   // 後手 残り(ms)
        qint64 byoyomi = 0; // 共通 秒読み(ms)
        qint64 binc = 0;    // 先手 増加(ms)
        qint64 winc = 0;    // 後手 増加(ms)
    };

    struct GameEndInfo {
        Cause  cause = Cause::Resignation;
        Player loser = P1;
    };

    struct Hooks {
        // --- UI/描画系の委譲 ---
        std::function<void(Player cur)> updateTurnDisplay; // 盤面の手番表示/ハイライト
        std::function<void(const QString& p1, const QString& p2)> setPlayersNames;
        std::function<void(const QString& e1, const QString& e2)> setEngineNames;
        std::function<void(bool inProgress)> setGameActions; // NewGame/Resign 等のON/OFF
        std::function<void()> renderBoardFromGc;             // gc→view 反映
        std::function<void(const QString& title, const QString& message)> showGameOverDialog;
        std::function<void(const QString& msg)> log;         // 任意ログ

        // --- 時計読み出し（ShogiClock API 差異吸収） ---
        std::function<qint64(Player)> remainingMsFor; // 残り時間
        std::function<qint64(Player)> incrementMsFor; // フィッシャー増加
        std::function<qint64()> byoyomiMs;            // 秒読み（共通）

        // --- USI 送受（go/stop/任意生コマンド） ---
        std::function<void(Usi* which, const GoTimes& t)> sendGoToEngine;
        std::function<void(Usi* which)> sendStopToEngine;
        std::function<void(Usi* which, const QString& cmd)> sendRawToEngine; // 任意

        // --- 新規対局の初期化（GUI固有処理） ---
        std::function<void(const QString& sfenStart)> initializeNewGame;
    };

    struct Deps {
        ShogiGameController* gc = nullptr;
        ShogiClock*          clock = nullptr;
        ShogiView*           view = nullptr;
        Usi*                 usi1 = nullptr;
        Usi*                 usi2 = nullptr;
        Hooks                hooks;
    };

    explicit MatchCoordinator(const Deps& deps, QObject* parent=nullptr);
    ~MatchCoordinator() override;

    // 外部 API
    void startNewGame(const QString& sfenStart);
    void handleResign();                 // 人間の投了
    void handleEngineResign(int idx);    // エンジン投了通知(1 or 2)
    void notifyTimeout(Player loser);    // ★ 時間切れ通知（UI→司令塔）
    void flipBoard();                    // 盤反転 + 表示更新
    void onTurnFinishedAndSwitch();      // 手番切替時（時計/UI更新 + go送信）
    void updateUsiPtrs(Usi* e1, Usi* e2);// エンジン再生成時などに差し替え

signals:
    void gameOverShown();
    void boardFlipped(bool nowFlipped);
    void gameStarted();
    void gameEnded(const GameEndInfo& info);

private:
    // NOTE: QPointer は使わず raw ポインタ（寿命は Main 側で管理）
    ShogiGameController* m_gc   = nullptr;
    ShogiClock*          m_clock= nullptr;
    ShogiView*           m_view = nullptr;
    Usi*                 m_usi1 = nullptr;
    Usi*                 m_usi2 = nullptr;
    Hooks                m_hooks;

    Player m_cur = P1;

private:
    // Main から段階的に移す関数群
    void setPlayersNamesForMode_();
    void setEngineNamesBasedOnMode_();
    void setGameInProgressActions_(bool inProgress);
    void renderShogiBoard_();
    void updateTurnDisplay_(Player p);

    // 時計/USI 時間計算
    GoTimes computeGoTimes_() const;

    // 終局処理
    void stopClockAndSendStops_();
    void displayResultsAndUpdateGui_(const GameEndInfo& info);

public:
    // MainWindow からエンジン初期化/開始を委譲するための統一API
    // side: P1 or P2
    void initializeAndStartEngineFor(Player side,
                                     const QString& enginePathIn,
                                     const QString& engineNameIn);

    // resign シグナルの配線（司令塔→自分の onXxx に直結）
    void wireResignSignals();

    // ゲームオーバー（win+quit）を一方のエンジンに送る
    void sendGameOverWinAndQuitTo(int idx); // idx: 1 or 2

    // エンジン破棄（片方 or 両方）
    void destroyEngine(int idx);   // idx: 1 or 2
    void destroyEngines();

    // （移行ステップ用）ポインタ参照が必要な場合に備えたアクセサ
    Usi* enginePtr(int idx) const; // 1 or 2 → Usi*

private:
    // resign 配線の実装（内部ユーティリティ）
    void wireResignToArbiter_(Usi* engine, bool asP1);

    // 保有エンジンから index を求める（1/2/0）
    int indexForEngine_(const Usi* p) const;

private slots:
    void onEngine1Resign();
    void onEngine2Resign();
};

#endif // MATCHCOORDINATOR_H
