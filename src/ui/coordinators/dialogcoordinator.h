#ifndef DIALOGCOORDINATOR_H
#define DIALOGCOORDINATOR_H

/// @file dialogcoordinator.h
/// @brief ダイアログコーディネータクラスの定義


#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>
#include <functional>

#include "kifparsetypes.h"

class QWidget;
class MatchCoordinator;
class Usi;
class UsiCommLogModel;
class ShogiEngineThinkingModel;
class KifuAnalysisListModel;
class KifuRecordListModel;
class ConsiderationTabManager;
class ShogiGameController;
class ConsiderationFlowController;
class TsumeSearchFlowController;
class AnalysisFlowController;
class KifuDisplay;
class GameInfoPaneController;
class KifuLoadCoordinator;
class EvaluationChartWidget;
class AnalysisResultsPresenter;
struct ShogiMove;

/**
 * @brief DialogCoordinator - ダイアログ表示の管理クラス
 *
 * MainWindowから各種ダイアログの表示処理を分離したクラス。
 * 以下の責務を担当:
 * - バージョン情報ダイアログ
 * - エンジン設定ダイアログ
 * - 成り・不成選択ダイアログ
 * - 検討ダイアログ
 * - 詰み探索ダイアログ
 * - 棋譜解析ダイアログ
 * - ゲームオーバーメッセージボックス
 * - エラーメッセージボックス
 */
class DialogCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit DialogCoordinator(QWidget* parentWidget, QObject* parent = nullptr);
    ~DialogCoordinator() override;

    // --------------------------------------------------------
    // 依存オブジェクトの設定
    // --------------------------------------------------------

    /**
     * @brief 依存オブジェクト群
     */
    struct Deps {
        MatchCoordinator* matchCoordinator = nullptr;         ///< 対局調整（非所有）
        ShogiGameController* gameController = nullptr;        ///< ゲーム制御（非所有）
        Usi* usiEngine = nullptr;                             ///< USIエンジン（解析用、非所有）
        UsiCommLogModel* logModel = nullptr;                  ///< ログモデル（解析用、非所有）
        ShogiEngineThinkingModel* thinkingModel = nullptr;    ///< 思考モデル（解析用、非所有）
        KifuAnalysisListModel* analysisModel = nullptr;       ///< 解析モデル（非所有）
        ConsiderationTabManager* considerationTabManager = nullptr; ///< 検討タブマネージャー（非所有）
    };

    /**
     * @brief 依存オブジェクトをまとめて更新する
     * @param deps 新しい依存オブジェクト群
     */
    void updateDeps(const Deps& deps);

    // --------------------------------------------------------
    // 情報ダイアログ
    // --------------------------------------------------------

    /**
     * @brief バージョン情報ダイアログを表示
     */
    void showVersionInformation();

    /**
     * @brief プロジェクトWebサイトを開く
     */
    void openProjectWebsite();

    /**
     * @brief エンジン設定ダイアログを表示
     */
    void showEngineSettingsDialog();

    // --------------------------------------------------------
    // ゲームプレイ関連ダイアログ
    // --------------------------------------------------------

    /**
     * @brief 成り・不成選択ダイアログを表示
     * @return true=成り, false=不成
     */
    bool showPromotionDialog();

    /**
     * @brief ゲームオーバーメッセージボックスを表示
     */
    void showGameOverMessage(const QString& title, const QString& message);

    // --------------------------------------------------------
    // 解析関連ダイアログ
    // --------------------------------------------------------

    /**
     * @brief 検討を直接開始するためのパラメータ
     */
    struct ConsiderationDirectParams {
        QString position;                  // 送信する position 文字列
        int engineIndex = 0;               // エンジンインデックス
        QString engineName;                // エンジン名
        bool unlimitedTime = true;         // 時間無制限フラグ
        int byoyomiSec = 20;               // 検討時間（秒）
        int multiPV = 1;                   // 候補手の数
        ShogiEngineThinkingModel* considerationModel = nullptr;  // 検討タブ用モデル
        int previousFileTo = 0;            // 前回の移動先の筋（1-9, 0=未設定）
        int previousRankTo = 0;            // 前回の移動先の段（1-9, 0=未設定）
        QString lastUsiMove;               // 開始局面に至った最後の指し手（USI形式）
    };

    /**
     * @brief ダイアログを表示せずに直接検討を開始
     */
    void startConsiderationDirect(const ConsiderationDirectParams& params);

    /**
     * @brief 検討ダイアログの依存コンテキスト
     *
     * MainWindowから一度設定することで、パラメータ収集を自動化します。
     */
    struct ConsiderationContext {
        ShogiGameController* gameController = nullptr;
        const QList<ShogiMove>* gameMoves = nullptr;
        const int* currentMoveIndex = nullptr;
        KifuRecordListModel* kifuRecordModel = nullptr;
        QStringList* sfenRecord = nullptr;
        QString* startSfenStr = nullptr;
        const QString* currentSfenStr = nullptr;             // 現在表示中の局面SFEN（分岐対応）
        class KifuBranchTree* branchTree = nullptr;          // 分岐ツリー（分岐ラインの手取得用）
        class KifuNavigationState* navState = nullptr;       // 現在の分岐ライン状態
        ShogiEngineThinkingModel** considerationModel = nullptr;  // ダブルポインタ（遅延生成用）
        const QStringList* gameUsiMoves = nullptr;                // 対局中のUSI指し手リスト
        class KifuLoadCoordinator* kifuLoadCoordinator = nullptr; // 棋譜読み込みコーディネータ
    };

    /**
     * @brief 検討コンテキストを設定
     * @param ctx 検討に必要な依存オブジェクト群
     */
    void setConsiderationContext(const ConsiderationContext& ctx);

    /**
     * @brief 検討を開始（コンテキストから自動パラメータ構築）
     *
     * setConsiderationContext で設定した依存オブジェクトから
     * 自動的にパラメータを構築して検討を開始します。
     * @return true=検討開始, false=エラー（エンジン未選択等）
     */
    bool startConsiderationFromContext();

    /**
     * @brief 詰み探索ダイアログの依存情報
     */
    struct TsumeSearchParams {
        QStringList* sfenRecord = nullptr;
        QString startSfenStr;
        QStringList positionStrList;
        int currentMoveIndex = 0;
        QStringList* usiMoves = nullptr;       // USI形式の指し手リスト（moves形式用）
        QString startPositionCmd;              // 開始局面コマンド（"startpos" or "sfen ..."）
    };

    /**
     * @brief 詰み探索ダイアログを表示
     */
    void showTsumeSearchDialog(const TsumeSearchParams& params);

    /**
     * @brief 詰み探索ダイアログの依存コンテキスト
     *
     * MainWindowから一度設定することで、パラメータ収集を自動化します。
     */
    struct TsumeSearchContext {
        QStringList* sfenRecord = nullptr;
        QString* startSfenStr = nullptr;
        QStringList* positionStrList = nullptr;
        const int* currentMoveIndex = nullptr;
        QStringList* gameUsiMoves = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
    };

    /**
     * @brief 詰み探索コンテキストを設定
     * @param ctx 詰み探索に必要な依存オブジェクト群
     */
    void setTsumeSearchContext(const TsumeSearchContext& ctx);

    /**
     * @brief 詰み探索ダイアログを表示（コンテキストから自動パラメータ構築）
     *
     * setTsumeSearchContext で設定した依存オブジェクトから
     * 自動的にパラメータを構築してダイアログを表示します。
     */
    void showTsumeSearchDialogFromContext();

    /**
     * @brief 棋譜解析ダイアログの依存情報
     */
    struct KifuAnalysisParams {
        QStringList* sfenRecord = nullptr;
        QList<KifuDisplay*>* moveRecords = nullptr;
        KifuRecordListModel* recordModel = nullptr;  // 棋譜モデル（指し手ラベル取得用）
        int activePly = 0;
        ShogiGameController* gameController = nullptr;  // 盤面情報取得用
        QString blackPlayerName;   // 先手対局者名
        QString whitePlayerName;   // 後手対局者名
        QStringList* usiMoves = nullptr;  // USI形式の指し手リスト
        AnalysisResultsPresenter* presenter = nullptr;  // 結果表示用プレゼンター
        bool boardFlipped = false;  // GUI本体の盤面反転状態
    };

    /**
     * @brief 棋譜解析ダイアログの依存コンテキスト
     *
     * MainWindowから一度設定することで、パラメータ収集を自動化します。
     */
    struct KifuAnalysisContext {
        QStringList* sfenRecord = nullptr;
        QList<KifuDisplay*>* moveRecords = nullptr;
        KifuRecordListModel* recordModel = nullptr;
        int* activePly = nullptr;
        ShogiGameController* gameController = nullptr;
        GameInfoPaneController* gameInfoController = nullptr;
        KifuLoadCoordinator* kifuLoadCoordinator = nullptr;
        EvaluationChartWidget* evalChart = nullptr;
        QStringList* gameUsiMoves = nullptr;  // 対局時のUSI形式指し手リスト（MainWindow::m_gameUsiMoves）
        AnalysisResultsPresenter* presenter = nullptr;  // 結果表示用プレゼンター
        std::function<bool()> getBoardFlipped;  // GUI本体の盤面反転状態取得コールバック
    };

    /**
     * @brief 棋譜解析コンテキストを設定
     * @param ctx 棋譜解析に必要な依存オブジェクト群
     */
    void setKifuAnalysisContext(const KifuAnalysisContext& ctx);

    /**
     * @brief 棋譜解析ダイアログを表示
     */
    void showKifuAnalysisDialog(const KifuAnalysisParams& params);

    /**
     * @brief 棋譜解析ダイアログを表示（コンテキストから自動パラメータ構築）
     * 
     * setKifuAnalysisContext で設定した依存オブジェクトから
     * 自動的にパラメータを構築してダイアログを表示します。
     */
    void showKifuAnalysisDialogFromContext();

    /**
     * @brief 棋譜解析を中止
     */
    void stopKifuAnalysis();

    /**
     * @brief 棋譜解析中かどうか
     */
    bool isKifuAnalysisRunning() const;

    // --------------------------------------------------------
    // エラー表示
    // --------------------------------------------------------

    /**
     * @brief エラーメッセージを表示
     */
    void showErrorMessage(const QString& message);

    /**
     * @brief Flowからのエラーを表示
     */
    void showFlowError(const QString& message);

signals:
    /**
     * @brief 検討モードが開始された
     */
    void considerationModeStarted();

    /**
     * @brief 検討モードの時間設定が確定した
     * @param unlimited 時間無制限の場合true
     * @param byoyomiSec 検討時間（秒）。unlimitedがtrueの場合は0
     */
    void considerationTimeSettingsReady(bool unlimited, int byoyomiSec);

    /**
     * @brief 検討モードの候補手の数が確定した
     * @param multiPV 候補手の数
     */
    void considerationMultiPVReady(int multiPV);

    /**
     * @brief 詰み探索モードが開始された
     */
    void tsumeSearchModeStarted();

    /**
     * @brief 詰み探索モードが終了した（ダイアログキャンセル時）
     */
    void tsumeSearchModeEnded();

    /**
     * @brief 棋譜解析モードが開始された
     */
    void analysisModeStarted();

    /**
     * @brief 棋譜解析モードが終了した（完了または中止）
     */
    void analysisModeEnded();

    /**
     * @brief 棋譜解析進捗を通知
     */
    void analysisProgressReported(int ply, int scoreCp);

    /**
     * @brief 棋譜解析結果の行が選択されたときに通知
     */
    void analysisResultRowSelected(int row);

private:
    QWidget* m_parentWidget = nullptr;
    MatchCoordinator* m_match = nullptr;
    ShogiGameController* m_gc = nullptr;
    Usi* m_usi = nullptr;
    UsiCommLogModel* m_logModel = nullptr;
    ShogiEngineThinkingModel* m_thinkingModel = nullptr;
    KifuAnalysisListModel* m_analysisModel = nullptr;
    ConsiderationTabManager* m_considerationTabManager = nullptr;

    // Flow コントローラ（遅延生成）
    AnalysisFlowController* m_analysisFlow = nullptr;

    // コンテキスト
    ConsiderationContext m_considerationCtx;
    TsumeSearchContext m_tsumeSearchCtx;
    KifuAnalysisContext m_kifuAnalysisCtx;

    // ヘルパー: 対局情報から対局者名を取得
    static void extractPlayerNames(const QList<KifGameInfoItem>& gameInfo,
                                   QString& outBlackName, QString& outWhiteName);

};

#endif // DIALOGCOORDINATOR_H
