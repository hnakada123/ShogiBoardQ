#ifndef USI_H
#define USI_H

/// @file usi.h
/// @brief USIプロトコル通信ファサードクラスの定義

#include <QLoggingCategory>
#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QThread>
#include <QPointer>
#include <QTimer>
#include <memory>

Q_DECLARE_LOGGING_CATEGORY(lcEngine)

#include "engineprocessmanager.h"
#include "usiprotocolhandler.h"
#include "thinkinginfopresenter.h"
#include "shogigamecontroller.h"
#include "usicommlogmodel.h"
#include "shogienginethinkingmodel.h"
#include "playmode.h"

/**
 * @brief USIプロトコル通信を管理するファサードクラス
 *
 * このクラスは後方互換性のために元のUsiクラスのインターフェースを維持しながら、
 * 内部実装を以下の3つのクラスに委譲する:
 * - EngineProcessManager: QProcessの管理
 * - UsiProtocolHandler: USIプロトコルの送受信
 * - ThinkingInfoPresenter: GUIへの表示更新
 *
 * 設計上の注意:
 * - 単一責任原則（SRP）に基づくクラス分割
 * - シグナル/スロットによる疎結合な設計
 * - 既存コードとの後方互換性を維持
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class Usi : public QObject
{
    Q_OBJECT

public:
    // --- 定数定義（後方互換性のため維持）---
    
    static constexpr int SENTE_HAND_FILE = 10;
    static constexpr int GOTE_HAND_FILE = 11;
    static constexpr int BOARD_SIZE = 9;
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

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
                 ShogiGameController* algorithm, PlayMode& playMode, 
                 QObject* parent = nullptr);
    ~Usi() override;

    // --- 公開インターフェース（後方互換性のため維持）---
    
    QString scoreStr() const;
    bool isResignMove() const;
    bool isWinMove() const;           ///< bestmove win（入玉宣言勝ち）を受信したか
    int lastScoreCp() const;
    QString pvKanjiStr() const;
    void setPvKanjiStr(const QString& newPvKanjiStr);

    QString convertFirstPlayerPieceNumberToSymbol(int rankFrom) const;
    QString convertSecondPlayerPieceNumberToSymbol(int rankFrom) const;

    void parseMoveCoordinates(int& fileFrom, int& rankFrom, int& fileTo, int& rankTo);

    void initializeAndStartEngineCommunication(QString& engineFile, QString& enginename);

    void handleHumanVsEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                          QPoint& outFrom, QPoint& outTo,
                                          int byoyomiMilliSec, const QString& btime,
                                          const QString& wtime, QStringList& positionStrList,
                                          int addEachMoveMiliSec1, int addEachMoveMiliSec2,
                                          bool useByoyomi);

    void handleEngineVsHumanOrEngineMatchCommunication(QString& positionStr,
                                                       QString& positionPonderStr,
                                                       QPoint& outFrom, QPoint& outTo,
                                                       int byoyomiMilliSec, const QString& btime,
                                                       const QString& wtime,
                                                       int addEachMoveMiliSec1,
                                                       int addEachMoveMiliSec2, bool useByoyomi);

    void sendGameOverCommand(const QString& result);
    void sendQuitCommand();
    QChar rankToAlphabet(int rank) const;
    void sendGameOverWinAndQuitCommands();
    void sendStopCommand();
    void waitForStopOrPonderhitCommand();

    void executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec, int multiPV = 1);

    /// 検討用コマンド送信（非ブロッキング）
    /// position + go infinite を送信し、タイムアウト時に stop を送信するタイマーをセット
    /// bestmove の待機は行わず、シグナル経由で通知を受ける
    void sendAnalysisCommands(const QString& positionStr, int byoyomiMilliSec, int multiPV = 1);

    void setPreviousFileTo(int newPreviousFileTo);
    void setPreviousRankTo(int newPreviousRankTo);

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

    void sendGoCommand(int byoyomiMilliSec, const QString& btime, const QString& wtime,
                       int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);

    void sendGoDepthCommand(int depth);       ///< go depth <x> - 指定深さまで探索
    void sendGoNodesCommand(qint64 nodes);    ///< go nodes <x> - 指定ノード数まで探索
    void sendGoMovetimeCommand(int timeMs);   ///< go movetime <x> - 指定時間(ms)探索

    /// go searchmoves <move1> ... <movei> [infinite] - 指定した手のみを探索
    /// @param moves 探索対象の手（USI形式、例: "7g7f", "8h2b+"）
    /// @param infinite trueの場合 "go searchmoves ... infinite" を送信
    void sendGoSearchmovesCommand(const QStringList& moves, bool infinite = true);

    /// go searchmoves <move1> ... <movei> depth <x> - 指定した手のみを指定深さまで探索
    void sendGoSearchmovesDepthCommand(const QStringList& moves, int depth);

    /// go searchmoves <move1> ... <movei> movetime <x> - 指定した手のみを指定時間探索
    void sendGoSearchmovesMovetimeCommand(const QStringList& moves, int timeMs);

    void sendRaw(const QString& command) const;

    /// エンジンプロセスが起動中かどうかを返す
    bool isEngineRunning() const;

    void setThinkingModel(ShogiEngineThinkingModel* m);
    void setLogModel(UsiCommLogModel* m);

    /**
     * @brief 解析用の盤面データを初期化する
     * 
     * 棋譜解析モードで、エンジンに position + go コマンドを送る前に呼び出す。
     * ShogiGameControllerから現在の盤面データをクローンし、
     * ThinkingInfoPresenterに設定する。
     */
    void prepareBoardDataForAnalysis();

    /**
     * @brief 盤面データを直接設定する
     * 
     * 棋譜解析で各局面ごとに盤面データを更新するために使用。
     */
    void setClonedBoardData(const QVector<QChar>& boardData);

    /**
     * @brief 基準SFENを設定する（ThinkingInfoPresenterの手番設定用）
     * 
     * 棋譜解析で各局面ごとにThinkingInfoPresenterの手番を正しく設定するために使用。
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

    void sendPositionAndGoMate(const QString& sfen, int timeMs, bool infinite);
    void sendStopForMate();

    void startAndInitializeEngine(const QString& engineFile, const QString& enginename);

    void executeTsumeCommunication(QString& positionStr, int mateLimitMilliSec);
    void sendPositionAndGoMateCommands(int mateLimitMilliSec, QString& positionStr);
    void cancelCurrentOperation();

#ifdef QT_DEBUG
    ShogiEngineThinkingModel* debugThinkingModel() const;
    UsiCommLogModel* debugLogModel() const;
#endif

signals:
    void stopOrPonderhitCommandSent();                ///< stop/ponderhit送信完了（ProtocolHandler → 外部）
    void bestMoveResignReceived();                    ///< bestmove resign受信（ProtocolHandler → 外部）
    void bestMoveWinReceived();                       ///< bestmove win（入玉宣言勝ち）受信（ProtocolHandler → 外部）
    void sigTsumeCheckmate(const Usi::TsumeResult& result); ///< 詰探索結果通知
    void checkmateSolved(const QStringList& pvMoves); ///< 詰みあり（ProtocolHandler → TsumeSearchFlowController）
    void checkmateNoMate();                           ///< 詰みなし（ProtocolHandler → TsumeSearchFlowController）
    void checkmateNotImplemented();                   ///< 詰探索未対応（ProtocolHandler → TsumeSearchFlowController）
    void checkmateUnknown();                          ///< 詰探索結果不明（ProtocolHandler → TsumeSearchFlowController）
    void errorOccurred(const QString& message);       ///< エンジンエラー発生（ProcessManager → 外部）
    void usiOkReceived();                             ///< usiok受信完了（ProtocolHandler → 外部）
    void readyOkReceived();                           ///< readyok受信完了（ProtocolHandler → 外部）
    void bestMoveReceived();                          ///< bestmove受信（ProtocolHandler → 外部）
    void infoLineReceived(const QString& line);       ///< info行受信通知（ProtocolHandler → 外部）
    void thinkingInfoUpdated(const QString& time, const QString& depth,
                             const QString& nodes, const QString& score,
                             const QString& pvKanjiStr, const QString& usiPv,
                             const QString& baseSfen, int multipv, int scoreCp); ///< 思考情報更新（Presenter → 外部）

private:
    // --- 内部コンポーネント ---

    std::unique_ptr<EngineProcessManager> m_processManager;  ///< エンジンプロセス管理（所有）
    std::unique_ptr<UsiProtocolHandler> m_protocolHandler;    ///< USIプロトコル送受信（所有）
    std::unique_ptr<ThinkingInfoPresenter> m_presenter;       ///< GUI表示更新（所有）

    // --- 外部参照（QPointerで生存を追跡）---

    QPointer<UsiCommLogModel> m_commLogModel;        ///< USI通信ログモデルへの参照（非所有）
    QPointer<ShogiEngineThinkingModel> m_thinkingModel; ///< 思考情報モデルへの参照（非所有）
    ShogiGameController* m_gameController = nullptr;  ///< ゲームコントローラへの参照（非所有）
    PlayMode& m_playMode;                             ///< 現在の対局モード

    // --- 状態 ---

    int m_previousFileTo = 0;                        ///< 前回の指し手の移動先筋
    int m_previousRankTo = 0;                        ///< 前回の指し手の移動先段
    QString m_lastUsiMove;                           ///< 開始局面に至った最後の指し手（USI形式、読み筋ハイライト用）
    QVector<QChar> m_clonedBoardData;                ///< 読み筋変換用にクローンした盤面データ
    bool m_analysisMode = false;                     ///< 解析モード中かどうか
    bool m_gameoverSent = false;                     ///< gameover送信済みフラグ（重複送信防止）
    QString m_pvKanjiStr;                            ///< 読み筋の漢字表記
    QPointer<ShogiEngineThinkingModel> m_considerationModel; ///< 検討タブ用モデルへの参照（非所有）
    int m_considerationMaxMultiPV = 1;               ///< 検討タブの最大MultiPV値
    QTimer* m_analysisStopTimer = nullptr;           ///< 検討停止タイマー（所有、動的生成）

    // --- プライベートメソッド ---
    
    void setupConnections();
    bool changeDirectoryToEnginePath(const QString& engineFile);
    void cloneCurrentBoardData();
    void applyMovesToBoardFromBestMoveAndPonder();
    
    void executeEngineCommunication(QString& positionStr, QString& positionPonderStr,
                                    QPoint& outFrom, QPoint& outTo, int byoyomiMilliSec,
                                    const QString& btime, const QString& wtime,
                                    int addEachMoveMilliSec1, int addEachMoveMilliSec2,
                                    bool useByoyomi);
    
    void processEngineResponse(QString& positionStr, QString& positionPonderStr,
                               int byoyomiMilliSec, const QString& btime, const QString& wtime,
                               int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);
    
    void sendCommandsAndProcess(int byoyomiMilliSec, QString& positionStr,
                                const QString& btime, const QString& wtime,
                                QString& positionPonderStr, int addEachMoveMilliSec1,
                                int addEachMoveMilliSec2, bool useByoyomi);
    
    void startPonderingAfterBestMove(QString& positionStr, QString& positionPonderStr);
    void appendBestMoveAndStartPondering(QString& positionStr, QString& positionPonderStr);

    QString computeBaseSfenFromBoard() const;
    void updateBaseSfenForPonder();
    
    QString convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote);
    
    void waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec, const QString& btime,
                                              const QString& wtime, bool useByoyomi);

private slots:
    /// エンジンプロセスエラー時のクリーンアップ処理
    void onProcessError(QProcess::ProcessError error, const QString& message);
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
    /// 思考情報を思考タブ・検討タブ・外部へ中継する
    void onThinkingInfoUpdated(const QString& time, const QString& depth,
                               const QString& nodes, const QString& score,
                               const QString& pvKanjiStr, const QString& usiPv,
                               const QString& baseSfen, int multipv, int scoreCp);
};

#endif // USI_H
