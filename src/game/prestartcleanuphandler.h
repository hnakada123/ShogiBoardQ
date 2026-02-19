#ifndef PRESTARTCLEANUPHANDLER_H
#define PRESTARTCLEANUPHANDLER_H

/// @file prestartcleanuphandler.h
/// @brief 対局開始前クリーンアップハンドラクラスの定義

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
 * GameStartCoordinator::requestPreStartCleanupシグナルに接続して使用する。
 *
 * @todo remove コメントスタイルガイド適用済み
 */
class PreStartCleanupHandler : public QObject
{
    Q_OBJECT

public:
    /// 依存オブジェクト構造体
    struct Dependencies {
        BoardInteractionController* boardController = nullptr;  ///< 盤面操作コントローラ（非所有）
        ShogiView* shogiView = nullptr;                         ///< 盤面ビュー（非所有）
        KifuRecordListModel* kifuRecordModel = nullptr;         ///< 棋譜モデル（非所有）
        KifuBranchListModel* kifuBranchModel = nullptr;         ///< 分岐モデル（非所有）
        UsiCommLogModel* lineEditModel1 = nullptr;              ///< エンジン1のUSIログモデル（非所有）
        UsiCommLogModel* lineEditModel2 = nullptr;              ///< エンジン2のUSIログモデル（非所有）
        TimeControlController* timeController = nullptr;        ///< 時間制御コントローラ（非所有）
        EvaluationChartWidget* evalChart = nullptr;             ///< 評価値グラフウィジェット（非所有）
        EvaluationGraphController* evalGraphController = nullptr; ///< 評価値グラフコントローラ（非所有）
        RecordPane* recordPane = nullptr;                       ///< 棋譜ペイン（非所有）

        // --- MainWindowの状態変数への参照 ---
        QString* startSfenStr = nullptr;                        ///< 開始SFEN文字列
        QString* currentSfenStr = nullptr;                      ///< 現在SFEN文字列
        int* activePly = nullptr;                               ///< アクティブな手数
        int* currentSelectedPly = nullptr;                      ///< 選択中の手数
        int* currentMoveIndex = nullptr;                        ///< 現在の手数インデックス

        // --- 分岐ツリー関連 ---
        LiveGameSession* liveGameSession = nullptr;             ///< ライブゲームセッション（非所有）
        KifuBranchTree* branchTree = nullptr;                   ///< 分岐ツリー（非所有）
        KifuNavigationState* navState = nullptr;                ///< ナビゲーション状態（非所有）
    };

    explicit PreStartCleanupHandler(const Dependencies& deps, QObject* parent = nullptr);
    ~PreStartCleanupHandler() override = default;

public slots:
    /// 対局開始前のクリーンアップを実行する（→ GameStartCoordinator::requestPreStartCleanup）
    void performCleanup();

public:
    /**
     * @brief 「現在の局面から開始」かどうかの判定ロジック
     *
     * テスト用に公開している。startSfenが残っていても
     * selectedPlyが進んでいれば現在局面開始とみなす。
     *
     * @todo remove コメントスタイルガイド適用済み
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
    // --- クリーンアップ個別処理 ---

    void clearBoardAndHighlights();
    void clearClockDisplay();

    /**
     * @brief 棋譜モデルをクリーンアップする
     * @param startFromCurrentPos 現在の局面から開始するか
     * @param keepRow 保持する行番号（入力値）
     * @return 実際に保持された行番号
     */
    int cleanupKifuModel(bool startFromCurrentPos, int keepRow);

    /**
     * @brief 手数トラッキングを更新する
     * @param startFromCurrentPos 現在の局面から開始するか
     * @param keepRow 保持する行番号
     */
    void updatePlyTracking(bool startFromCurrentPos, int keepRow);

    void clearBranchModel();
    void resetUsiLogModels();
    void resetTimeControl();

    /// 評価値グラフを初期化またはトリムする
    void resetEvaluationGraph();

    /// 棋譜欄の開始行を選択する（黄色ハイライト）
    void selectStartRow();

    bool isStartFromCurrentPosition() const;

    // --- ライブゲームセッション ---

    /// ライブゲームセッションを開始し、分岐ツリーへのリアルタイム更新を有効にする
    void startLiveGameSession();

    /// 分岐ツリーのルートノードを作成する（セッションは開始しない）
    void ensureBranchTreeRoot();

    // --- メンバー変数 ---

    BoardInteractionController* m_boardController = nullptr;    ///< 盤面操作コントローラ（非所有）
    ShogiView* m_shogiView = nullptr;                           ///< 盤面ビュー（非所有）
    KifuRecordListModel* m_kifuRecordModel = nullptr;           ///< 棋譜モデル（非所有）
    KifuBranchListModel* m_kifuBranchModel = nullptr;           ///< 分岐モデル（非所有）
    UsiCommLogModel* m_lineEditModel1 = nullptr;                ///< エンジン1 USIログ（非所有）
    UsiCommLogModel* m_lineEditModel2 = nullptr;                ///< エンジン2 USIログ（非所有）
    TimeControlController* m_timeController = nullptr;          ///< 時間制御コントローラ（非所有）
    EvaluationChartWidget* m_evalChart = nullptr;               ///< 評価値グラフ（非所有）
    EvaluationGraphController* m_evalGraphController = nullptr; ///< 評価値グラフコントローラ（非所有）
    RecordPane* m_recordPane = nullptr;                         ///< 棋譜ペイン（非所有）

    // --- MainWindow状態変数への参照 ---

    QString* m_startSfenStr = nullptr;         ///< 開始SFEN文字列への参照
    QString* m_currentSfenStr = nullptr;       ///< 現在SFEN文字列への参照
    int* m_activePly = nullptr;                ///< アクティブ手数への参照
    int* m_currentSelectedPly = nullptr;       ///< 選択中手数への参照
    int* m_currentMoveIndex = nullptr;         ///< 手数インデックスへの参照

    // --- 分岐ツリー関連 ---

    LiveGameSession* m_liveGameSession = nullptr;  ///< ライブゲームセッション（非所有）
    KifuBranchTree* m_branchTree = nullptr;        ///< 分岐ツリー（非所有）
    KifuNavigationState* m_navState = nullptr;     ///< ナビゲーション状態（非所有）

    // --- performCleanup()用の一時保存値 ---

    QString m_savedCurrentSfen;                    ///< cleanup開始時のSFEN（selectStartRow前に保存）
    int m_savedSelectedPly = 0;                    ///< cleanup開始時の選択手数
    KifuBranchNode* m_savedCurrentNode = nullptr;  ///< cleanup開始時に選択されていたノード
};

#endif // PRESTARTCLEANUPHANDLER_H
