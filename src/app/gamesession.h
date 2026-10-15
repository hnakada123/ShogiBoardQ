#ifndef GAMESESSION_H
#define GAMESESSION_H

/// @file gamesession.h
/// @brief 対局の流れ（開始・着手の記録・終局・待った・中断と再開・連続対局）の UI 側の受け持ち

#include <QObject>
#include <QString>

#include "gamestartcoordinator.h"
#include "mainwindowresetservice.h"
#include "matchcoordinator.h"
#include "matchrecordsink.h"

struct AppState;
class AnalysisSession;
class BoardInteractionController;
class BoardSyncPresenter;
class ConsecutiveGamesController;
class CsaGameWiring;
class DialogCoordinator;
class EvaluationGraphController;
class GameRecordModel;
class JosekiWindowController;
class KifuBranchTree;
class KifuFileController;
class KifuNavigationCoordinator;
class KifuNavigationState;
class KifuRecordListModel;
class LiveGameSession;
class PlayerInfoPresenter;
class PreStartCleanupHandler;
class RecordPane;
class ReplayController;
class ShogiGameController;
class ShogiView;
class TimeControlController;
class TurnStateSyncService;
class UiStatePolicyManager;

/**
 * @brief 対局の流れの UI 側の受け持ち
 *
 * 利用者の操作（対局開始・投了・中断・再開・すぐ指させる・待った）を受け、司令塔（MatchCoordinator）や
 * 通信対局（CsaGameCoordinator）に頼む。司令塔の知らせ（開始・終局・中断・待った）を受けて、
 * 棋譜欄・分岐ツリー・時計・盤の表示・メニューの状態を合わせる。
 *
 * 対局の棋譜の受け手（MatchRecordSink）でもある。確定した行はライブ対局のセッション（分岐ツリー）に追記する。
 */
class GameSession : public QObject, public MatchRecordSink
{
    Q_OBJECT

public:
    struct Deps {
        AppState* state = nullptr;
        MatchCoordinator* match = nullptr;
        GameStartCoordinator* gameStart = nullptr;
        CsaGameWiring* csa = nullptr;                     ///< 通信対局の司令塔は最初の CSA 対局で作られる
        AnalysisSession* analysis = nullptr;              ///< 検討・詰み探索（対局を始める前に止める）
        ConsecutiveGamesController* consecutive = nullptr;
        PreStartCleanupHandler* preStartCleanup = nullptr;

        ShogiGameController* gameController = nullptr;
        ShogiView* shogiView = nullptr;
        BoardInteractionController* boardController = nullptr;
        BoardSyncPresenter* boardSync = nullptr;
        TurnStateSyncService* turnSync = nullptr;
        TimeControlController* timeController = nullptr;
        ReplayController* replayController = nullptr;
        UiStatePolicyManager* uiStatePolicy = nullptr;
        PlayerInfoPresenter* playerInfo = nullptr;
        JosekiWindowController* josekiController = nullptr;
        DialogCoordinator* dialogCoordinator = nullptr;
        KifuFileController* kifuFileController = nullptr;

        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;
        KifuNavigationCoordinator* kifuNav = nullptr;
        LiveGameSession* liveSession = nullptr;
        GameRecordModel* gameRecord = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        RecordPane* recordPane = nullptr;
        EvaluationGraphController* evalGraph = nullptr;

        MainWindowResetService::SessionUiDeps sessionUi;   ///< 対局ごとに消すログ・思考表示
        MainWindowResetService::ModelResetDeps models;     ///< 「新規」で作り直す棋譜まわり
    };

    explicit GameSession(QObject* parent = nullptr);
    void setDeps(const Deps& deps);

    // --- MatchRecordSink（対局・通信対局の棋譜の行） ---
    bool canAppendRecord() const override;
    void appendRecord(const RecordEntry& entry) override;
    bool canUndoRecords(int count) const override;
    void undoRecords(int count) override;

    // --- リセット ---

    /// 「新規」: 対局・検討・通信対局を止め、平手の開始局面にする
    void resetToInitialState();
    /// 棋譜を読み込む前に、前の対局のログ・グラフ・コメントを消す
    void clearUiBeforeKifuLoad();
    /// 棋譜まわり（分岐ツリー・対局情報・終了時刻）を開始局面 sfen で作り直す
    void resetModels(const QString& sfen);

    // --- 通信対局 ---

    /// 通信対局の棋譜を、サーバーの開始局面と対局者名で作り直してライブ記録を始める
    void beginCsaGameRecord(const QString& startSfen, const QString& blackName, const QString& whiteName);
    /// 終局時刻を記録して対局情報に反映する
    void recordGameEndTime();
    /// 対局者を下手・上手と呼ぶか（駒落ちで、対局情報の見出しが下手・上手のとき）
    bool usesHandicapNames() const;

public slots:
    // --- 利用者の操作 ---
    /// 対局開始ダイアログを出し、選んだ条件で対局を始める
    void startGameFromDialog();
    void resign();
    void breakOff();
    void makeEngineMoveNow();
    void undoTwoMoves();
    void resumeInterruptedGame();
    /// 中断した対局を再開できなくする（棋譜を変えたとき・別の対局を始めたとき）
    void discardInterruptedGame();
    /// 評価値グラフを消す（局面編集を始めたとき）
    void clearEvalState();

    // --- 対局の開始（GameStartCoordinator から） ---
    void prepareStart(const GameStartPreparation& preparation);
    void applyTimeControl(const GameStartCoordinator::TimeSettings& tc);
    void onGameStarted(const MatchStartOptions& opt);

    // --- 司令塔から ---
    /// 対局を準備した（盤は開始局面）。盤を描き直し、対局者名・エンジン名の表示をモードに合わせる
    void onGamePrepared(const MatchStartOptions& opt);
    /// 盤を描き直す（着手・待った）
    void renderBoard();
    void onGameOverStateChanged(const MatchCoordinator::GameOverState& st);
    void onGameEnded(const MatchCoordinator::GameEndInfo& info);
    void onGameEndProcessed(const MatchCoordinator::GameEndInfo& info);
    void onGameInterrupted();
    void onMovesUndone(int ply);

    // --- 連続対局 ---
    void startNextGame(const GameStartCoordinator::StartParams& params);

private:
    bool isCsaGameActive() const;
    void stopEnginesAndNetwork();
    void resetGameState();
    void resetUiState(const QString& sfen);

    Deps m_deps;
    int m_interruptedNodeId = -1;
};

#endif // GAMESESSION_H
