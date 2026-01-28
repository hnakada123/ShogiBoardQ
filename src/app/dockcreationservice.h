#ifndef DOCKCREATIONSERVICE_H
#define DOCKCREATIONSERVICE_H

#include <QObject>
#include <QDockWidget>
#include <QMainWindow>

class QMenu;
class QAction;
class EvaluationChartWidget;
class RecordPane;
class EngineAnalysisTab;
class GameInfoPaneController;
class AnalysisResultsPresenter;
class MenuWindowWiring;
class JosekiWindowWiring;
class ShogiEngineThinkingModel;
class UsiCommLogModel;

/**
 * @brief ドック作成サービス
 *
 * MainWindowからドック作成ロジックを分離したクラス。
 * 各種ドックウィジェットの作成、設定復元、メニュー登録を担当する。
 */
class DockCreationService : public QObject
{
    Q_OBJECT

public:
    explicit DockCreationService(QMainWindow* mainWindow, QObject* parent = nullptr);

    // 依存オブジェクトの設定
    void setDisplayMenu(QMenu* menu) { m_displayMenu = menu; }
    void setEvalChart(EvaluationChartWidget* chart) { m_evalChart = chart; }
    void setRecordPane(RecordPane* pane) { m_recordPane = pane; }
    void setAnalysisTab(EngineAnalysisTab* tab) { m_analysisTab = tab; }
    void setGameInfoController(GameInfoPaneController* controller) { m_gameInfoController = controller; }
    void setAnalysisPresenter(AnalysisResultsPresenter* presenter) { m_analysisPresenter = presenter; }
    void setMenuWiring(MenuWindowWiring* wiring) { m_menuWiring = wiring; }
    void setJosekiWiring(JosekiWindowWiring* wiring) { m_josekiWiring = wiring; }
    void setModels(ShogiEngineThinkingModel* think1, ShogiEngineThinkingModel* think2,
                   UsiCommLogModel* log1, UsiCommLogModel* log2);

    // ドック作成メソッド
    QDockWidget* createEvalChartDock();
    QDockWidget* createRecordPaneDock();
    void createAnalysisDocks();  // 複数のドックを作成
    QDockWidget* createMenuWindowDock();
    QDockWidget* createJosekiWindowDock();
    QDockWidget* createAnalysisResultsDock();

    // 作成されたドックへのアクセス
    QDockWidget* evalChartDock() const { return m_evalChartDock; }
    QDockWidget* recordPaneDock() const { return m_recordPaneDock; }
    QDockWidget* gameInfoDock() const { return m_gameInfoDock; }
    QDockWidget* thinkingDock() const { return m_thinkingDock; }
    QDockWidget* considerationDock() const { return m_considerationDock; }
    QDockWidget* usiLogDock() const { return m_usiLogDock; }
    QDockWidget* csaLogDock() const { return m_csaLogDock; }
    QDockWidget* commentDock() const { return m_commentDock; }
    QDockWidget* branchTreeDock() const { return m_branchTreeDock; }
    QDockWidget* menuWindowDock() const { return m_menuWindowDock; }
    QDockWidget* josekiWindowDock() const { return m_josekiWindowDock; }
    QDockWidget* analysisResultsDock() const { return m_analysisResultsDock; }

signals:
    // 評価値グラフのフローティング状態変更通知
    void evalChartFloatingChanged(bool floating);

private:
    // 共通のドック設定ヘルパー
    void setupDockFeatures(QDockWidget* dock, const QString& objectName);
    void addToggleActionToMenu(QDockWidget* dock, const QString& title);
    void restoreDockState(QDockWidget* dock,
                          const QByteArray& geometry,
                          bool wasFloating,
                          bool wasVisible);

    QMainWindow* m_mainWindow = nullptr;
    QMenu* m_displayMenu = nullptr;

    // 依存オブジェクト
    EvaluationChartWidget* m_evalChart = nullptr;
    RecordPane* m_recordPane = nullptr;
    EngineAnalysisTab* m_analysisTab = nullptr;
    GameInfoPaneController* m_gameInfoController = nullptr;
    AnalysisResultsPresenter* m_analysisPresenter = nullptr;
    MenuWindowWiring* m_menuWiring = nullptr;
    JosekiWindowWiring* m_josekiWiring = nullptr;
    ShogiEngineThinkingModel* m_modelThinking1 = nullptr;
    ShogiEngineThinkingModel* m_modelThinking2 = nullptr;
    UsiCommLogModel* m_lineEditModel1 = nullptr;
    UsiCommLogModel* m_lineEditModel2 = nullptr;

    // 作成されたドック
    QDockWidget* m_evalChartDock = nullptr;
    QDockWidget* m_recordPaneDock = nullptr;
    QDockWidget* m_gameInfoDock = nullptr;
    QDockWidget* m_thinkingDock = nullptr;
    QDockWidget* m_considerationDock = nullptr;
    QDockWidget* m_usiLogDock = nullptr;
    QDockWidget* m_csaLogDock = nullptr;
    QDockWidget* m_commentDock = nullptr;
    QDockWidget* m_branchTreeDock = nullptr;
    QDockWidget* m_menuWindowDock = nullptr;
    QDockWidget* m_josekiWindowDock = nullptr;
    QDockWidget* m_analysisResultsDock = nullptr;
};

#endif // DOCKCREATIONSERVICE_H
