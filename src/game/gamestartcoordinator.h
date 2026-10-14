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
 *  - 将棋時計の初期化を依頼シグナルで通知（具体的なセットはUI層に委譲）
 *  - 対局開始前のUI/内部状態クリアを依頼シグナルで通知
 *  - 必要に応じて初手go（エンジン初手）をトリガ
 *
 * @note 具体的なShogiClock APIは呼ばない（プロジェクト差異を吸収するため）。
 *       MainWindow側で既存ロジックをスロット接続する。
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

    /// 盤面ビュー操作コールバック群
    struct ViewHooks {
        std::function<void()> initializeToFlatStartingPosition;               ///< 平手初期局面を設定
        std::function<void()> removeHighlightAllData;                         ///< ハイライト全消去
        std::function<void(ShogiGameController*, const QString&)> applyResumePosition; ///< 再開SFEN適用
        std::function<void(const QString&, const QString&, bool)> addKifuHeaderItem;   ///< 棋譜ヘッダ行追加（move, time, prepend）
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

    /// 開始前の適用（時計/人手前など）を依頼するための軽量入力
    struct Request {
        QString          startSfen;          ///< "startpos ..." or "<sfen> [b|w] ..."
        StartGameDialogData dialogData;      ///< ダイアログから抽出した持ち時間等の設定
        ShogiClock*      clock = nullptr;    ///< 時計への参照（任意）
        bool             skipCleanup = false;///< trueならクリーンアップをスキップ（呼び出し済みの場合）
    };

    /// initializeGame等の段階実行APIに渡すコンテキスト
    struct Ctx {
        ShogiGameController*   gc = nullptr;                 ///< ゲームコントローラ
        ShogiClock*            clock = nullptr;              ///< 時計
        StartGameDialogData    dialogData;                   ///< 対局開始ダイアログから抽出したデータ
        QString*               startSfenStr = nullptr;       ///< 開始SFEN文字列への参照
        QString*               currentSfenStr = nullptr;     ///< 現在SFEN文字列への参照

        int   selectedPly = -1;                              ///< 選択中の手数

        KifuRecordListModel* kifuModel  = nullptr;           ///< 棋譜欄モデル
        QStringList*         sfenRecord = nullptr;           ///< SFEN履歴リスト

        bool                 isReplayMode = false;           ///< 再生モード中は時計を動かさない

        qint64*              initialTimeP1MsOut = nullptr;   ///< 先手初期ms出力先（NULLなら無視）
        qint64*              initialTimeP2MsOut = nullptr;   ///< 後手初期ms出力先（NULLなら無視）

        Turn bottomSide = Turn::Black;                       ///< 盤の手前に表示している側
    };

    // --- メインAPI ---

    explicit GameStartCoordinator(const Deps& deps, QObject* parent = nullptr);

    /// StartOptionsを受け取り対局を開始する
    void start(const StartParams& params);
    /// 時計を設定し直さず、中断時の状態から開始通知・探索を再開する。
    bool resumeInterruptedGame();

    /// 開始前の時計/表示ポリシーを適用する
    void prepare(const Request& req);

    // --- 段階実行API ---

    /// 現在局面から開始する場合のデータ準備
    void prepareDataCurrentPosition(const Ctx& c);

    /// 初期局面（平手／手合割）で開始する場合の準備
    void prepareInitialPosition(const Ctx& c);

    /// 対局開始フローを実行する。不正な開始局面では状態を変更せずエラーを返す。
    bool initializeGame(const Ctx& c, QString& errorMessage);

    /// 時計を設定し対局を開始する
    void setTimerAndStart(const Ctx& c);

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

    /// 盤面ビュー操作コールバックを設定する
    void setViewHooks(const ViewHooks& hooks);

signals:
    // --- 開始前フック ---

    /// UI/状態を初期化してほしい（→ SessionLifecycleCoordinator::performPreStartCleanup → PreStartCleanupHandler::performCleanup）
    void requestPreStartCleanup();

    /// 時計適用の依頼（→ SessionLifecycleCoordinator::applyTimeControl）
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

    /// 分岐ツリーの完全リセットを依頼（平手・駒落ちから新規対局を始めるとき）
    void requestBranchTreeResetForNewGame();

private:
    /// 開始の通知・開始行の選択・時計の始動・初手を行う（start と resumeInterruptedGame の共通部分）
    void finishStart(const MatchCoordinator::StartOptions& opt, int selectKifuRow, bool startClockAndFirstMove);
    bool validate(const StartParams& params, QString& whyNot) const;
    bool validateKingsForGame(const QString& sfen, QString& whyNot) const;

    QPointer<MatchCoordinator> m_match;       ///< 対局進行の司令塔（非所有、MCW が所有）
    ViewHooks            m_viewHooks;        ///< 盤面ビュー操作コールバック群
};

Q_DECLARE_METATYPE(GameStartCoordinator::TimeSettings)
Q_DECLARE_METATYPE(GameStartCoordinator::Request)

#endif // GAMESTARTCOORDINATOR_H
