#ifndef GAMESESSION_H
#define GAMESESSION_H

/// @file gamesession.h
/// @brief 対局の利用者の操作（開始・投了・中断・再開・待った・新規）と、対局の棋譜の受け手

#include <QObject>
#include <QString>

#include "gamestartoptionsbuilder.h"
#include "matchcoordinator.h"
#include "matchrecordsink.h"

struct AppState;
class AnalysisSession;
class ConsecutiveGameSeries;
class CsaGameCoordinator;
class GameRecordModel;
class KifuFileController;
class KifuNavigationState;
class LiveGameSession;
class MainWindowAppearanceController;
class MatchTimeRecord;
class PlayerInfoPresenter;
class SessionResetCoordinator;
class ShogiClock;
class ShogiView;
class TimeDisplayPresenter;
class UiStatePolicyManager;

/**
 * @brief 対局の利用者の操作を受け、対局の司令塔（MatchCoordinator）・通信対局へ頼む
 *
 * 対局の開始はここで1つの関数として順に行う（棋譜を整える → 時計 → 対局者の表示 → 対局 → 初手）。
 * 対局の棋譜の受け手（MatchRecordSink）でもあり、確定した行はライブ対局（分岐ツリー）に書く。
 * 盤・棋譜欄・手番の表示は、ツリーと表示中の手が変わると各部品が自分で合わせる。
 */
class GameSession : public QObject, public MatchRecordSink
{
    Q_OBJECT

public:
    struct Deps {
        AppState* state = nullptr;                          ///< 対局者・上書き保存先・動作状態
        MatchCoordinator* match = nullptr;                  ///< ローカル対局の司令塔
        AnalysisSession* analysis = nullptr;                ///< 検討・詰み探索（「新規」で止める）
        ConsecutiveGameSeries* consecutive = nullptr;  ///< 連続対局
        SessionResetCoordinator* reset = nullptr;           ///< 棋譜と表示の作り直し
        KifuNavigationState* navState = nullptr;            ///< 表示中の手（現在局面からの開始・再開）
        LiveGameSession* liveSession = nullptr;             ///< 対局の手順の書き先
        GameRecordModel* gameRecord = nullptr;              ///< 未保存の印
        ShogiClock* clock = nullptr;                        ///< 対局の時計
        MatchTimeRecord* timeRecord = nullptr;              ///< 持ち時間と開始・終了の日時
        TimeDisplayPresenter* timePresenter = nullptr;      ///< 時計の表示
        PlayerInfoPresenter* playerInfo = nullptr;          ///< 対局者・対局情報の表示
        UiStatePolicyManager* uiStatePolicy = nullptr;      ///< 「再開」を出すか
        KifuFileController* kifuFileController = nullptr;   ///< 未保存の棋譜の確認
        ShogiView* shogiView = nullptr;                     ///< 手前に表示している側
        MainWindowAppearanceController* appearance = nullptr; ///< 「人を手前に表示する」の盤の反転
        CsaGameCoordinator* csaGame = nullptr;              ///< 通信対局中の投了・中断・即指しの送り先（必須）
    };

    explicit GameSession(const Deps& deps, QObject* parent = nullptr);

    // --- MatchRecordSink（対局・通信対局の棋譜の行） ---
    bool canAppendRecord() const override;
    void appendRecord(const RecordEntry& entry) override;
    bool canUndoRecords(int count) const override;
    void undoRecords(int count) override;

    /// 計画どおりに対局を始める。beginSeries なら連続対局の1局目として局数を数え始める
    bool startGame(const GameStartPlan& plan, bool beginSeries);

    // --- 通信対局 ---
    /// 通信対局の棋譜を、サーバーの開始局面と対局者名で作り直してライブ記録を始める
    void beginCsaGameRecord(const QString& startSfen, const QString& blackName, const QString& whiteName);
    /// 通信対局が終わった（ライブ記録を確定し、終局時刻を記録する。ローカル対局の終局と同じ後始末）
    void endCsaGameRecord();
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
    /// 「新規」: 対局・検討・通信対局を止め、平手の開始局面だけの棋譜にする（未保存の確認は呼び出し側）
    void resetToInitialState();
    /// 中断した対局を再開できなくする（棋譜を変えたとき・別の対局を始めたとき）
    void discardInterruptedGame();
    /// 棋譜を読み込んだ。前の対局の対局者・持ち時間・開始と終了の日時を、保存や表示に使わないよう消す
    void forgetFinishedMatch();

    // --- 司令塔から ---
    void onGameEnded(const MatchCoordinator::GameEndInfo& info);
    void onGameEndProcessed(const MatchCoordinator::GameEndInfo& info);
    void onGameInterrupted();
    /// 連続対局の次局
    void startNextGame(const GameStartPlan& plan);

private:
    bool isCsaGameActive() const;
    QString recordStartSfen() const;

    Deps m_deps;
    int m_interruptedNodeId = -1;

};

#endif // GAMESESSION_H
