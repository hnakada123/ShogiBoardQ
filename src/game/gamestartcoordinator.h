#ifndef GAMESTARTCOORDINATOR_H
#define GAMESTARTCOORDINATOR_H

/// @file gamestartcoordinator.h
/// @brief 対局開始コーディネータクラスの定義

#include <QObject>
#include <QPointer>
#include <QString>

#include "matchcoordinator.h"
#include "startgamedatabridge.h"

class QWidget;
class ShogiClock;
class ShogiGameController;
class KifuLoadCoordinator;

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
        ShogiClock*           clock  = nullptr;  ///< 時計（任意、時間適用リクエストを出すだけ）
        ShogiGameController*  gc     = nullptr;  ///< ゲームコントローラ（任意、開始前の状態同期用）
    };

    /// 盤面ビュー操作コールバック群
    struct ViewHooks {
        std::function<void()> initializeToFlatStartingPosition;               ///< 平手初期局面を設定
        std::function<void()> removeHighlightAllData;                         ///< ハイライト全消去
        std::function<void(ShogiGameController*, const QString&)> applyResumePosition; ///< 再開SFEN適用
        std::function<void(const QString&, const QString&, bool)> addKifuHeaderItem;   ///< 棋譜ヘッダ行追加（move, time, prepend）
    };

    /// 片方の対局者の時間設定（ミリ秒単位）
    struct TimeSide {
        qint64 baseMs      = 0;  ///< 持ち時間
        qint64 byoyomiMs   = 0;  ///< 秒読み
        qint64 incrementMs = 0;  ///< フィッシャー加算
    };

    /// 時間制御の設定
    struct TimeControl {
        bool     enabled = false;  ///< 時間制御が有効か
        TimeSide p1;               ///< 先手の時間設定
        TimeSide p2;               ///< 後手の時間設定
    };

    /// 対局開始パラメータ
    struct StartParams {
        MatchCoordinator::StartOptions opt;  ///< 既存のStartOptionsをそのまま注入
        TimeControl                    tc;   ///< 時計適用のために併送
        bool autoStartEngineMove = true;     ///< 先手がエンジンなら初手goを起動
    };

    /// 開始前の適用（時計/人手前など）を依頼するための軽量入力
    struct Request {
        int              mode = 0;           ///< PlayModeをintで受ける（enum依存を避ける）
        QString          startSfen;          ///< "startpos ..." or "<sfen> [b|w] ..."
        bool             bottomIsP1 = true;  ///< 手前が先手か
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
        bool  resumeFromCurrent = true;                      ///< 現在局面から再開するか

        KifuRecordListModel* kifuModel  = nullptr;           ///< 棋譜欄モデル
        QStringList*         sfenRecord = nullptr;           ///< SFEN履歴リスト
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;  ///< 分岐構造の設定用

        bool                 isReplayMode = false;           ///< 再生モード中は時計を動かさない

        qint64*              initialTimeP1MsOut = nullptr;   ///< 先手初期ms出力先（NULLなら無視）
        qint64*              initialTimeP2MsOut = nullptr;   ///< 後手初期ms出力先（NULLなら無視）

        bool bottomIsP1 = true;                              ///< 手前が先手か
    };

    // --- メインAPI ---

    explicit GameStartCoordinator(const Deps& deps, QObject* parent = nullptr);

    /// StartOptionsを受け取り対局を開始する
    void start(const StartParams& params);

    /// 開始前の時計/表示ポリシーを適用する
    void prepare(const Request& req);

    // --- 段階実行API ---

    /// 現在局面から開始する場合のデータ準備
    void prepareDataCurrentPosition(const Ctx& c);

    /// 初期局面（平手／手合割）で開始する場合の準備
    void prepareInitialPosition(const Ctx& c);

    /// ダイアログ表示→対局開始までの一連のフローを実行する
    void initializeGame(const Ctx& c);

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

    /// MatchCoordinator ポインタを外部（MCW）からセットする
    void setMatch(MatchCoordinator* match);

    /// 盤面ビュー操作コールバックを設定する
    void setViewHooks(const ViewHooks& hooks);

signals:
    // --- 開始前フック ---

    /// UI/状態を初期化してほしい（PreStartCleanupHandler::performCleanupに接続）
    void requestPreStartCleanup();

    /// 時計適用の依頼（MainWindow側の既存ロジックへ接続）
    void requestApplyTimeControl(const GameStartCoordinator::TimeControl& tc);

    // --- 進捗通知 ---

    /// 対局開始完了の通知（→ MainWindow::disableNavigationForGame, onGameStarted）
    void started(const MatchCoordinator::StartOptions& opt);

    /// 対局者名が確定した時に発行される（GameStartCoordinator → MainWindow）
    void playerNamesResolved(const QString& human1, const QString& human2,
                             const QString& engine1, const QString& engine2,
                             int playMode);

    // --- MatchCoordinatorシグナルの転送 ---

    /// 時計更新（MatchCoordinator::timeUpdated の再送出）
    void timeUpdated(qint64 p1ms, qint64 p2ms, bool p1turn, qint64 urgencyMs);

    /// 終局手の棋譜追記要求（MatchCoordinator::requestAppendGameOverMove の再送出）
    void requestAppendGameOverMove(const MatchCoordinator::GameEndInfo& info);

    /// 盤面反転通知（MatchCoordinator::boardFlipped の再送出）
    void boardFlipped(bool nowFlipped);

    /// 終局状態変更通知（MatchCoordinator::gameOverStateChanged の再送出）
    void gameOverStateChanged(const MatchCoordinator::GameOverState& st);

    /// 対局終了通知（MatchCoordinator::gameEnded の再送出）
    void matchGameEnded(const MatchCoordinator::GameEndInfo& info);

    /// 連続対局設定（EvE対局時のみ、totalGames=合計数, switchTurn=手番入替）
    void consecutiveGamesConfigured(int totalGames, bool switchTurn);

    /// 対局開始後に棋譜欄の指定行を選択する（現在局面から開始時に使用）
    void requestSelectKifuRow(int row);

    /// 分岐ツリーの完全リセットを依頼（kifuLoadCoordinator が null の場合用）
    void requestBranchTreeResetForNewGame();

private:
    bool validate(const StartParams& params, QString& whyNot) const;

    QPointer<MatchCoordinator> m_match;       ///< 対局進行の司令塔（非所有、再生成追跡）
    ShogiClock*          m_clock = nullptr;  ///< 将棋時計（非所有）
    ShogiGameController* m_gc    = nullptr;  ///< ゲームコントローラ（非所有）
    ViewHooks            m_viewHooks;        ///< 盤面ビュー操作コールバック群
};

Q_DECLARE_METATYPE(GameStartCoordinator::TimeControl)
Q_DECLARE_METATYPE(GameStartCoordinator::Request)

#endif // GAMESTARTCOORDINATOR_H
