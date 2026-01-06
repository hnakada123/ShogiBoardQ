#ifndef USI_H
#define USI_H

#include <QObject>
#include <QPoint>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QThread>
#include <QPointer>
#include <memory>

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
 */
class Usi : public QObject
{
    Q_OBJECT

public:
    // === 定数定義（後方互換性のため維持）===
    
    static constexpr int SENTE_HAND_FILE = 10;
    static constexpr int GOTE_HAND_FILE = 11;
    static constexpr int BOARD_SIZE = 9;
    static constexpr int NUM_BOARD_SQUARES = BOARD_SIZE * BOARD_SIZE;

    /// 詰み探索結果
    struct TsumeResult {
        enum Kind { Solved, NoMate, NotImplemented, Unknown } kind = Unknown;
        QStringList pvMoves;
    };

    // === コンストラクタ・デストラクタ ===
    
    explicit Usi(UsiCommLogModel* model, ShogiEngineThinkingModel* modelThinking,
                 ShogiGameController* algorithm, PlayMode& playMode, 
                 QObject* parent = nullptr);
    ~Usi() override;

    // === 公開インターフェース（後方互換性のため維持）===
    
    QString scoreStr() const;
    bool isResignMove() const;
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

    void executeAnalysisCommunication(QString& positionStr, int byoyomiMilliSec);

    void setPreviousFileTo(int newPreviousFileTo);
    void setPreviousRankTo(int newPreviousRankTo);

    qint64 lastBestmoveElapsedMs() const;

    void sendGameOverLoseAndQuitCommands();

    void setLogIdentity(const QString& engineTag, const QString& sideTag,
                        const QString& engineName = QString());

    void setSquelchResignLogging(bool on);

    void resetResignNotified();
    void markHardTimeout();
    void clearHardTimeout();
    bool isIgnoring() const;

    void sendGoCommand(int byoyomiMilliSec, const QString& btime, const QString& wtime,
                       int addEachMoveMilliSec1, int addEachMoveMilliSec2, bool useByoyomi);

    void sendRaw(const QString& command) const;

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

    void cleanupEngineProcessAndThread();

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
    void stopOrPonderhitCommandSent();
    void bestMoveResignReceived();
    void sigTsumeCheckmate(const Usi::TsumeResult& result);
    void checkmateSolved(const QStringList& pvMoves);
    void checkmateNoMate();
    void checkmateNotImplemented();
    void checkmateUnknown();
    void errorOccurred(const QString& message);
    void usiOkReceived();
    void readyOkReceived();
    void bestMoveReceived();
    void infoLineReceived(const QString& line);  // info行受信通知
    void thinkingInfoUpdated(const QString& time, const QString& depth,
                             const QString& nodes, const QString& score,
                             const QString& pvKanjiStr, const QString& usiPv,
                             const QString& baseSfen);

private:
    // === 内部コンポーネント ===
    
    std::unique_ptr<EngineProcessManager> m_processManager;
    std::unique_ptr<UsiProtocolHandler> m_protocolHandler;
    std::unique_ptr<ThinkingInfoPresenter> m_presenter;

    // === 外部参照（QPointerで生存を追跡）===
    
    QPointer<UsiCommLogModel> m_commLogModel;
    QPointer<ShogiEngineThinkingModel> m_thinkingModel;
    ShogiGameController* m_gameController = nullptr;
    PlayMode& m_playMode;

    // === 状態 ===
    
    int m_previousFileTo = 0;
    int m_previousRankTo = 0;
    QVector<QChar> m_clonedBoardData;
    bool m_analysisMode = false;
    bool m_gameoverSent = false;
    QString m_pvKanjiStr;

    // === プライベートメソッド ===
    
    void setupConnections();
    void changeDirectoryToEnginePath(const QString& engineFile);
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
    
    QString convertHumanMoveToUsiFormat(const QPoint& outFrom, const QPoint& outTo, bool promote);
    
    void waitAndCheckForBestMoveRemainingTime(int byoyomiMilliSec, const QString& btime,
                                              const QString& wtime, bool useByoyomi);

private slots:
    void onProcessError(QProcess::ProcessError error, const QString& message);
    void onCommandSent(const QString& command);
    void onDataReceived(const QString& line);
    void onStderrReceived(const QString& line);
    
    // === モデル更新スロット（シグナル経由で更新）===
    void onSearchedMoveUpdated(const QString& move);
    void onSearchDepthUpdated(const QString& depth);
    void onNodeCountUpdated(const QString& nodes);
    void onNpsUpdated(const QString& nps);
    void onHashUsageUpdated(const QString& hashUsage);
    void onCommLogAppended(const QString& log);
    void onClearThinkingInfoRequested();
    void onThinkingInfoUpdated(const QString& time, const QString& depth,
                               const QString& nodes, const QString& score,
                               const QString& pvKanjiStr, const QString& usiPv,
                               const QString& baseSfen);
};

#endif // USI_H
