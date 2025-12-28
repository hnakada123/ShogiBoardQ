#ifndef DIALOGCOORDINATOR_H
#define DIALOGCOORDINATOR_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <functional>

class QWidget;
class MatchCoordinator;
class Usi;
class UsiCommLogModel;
class KifuAnalysisListModel;
class EngineAnalysisTab;
class ShogiGameController;
class ConsiderationFlowController;
class TsumeSearchFlowController;
class AnalysisFlowController;
class KifuDisplay;

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
        int activePly = 0;
    };

    /**
     * @brief 棋譜解析ダイアログを表示
     */
    void showKifuAnalysisDialog(const KifuAnalysisParams& params);

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

private:
    QWidget* m_parentWidget = nullptr;
    MatchCoordinator* m_match = nullptr;
    ShogiGameController* m_gc = nullptr;
    Usi* m_usi = nullptr;
    UsiCommLogModel* m_logModel = nullptr;
    KifuAnalysisListModel* m_analysisModel = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;

    // Flow コントローラ（遅延生成）
    AnalysisFlowController* m_analysisFlow = nullptr;
};

#endif // DIALOGCOORDINATOR_H
