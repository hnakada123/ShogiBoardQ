#ifndef DIALOGCOORDINATOR_H
#define DIALOGCOORDINATOR_H

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
class EngineAnalysisTab;
class ShogiGameController;
class ConsiderationFlowController;
class TsumeSearchFlowController;
class AnalysisFlowController;
class KifuDisplay;
class GameInfoPaneController;
class KifuLoadCoordinator;
class RecordPane;

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
     * @brief MatchCoordinatorを設定
     */
    void setMatchCoordinator(MatchCoordinator* match);

    /**
     * @brief ShogiGameControllerを設定（成り判定用）
     */
    void setGameController(ShogiGameController* gc);

    /**
     * @brief USIエンジンを設定（解析用）
     */
    void setUsiEngine(Usi* usi);

    /**
     * @brief ログモデルを設定（解析用）
     */
    void setLogModel(UsiCommLogModel* logModel);

    /**
     * @brief 思考モデルを設定（解析用）
     */
    void setThinkingModel(ShogiEngineThinkingModel* thinkingModel);

    /**
     * @brief 解析モデルを設定
     */
    void setAnalysisModel(KifuAnalysisListModel* model);

    /**
     * @brief 解析タブを設定
     */
    void setAnalysisTab(EngineAnalysisTab* tab);

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
     * @brief 検討ダイアログの依存情報
     */
    struct ConsiderationParams {
        QString position;  // 送信する position 文字列
    };

    /**
     * @brief 検討ダイアログを表示
     */
    void showConsiderationDialog(const ConsiderationParams& params);

    /**
     * @brief 詰み探索ダイアログの依存情報
     */
    struct TsumeSearchParams {
        QStringList* sfenRecord = nullptr;
        QString startSfenStr;
        QStringList positionStrList;
        int currentMoveIndex = 0;
    };

    /**
     * @brief 詰み探索ダイアログを表示
     */
    void showTsumeSearchDialog(const TsumeSearchParams& params);

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
        RecordPane* recordPane = nullptr;
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

Q_SIGNALS:
    /**
     * @brief 検討モードが開始された
     */
    void considerationModeStarted();

    /**
     * @brief 詰み探索モードが開始された
     */
    void tsumeSearchModeStarted();

    /**
     * @brief 棋譜解析モードが開始された
     */
    void analysisModeStarted();

    /**
     * @brief 棋譜解析進捗を通知
     */
    void analysisProgressReported(int ply, int scoreCp);

private:
    QWidget* m_parentWidget = nullptr;
    MatchCoordinator* m_match = nullptr;
    ShogiGameController* m_gc = nullptr;
    Usi* m_usi = nullptr;
    UsiCommLogModel* m_logModel = nullptr;
    ShogiEngineThinkingModel* m_thinkingModel = nullptr;
    KifuAnalysisListModel* m_analysisModel = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;

    // Flow コントローラ（遅延生成）
    AnalysisFlowController* m_analysisFlow = nullptr;

    // 棋譜解析コンテキスト
    KifuAnalysisContext m_kifuAnalysisCtx;

    // ヘルパー: 対局情報から対局者名を取得
    static void extractPlayerNames(const QList<KifGameInfoItem>& gameInfo,
                                   QString& outBlackName, QString& outWhiteName);
};

#endif // DIALOGCOORDINATOR_H
