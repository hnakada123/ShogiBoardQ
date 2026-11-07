#ifndef GAMESESSION_H
#define GAMESESSION_H

/// @file gamesession.h
/// @brief 対局の利用者の操作（開始・投了・中断・再開・待った・新規）と、対局の棋譜の受け手

#include <QObject>
#include <QPointer>
#include <QString>
#include <memory>

#include "gamestartoptionsbuilder.h"
#include "matchautosave.h"
#include "matchcoordinator.h"
#include "matchrecordsink.h"

class AppActivityState;
class QMessageBox;
class JosekiBookSession;
class ConsecutiveGameSeries;
class CsaGameCoordinator;
class GameChannels;
class KifuDocument;
class KifuFileController;
class KifuNavigationState;
class LiveGameSession;
class MainWindowAppearanceController;
class NyugyokuDeclarationHandler;
class SessionResetCoordinator;
class ShogiView;
class TimeDisplayPresenter;

/**
 * @brief 対局の利用者の操作を受け、対局の棋譜を開始から終局まで受け持つ
 *
 * ローカル対局と通信対局の棋譜は、対局の棋譜の受け手（MatchRecordSink）として同じ1本の流れで扱う。
 * - 開始: 棋譜を整え、対局者・持ち時間・開始日時を文書に記録する（ローカル対局は startGame、
 *   通信対局は司令塔が beginRecord で頼む）
 * - 着手: 確定した行をライブ対局（分岐ツリー）に書く（appendRecord）
 * - 終局: ライブ対局を確定し、終了日時を記録して自動保存する（finishRecord）
 * 投了・中断・即指し・入玉宣言は対局相手の窓口（GameChannel）へ頼み、対局の種類で分岐しない。
 * 盤・棋譜欄・手番の表示は、ツリーと表示中の手が変わると各部品が自分で合わせる。
 */
class GameSession : public QObject, public MatchRecordSink
{
    Q_OBJECT

public:
    struct Deps {
        const AppActivityState* activity = nullptr;         ///< 動作状態
        const GameChannels* channels = nullptr;             ///< 対局相手の窓口（投了・中断・即指し・入玉宣言の送り先。必須）
        MatchCoordinator* match = nullptr;                  ///< ローカル対局の司令塔
        ConsecutiveGameSeries* consecutive = nullptr;  ///< 連続対局
        SessionResetCoordinator* reset = nullptr;           ///< 棋譜と表示の作り直し
        KifuNavigationState* navState = nullptr;            ///< 表示中の手（現在局面からの開始・再開）
        LiveGameSession* liveSession = nullptr;             ///< 対局の手順の書き先
        KifuDocument* document = nullptr;                   ///< 棋譜の文書（対局者・持ち時間・開始と終了の日時・対局情報）
        TimeDisplayPresenter* timePresenter = nullptr;      ///< 時計の表示
        KifuFileController* kifuFileController = nullptr;   ///< 未保存の棋譜の確認
        ShogiView* shogiView = nullptr;                     ///< 手前に表示している側
        MainWindowAppearanceController* appearance = nullptr; ///< 「人を手前に表示する」の盤の反転
        CsaGameCoordinator* csaGame = nullptr;              ///< 通信対局の司令塔（必須）
        NyugyokuDeclarationHandler* nyugyoku = nullptr;     ///< 利用者の入玉宣言
        /// 定跡ウィンドウで開いている定跡（エンジンが定跡を使う対局は、始めたときの写しを使う。省略可）
        const JosekiBookSession* josekiBook = nullptr;
    };

    explicit GameSession(const Deps& deps, QObject* parent = nullptr);
    ~GameSession() override;

    // --- MatchRecordSink（ローカル対局・通信対局の棋譜） ---
    /// 通信対局が始まった（サーバーの開始局面・対局者名・持ち時間で棋譜を作り直す）
    void beginRecord(const MatchRecordStart& start) override;
    bool canAppendRecord() const override;
    void appendRecord(const RecordEntry& entry) override;
    bool canUndoRecords(int count) const override;
    void undoRecords(int count) override;
    void finishRecord(bool autoSave) override;

    /// 計画どおりに対局を始める。beginSeries なら連続対局の1局目として局数を数え始める
    bool startGame(const GameStartPlan& plan, bool beginSeries);

signals:
    /// 対局（ローカル・通信）で指された手を棋譜に書いた（sfen は指す前の局面。定跡ウィンドウの「盤の手を登録」が受ける）
    void gameMoveRecorded(const QString& sfen, const QString& usiMove);
    /// ローカル対局を始める（連続対局の次局も。棋譜を作り直してエンジンを起動する前。前の対局の通信ログを消す）
    void gameStarting();

public slots:
    // --- 利用者の操作 ---
    /// 対局開始ダイアログを出し、選んだ条件で対局を始める
    void startGameFromDialog();
    void resign();
    void breakOff();
    void makeEngineMoveNow();
    /// 手番の人間が入玉宣言する（確認してから宣言する）
    void declareNyugyoku();
    void undoTwoMoves();
    void resumeInterruptedGame();
    /// 対局の後始末（「新規」）。通信対局を止め、終局の状態・連続対局・中断した対局を捨てる。
    /// 検討・エンジン・棋譜の作り直しは「新規」の流れ（MainWindowActions::startNewRecord）が行う
    void abandonGame();
    /// 中断した対局を再開できなくする（棋譜を変えたとき・別の対局を始めたとき）
    void discardInterruptedGame();

    // --- 司令塔から ---
    /// 終局した（棋譜は司令塔が finishRecord で確定済み。中断なら連続対局をやめる）
    void onGameEnded(const MatchCoordinator::GameEndInfo& info);
    void onGameEndProcessed(const MatchCoordinator::GameEndInfo& info);
    /// 終局の知らせを出す。エンジン同士の連続対局で次の局が残っていれば、閉じるのを待たずに次の局へ進め
    /// （通信対局と同じ）、前の局の知らせは閉じる。最後の局は閉じるまで待つ
    void showGameOverMessage(const QString& title, const QString& message);
    void onGameInterrupted();
    /// 連続対局の次局
    void startNextGame(const GameStartPlan& plan);

private:
    QString recordStartSfen() const;
    /// エンジン同士の連続対局で、次の局が残っているか
    bool seriesContinues() const;
    /// 対局開始ダイアログに出す、定跡ウィンドウで開いている定跡の名前（開いていなければ空）
    QString josekiBookName() const;
    /// 対局者を文書に記録し、対局情報（開始日時・持ち時間）を作る（ローカル対局・通信対局で共通）
    void beginMatchRecord(const MatchPlayers& players, const TimeControl& timeControl, bool continuesRecord);
    /// 読み込んだ棋譜の途中から続けた対局の対局者・開始日時（今）・持ち時間を、分岐の最初の手のメモにする
    void noteContinuedMatch(const MatchPlayers& players, const TimeControl& timeControl);

    Deps m_deps;
    MatchAutoSave m_autoSave;          ///< 進行中の対局の自動保存
    QPointer<QMessageBox> m_gameOverNotice;  ///< 待たずに出した連続対局の途中の局の終局の知らせ（次の知らせで閉じる）

};

#endif // GAMESESSION_H
