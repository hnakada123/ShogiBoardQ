#ifndef PRESTARTCLEANUPHANDLER_H
#define PRESTARTCLEANUPHANDLER_H

#include <QObject>
#include <QString>
#include <QHash>
#include <QMap>

class BoardInteractionController;
class ShogiView;
class KifuRecordListModel;
class KifuBranchListModel;
class UsiCommLogModel;
class TimeControlController;
class EvaluationChartWidget;
class EvaluationGraphController;
class RecordPane;
class LiveGameSession;
class KifuBranchTree;
class KifuBranchNode;
class KifuNavigationState;

/**
 * @brief 対局開始前のクリーンアップ処理を担当するクラス
 *
 * 責務:
 * - 盤面/ハイライトのビジュアル初期化
 * - 「現在の局面から開始」判定
 * - 棋譜モデルの部分削除または全消去
 * - 手数トラッキング更新
 * - 分岐モデルクリア
 * - USIログ初期化
 * - 時間制御リセット
 *
 * GameStartCoordinatorからの requestPreStartCleanup シグナルに応じて
 * クリーンアップ処理を実行する。
 */
class PreStartCleanupHandler : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief 依存オブジェクト構造体
     */
    struct Dependencies {
        BoardInteractionController* boardController = nullptr;
        ShogiView* shogiView = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        KifuBranchListModel* kifuBranchModel = nullptr;
        UsiCommLogModel* lineEditModel1 = nullptr;
        UsiCommLogModel* lineEditModel2 = nullptr;
        TimeControlController* timeController = nullptr;
        EvaluationChartWidget* evalChart = nullptr;
        EvaluationGraphController* evalGraphController = nullptr;
        RecordPane* recordPane = nullptr;
        
        // MainWindowの状態変数への参照
        QString* startSfenStr = nullptr;
        QString* currentSfenStr = nullptr;
        int* activePly = nullptr;
        int* currentSelectedPly = nullptr;
        int* currentMoveIndex = nullptr;

        // 分岐ツリー関連
        LiveGameSession* liveGameSession = nullptr;
        KifuBranchTree* branchTree = nullptr;
        KifuNavigationState* navState = nullptr;
    };

    /**
     * @brief コンストラクタ
     * @param deps 依存オブジェクト
     * @param parent 親オブジェクト
     */
    explicit PreStartCleanupHandler(const Dependencies& deps, QObject* parent = nullptr);

    /**
     * @brief デストラクタ
     */
    ~PreStartCleanupHandler() override = default;

public slots:
    /**
     * @brief 対局開始前のクリーンアップを実行
     *
     * GameStartCoordinator::requestPreStartCleanup シグナルに接続する。
     */
    void performCleanup();

signals:
    /**
     * @brief コメント欄クリアを要求
     * @param text 空文字列
     * @param asHtml HTMLフラグ
     */
    void broadcastCommentRequested(const QString& text, bool asHtml);

public:
    /**
     * @brief 「現在の局面から開始」判定の共通ロジック
     *
     * テストで使用するためヘッダで公開する。
     */
    static bool shouldStartFromCurrentPosition(const QString& startSfen,
                                               const QString& currentSfen,
                                               int selectedPly)
    {
        const QString cur = currentSfen.trimmed();
        const QString start = startSfen.trimmed();

        if (cur.isEmpty()) {
            return false;
        }

        if (start.isEmpty()) {
            return true;
        }

        // startSfen が残っていても、選択手数が進んでいれば現在局面開始とみなす
        if (selectedPly > 0 && cur != QStringLiteral("startpos")) {
            return true;
        }

        return false;
    }

private:
    /**
     * @brief 盤面とハイライトを初期化
     */
    void clearBoardAndHighlights();

    /**
     * @brief 時計表示を初期化
     */
    void clearClockDisplay();

    /**
     * @brief 棋譜モデルをクリーンアップ
     * @param startFromCurrentPos 現在の局面から開始するか
     * @param keepRow 保持する行番号（出力）
     * @return 実際に保持された行番号
     */
    int cleanupKifuModel(bool startFromCurrentPos, int keepRow);

    /**
     * @brief 手数トラッキングを更新
     * @param startFromCurrentPos 現在の局面から開始するか
     * @param keepRow 保持する行番号
     */
    void updatePlyTracking(bool startFromCurrentPos, int keepRow);

    /**
     * @brief 分岐モデルをクリア
     */
    void clearBranchModel();

    /**
     * @brief USIログモデルを初期化
     */
    void resetUsiLogModels();

    /**
     * @brief 時間制御をリセット
     */
    void resetTimeControl();

    /**
     * @brief 評価値グラフを初期化またはトリム
     */
    void resetEvaluationGraph();

    /**
     * @brief 棋譜欄の開始行を選択（黄色ハイライト）
     */
    void selectStartRow();

    /**
     * @brief 「現在の局面から開始」かどうかを判定
     * @return 現在の局面から開始する場合true
     */
    bool isStartFromCurrentPosition() const;

    /**
     * @brief ライブゲームセッションを開始
     *
     * 対局開始時にLiveGameSessionを開始し、分岐ツリーへの
     * リアルタイム更新を有効にする。
     */
    void startLiveGameSession();

    /**
     * @brief 分岐ツリーのルートノードを作成（セッションは開始しない）
     */
    void ensureBranchTreeRoot();

    // 依存オブジェクト
    BoardInteractionController* m_boardController = nullptr;
    ShogiView* m_shogiView = nullptr;
    KifuRecordListModel* m_kifuRecordModel = nullptr;
    KifuBranchListModel* m_kifuBranchModel = nullptr;
    UsiCommLogModel* m_lineEditModel1 = nullptr;
    UsiCommLogModel* m_lineEditModel2 = nullptr;
    TimeControlController* m_timeController = nullptr;
    EvaluationChartWidget* m_evalChart = nullptr;
    EvaluationGraphController* m_evalGraphController = nullptr;
    RecordPane* m_recordPane = nullptr;

    // MainWindowの状態変数への参照
    QString* m_startSfenStr = nullptr;
    QString* m_currentSfenStr = nullptr;
    int* m_activePly = nullptr;
    int* m_currentSelectedPly = nullptr;
    int* m_currentMoveIndex = nullptr;

    // 分岐ツリー関連
    LiveGameSession* m_liveGameSession = nullptr;
    KifuBranchTree* m_branchTree = nullptr;
    KifuNavigationState* m_navState = nullptr;

    // performCleanup() 開始時に保存する値（selectStartRow() で変更される前の値）
    QString m_savedCurrentSfen;
    int m_savedSelectedPly = 0;
    KifuBranchNode* m_savedCurrentNode = nullptr;  // ユーザーが選択していたノード
};

#endif // PRESTARTCLEANUPHANDLER_H
