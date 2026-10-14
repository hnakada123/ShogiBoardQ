#ifndef USI_H
#define USI_H

/// @file usi.h
/// @brief USIプロトコル通信ファサードクラスの定義

#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QList>
#include <QPointer>
#include <QTimer>
#include <functional>
#include <vector>
#include <memory>

#include "logcategories.h"
#include "usitimingparams.h"
#include "usimove.h"

#include "engineprocessmanager.h"
#include "usienginesession.h"
#include "usiprotocolhandler.h"
#include "thinkinginfopresenter.h"
#include "shogigamecontroller.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "thinkinginforow.h"
#include "usiinfo.h"

class UsiMatchHandler;
class ShogiClock;

/**
 * @brief GUI から使う USI エンジン1台の窓口
 *
 * 対局・検討・棋譜解析・詰み探索の要求を受け、次のクラスに委譲する:
 * - UsiEngineSession / EngineProcessManager: プロセスと初期化（usi〜readyok）
 * - UsiProtocolHandler: USIコマンドの送信と受信行の解釈（info は UsiInfo にする）
 * - ThinkingInfoPresenter: UsiInfo を思考タブの行（ThinkingInfoRow）にする
 * - UsiMatchHandler: 対局の position/go/bestmove
 *
 * 送信は用途ごとの型付きメソッドを使う。任意の文字列を送れるのは、利用者が入力する
 * USI コンソール（sendConsoleCommand）だけにする。
 */
class Usi : public QObject
{
    Q_OBJECT

public:
    const UsiProtocolHandler::SearchCandidate& searchCandidate() const
    { return m_protocolHandler->searchCandidate(); }
    void setMatchClock(ShogiClock* clock) { m_matchClock = clock; }
    /// 詰み探索結果
    struct TsumeResult {
        enum Kind {
            Solved,          ///< 詰みあり
            NoMate,          ///< 詰みなし
            NotImplemented,  ///< エンジンが詰探索未対応
            Unknown          ///< 不明（初期値）
        } kind = Unknown;
        QStringList pvMoves; ///< 詰み手順（USI形式）
    };

    // --- 構築・破棄 ---
    
    explicit Usi(UsiCommLogModel* model, ShogiEngineThinkingModel* modelThinking,
                 ShogiGameController* algorithm,
                 QObject* parent = nullptr);
    ~Usi() override;

    // --- 状態の参照 ---

    QString scoreStr() const;
    bool isResignMove() const;
    bool isWinMove() const;           ///< bestmove win（入玉宣言勝ち）を受信したか
    int lastScoreCp() const;
    QString lastScoreMate() const;

    void initializeAndStartEngineCommunication(QString& engineFile, QString& enginename);

    void sendGameOverCommand(GameOverResult result);
    void sendQuitCommand();
    void sendGameOverWinAndQuitCommands();
    void sendStopCommand();
    /// 対局中の「すぐ指させる」。準備中に保留した position・go は残し、それを送った後で stop を送る
    void requestImmediateMove();

    /// 検討用コマンド送信（非ブロッキング）
    /// position + go infinite を送信し、タイムアウト時に stop を送信するタイマーをセット
    /// bestmove の待機は行わず、シグナル経由で通知を受ける
    void sendAnalysisCommands(const QString& positionStr, int byoyomiMilliSec, int multiPV = 1);

    /// 棋譜解析の1局面: position を送り、go infinite で探索を始める（停止は sendStopCommand）
    void sendPositionAndGoInfinite(const QString& positionStr);

    /// setoption name <name> value <value> を送る（初期化中なら完了後に送る）
    void sendSetOptionCommand(const QString& name, const QString& value);

    /// 直前の指し手の行き先（読み筋の最初の手の「同」表記に使う。無ければ {0, 0}）
    void setPreviousMove(BoardSquare to);

    // 開始局面に至った最後の指し手（USI形式）を設定する。
    // 読み筋表示ウィンドウでのハイライト用。
    void setLastUsiMove(const QString& move);

    qint64 lastBestmoveElapsedMs() const;

    void sendGameOverLoseAndQuitCommands();

    void setLogIdentity(const QString& engineTag, const QString& sideTag,
                        const QString& engineName = QString());

    void setSquelchResignLogging(bool on);

    /// 検討タブ用モデルを設定する
    /// このモデルはMultiPVモード（updateByMultipv）で更新される
    void setConsiderationModel(ShogiEngineThinkingModel* model, int maxMultiPV = 1);

    /// 検討中にMultiPVを変更する
    void updateConsiderationMultiPV(int multiPV);

    void resetResignNotified();
    void resetWinNotified();          ///< 入玉宣言勝ち通知フラグをリセット
    void markHardTimeout();
    void clearHardTimeout();
    bool isIgnoring() const;

    /// 現在実行中のエンジンファイルパスを取得
    QString currentEnginePath() const;

    /// 利用者が USI コンソールに入力したコマンドをそのまま送る。アプリの処理は型付きの送信メソッドを使う
    void sendConsoleCommand(const QString& command);

    /// エンジンプロセスが起動中かどうかを返す
    bool isEngineRunning() const;

    void setThinkingModel(ShogiEngineThinkingModel* m);
    void setLogModel(UsiCommLogModel* m);

    /**
     * @brief 基準SFENを設定する（読み筋を日本語にする局面）
     * 
     * 棋譜解析で各局面ごとに、読み筋を変換する局面と手番を ThinkingInfoPresenter に設定するために使用。
     */
    void setBaseSfen(const QString& sfen);

    /**
     * @brief ThinkingInfoPresenterのバッファをフラッシュする
     * 
     * 棋譜解析でbestmove受信前に漢字PVを確定させるために使用。
     */
    void flushThinkingInfoBuffer();

    /**
     * @brief 思考情報をクリアする
     * 
     * 棋譜解析で各局面の解析開始前に思考タブをクリアするために使用。
     */
    void requestClearThinkingInfo();

    void cleanupEngineProcessAndThread(bool clearThinking = true);

    /// 要求受付を返す。実際の初期化完了は engineInitialized、失敗は errorOccurred で通知する。
    [[nodiscard]] bool startAndInitializeEngineAsync(const QString& engineFile, const QString& enginename);
    bool isInitializing() const { return m_session && m_session->isInitializing(); }
    void requestMatchMove(const QString& position, const QString& ponder, const UsiTimingParams& timing);
    /// 人間の指し手を局面に加えて、エンジンに次の手を考えさせる
    void requestHumanReply(QString& position, const QString& ponder, const UsiMove& humanMove,
                           const UsiTimingParams& timing, QStringList& history);

    void executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec);
    void sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr);
    void cancelCurrentOperation();

signals:
    void searchCandidateChanged();
    void engineInitialized();
    /// 対局の思考で指し手が決まった（成り・不成を含む USI の指し手）
    void matchMoveReady(const UsiMove& move, const QString& position, const QString& ponder);
    void bestMoveResignReceived();                    ///< bestmove resign受信（ProtocolHandler → 外部）
    void bestMoveWinReceived();                       ///< bestmove win（入玉宣言勝ち）受信（ProtocolHandler → 外部）
    void checkmateSolved(const QStringList& pvMoves); ///< 詰みあり（ProtocolHandler → TsumeSearchFlowController）
    void checkmateNoMate();                           ///< 詰みなし（ProtocolHandler → TsumeSearchFlowController）
    void checkmateNotImplemented();                   ///< 詰探索未対応（ProtocolHandler → TsumeSearchFlowController）
    void checkmateUnknown();                          ///< 詰探索結果不明・時間切れ（ProtocolHandler → TsumeSearchFlowController）
    void errorOccurred(const QString& message);       ///< エンジンエラー発生（ProcessManager → 外部）
    void usiOkReceived();                             ///< usiok受信完了（ProtocolHandler → 外部）
    void readyOkReceived();                           ///< readyok受信完了（ProtocolHandler → 外部）
    void bestMoveReceived();                          ///< bestmove受信（ProtocolHandler → 外部）
    void infoReceived(const UsiInfo& info);           ///< info受信通知（ProtocolHandler → AnalysisFlowController）
    void thinkingInfoUpdated(const ThinkingInfoRow& row); ///< 思考情報更新（Presenter → 外部）

private:
    bool deferUntilReady(std::function<void()> action) const;
    quint64 m_asyncGeneration = 0;
    mutable std::vector<std::function<void()>> m_pendingActions;

    // --- 内部コンポーネント ---

    UsiEngineSession* m_session = nullptr; ///< GUI/CLI 共通の接続ライフサイクル
    EngineProcessManager* m_processManager = nullptr; ///< session 所有
    UsiProtocolHandler* m_protocolHandler = nullptr; ///< session 所有
    std::unique_ptr<ThinkingInfoPresenter> m_presenter;       ///< GUI表示更新（所有）
    std::unique_ptr<UsiMatchHandler> m_matchHandler;          ///< 対局通信・盤面データ管理（所有）

    // --- 外部参照（QPointerで生存を追跡）---

    QPointer<UsiCommLogModel> m_commLogModel;        ///< USI通信ログモデルへの参照（非所有）
    QPointer<ShogiEngineThinkingModel> m_thinkingModel; ///< 思考情報モデルへの参照（非所有）
    ShogiGameController* m_gameController = nullptr;  ///< ゲームコントローラへの参照（非所有）
    ShogiClock* m_matchClock = nullptr; ///< ローカル対局の時間判定元（非所有）

    // --- 状態 ---

    BoardSquare m_previousMove;                      ///< 直前の指し手の行き先
    bool m_gameoverSent = false;                     ///< gameover送信済みフラグ（重複送信防止）
    QPointer<ShogiEngineThinkingModel> m_considerationModel; ///< 検討タブ用モデルへの参照（非所有）
    int m_considerationMaxMultiPV = 1;               ///< 検討タブの最大MultiPV値
    QTimer* m_analysisStopTimer = nullptr;           ///< 検討停止タイマー（所有、動的生成）

    // --- プライベートメソッド ---

    void setupConnections();
    void onMatchBestMoveReceived();
    void resetAnalysisStopTimer();
    void prepareAnalysisSession(const QString& positionStr, int multiPV);

private slots:
    void onProtocolError(const QString& message);
    void onEngineInitialized();
    void onCommandSent(const QString& command);
    void onDataReceived(const QString& line);
    void onStderrReceived(const QString& line);

    // --- モデル更新スロット（Presenter → CommLogModel/ThinkingModel転送）---
    void onSearchedMoveUpdated(const QString& move);
    void onSearchDepthUpdated(const QString& depth);
    void onNodeCountUpdated(const QString& nodes);
    void onNpsUpdated(const QString& nps);
    void onHashUsageUpdated(const QString& hashUsage);
    void onCommLogAppended(const QString& log);
    void onClearThinkingInfoRequested();
    /// 検討停止タイマー発火時の処理（sendAnalysisCommands用）
    void onConsiderationStopTimeout();
    /// 思考情報を思考タブ・検討タブ・外部へ中継する
    void onThinkingInfoUpdated(const ThinkingInfoRow& row);
};

#endif // USI_H
