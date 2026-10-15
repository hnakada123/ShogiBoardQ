#ifndef GAMESTARTCOORDINATOR_H
#define GAMESTARTCOORDINATOR_H

/// @file gamestartcoordinator.h
/// @brief 対局開始コーディネータクラスの定義

#include <QObject>

#include "timecontrol.h"
#include <QPointer>
#include <QString>

#include "matchcoordinator.h"
#include "startgamedatabridge.h"

class ShogiClock;
class ShogiGameController;

/**
 * @brief 対局開始フローを司るコーディネータ
 *
 * 責務:
 *  - StartOptionsを受け取り、MatchCoordinatorへ対局開始を指示する
 *  - 将棋時計の設定を適用し、表示層へ通知する
 *  - 対局開始前のUI/内部状態クリアを依頼シグナルで通知
 *  - 必要に応じて初手go（エンジン初手）をトリガ
 *
 *
 */
class GameStartCoordinator : public QObject
{
    Q_OBJECT
public:
    // --- 型定義 ---

    /// 依存オブジェクト
    struct Deps {
        MatchCoordinator*     match  = nullptr;  ///< 対局進行の司令塔（必須）
    };

    /// 対局開始時の時間の設定
    struct TimeSettings {
        bool enabled = false;      ///< 時間制御が有効か
        TimeControl rules;         ///< 先手・後手の持ち時間の決まり
        bool loseOnTimeout = true; ///< 時間切れを負けとするか（時計の有効/無効とは独立）
    };

    /// 対局開始パラメータ
    struct StartParams {
        MatchCoordinator::StartOptions opt;  ///< 既存のStartOptionsをそのまま注入
        TimeSettings                   tc;   ///< 時計適用のために併送
        bool autoStartEngineMove = true;     ///< 時計を動かし、先手がエンジンなら初手goを起動
        int selectKifuRow = -1;              ///< 開始後に選ぶ棋譜欄の行（現在局面から始めるとき。-1 なら選ばない）
        bool announcePlayerNames = false;    ///< 対局者名を通知する（ダイアログを経ない連続対局の2局目以降）
    };

    /// 対局開始に必要な入力と、検証後に更新する表示用SFEN
    struct Ctx {
        ShogiClock*            clock = nullptr;              ///< 時計
        StartGameDialogData    dialogData;                   ///< 対局開始ダイアログから抽出したデータ
        QString*               startSfenStr = nullptr;       ///< 開始SFEN文字列への参照
        QString*               currentSfenStr = nullptr;     ///< 現在SFEN文字列への参照

        RecordLine recordLine;                              ///< 選択した手順のスナップショット
        int startNodeId = -1;                                ///< 分岐ツリー上の選択ノード

        Turn bottomSide = Turn::Black;                       ///< 盤の手前に表示している側
    };

    // --- メインAPI ---

    explicit GameStartCoordinator(const Deps& deps, QObject* parent = nullptr);

    /// StartOptionsを受け取り対局を開始する
    void start(const StartParams& params);
    /// 時計を設定し直さず、中断時の状態から開始通知・探索を再開する。
    bool resumeInterruptedGame();

    /// 対局開始フローを実行する。不正な開始局面では状態を変更せずエラーを返す。
    bool initializeGame(const Ctx& c, QString& errorMessage);

    // --- ユーティリティ（GameStartOptionsBuilder へ委譲） ---

    /// ダイアログ状態からPlayModeを決定する
    PlayMode setPlayMode(const Ctx& c) const;

    /// SFENの手番とダイアログ設定を整合させてPlayModeを決定する
    static PlayMode determinePlayModeAlignedWithTurn(
        int initPositionNumber,
        bool isPlayer1Human,
        bool isPlayer2Human,
        const QString& startSfen
        );


signals:
    // --- 開始前フック ---

    /// UI/状態を初期化してほしい（→ SessionLifecycleCoordinator::performPreStartCleanup → PreStartCleanupHandler::performCleanup）
    void requestPreStartCleanup(const GameStartPreparation& preparation);

    /// 時計の設定適用後の通知（→ SessionLifecycleCoordinator::applyTimeControl）
    void requestApplyTimeControl(const GameStartCoordinator::TimeSettings& tc);

    // --- 進捗通知 ---

    /// 対局開始完了の通知（→ UiStatePolicyManager::transitionToDuringGame、SessionLifecycleCoordinator::handleGameStarted、
    /// GameSessionOrchestrator::discardInterruptedGame の順）
    void started(const MatchCoordinator::StartOptions& opt);

    /// 対局者名が確定した時に発行される（→ PlayerInfoWiring::onMenuPlayerNamesResolved）
    void playerNamesResolved(const QString& human1, const QString& human2,
                             const QString& engine1, const QString& engine2,
                             int playMode);

    /// 連続対局設定（EvE対局時のみ、totalGames=合計数, switchTurn=手番入替）
    void consecutiveGamesConfigured(int totalGames, bool switchTurn);

    /// 対局開始後に棋譜欄の指定行を選択する（現在局面から開始時に使用）
    void requestSelectKifuRow(int row);


private:
    /// 開始の通知・開始行の選択・時計の始動・初手を行う（start と resumeInterruptedGame の共通部分）
    void finishStart(const MatchCoordinator::StartOptions& opt, int selectKifuRow, bool startClockAndFirstMove);
    bool validate(const StartParams& params, QString& whyNot) const;
    bool validateKingsForGame(const QString& sfen, QString& whyNot) const;

    QPointer<MatchCoordinator> m_match;       ///< 対局進行の司令塔（非所有、MCW が所有）
};

Q_DECLARE_METATYPE(GameStartCoordinator::TimeSettings)

#endif // GAMESTARTCOORDINATOR_H
