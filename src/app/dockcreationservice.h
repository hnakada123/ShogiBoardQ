#ifndef DOCKCREATIONSERVICE_H
#define DOCKCREATIONSERVICE_H

/// @file dockcreationservice.h
/// @brief ドック生成サービスクラスの定義


#include <QObject>
#include <QDockWidget>
#include <QMainWindow>

class QMenu;
class QAction;
class EvaluationChartWidget;
#include "recordpane.h"
class GameInfoPane;
class AnalysisResultsPanel;
class MenuWindowController;
class JosekiWindowController;
class ThinkingPanel;
class ConsiderationTabManager;
class UsiLogPanel;
class CsaLogPanel;
class CommentEditorPanel;
class BranchTreeManager;

/**
 * @brief メインウィンドウのドックを作る
 *
 * 各部品をドックに入れ、保存した位置・表示状態を戻し、「表示」メニューに表示の切り替えを足す。
 */
class DockCreationService : public QObject
{
    Q_OBJECT

public:
    /// ドックに入れる部品（無いものは、そのドックを作らないか空にする）
    struct Deps {
        QMainWindow* mainWindow = nullptr;                  ///< ドックを置くウィンドウ（必須）
        QMenu* displayMenu = nullptr;                       ///< 「表示」メニュー（各ドックの表示切り替え）
        EvaluationChartWidget* evalChart = nullptr;
        RecordPane* recordPane = nullptr;
        GameInfoPane* gameInfo = nullptr;
        ThinkingPanel* thinking = nullptr;
        ConsiderationTabManager* consideration = nullptr;
        UsiLogPanel* usiLog = nullptr;
        CsaLogPanel* csaLog = nullptr;
        CommentEditorPanel* comment = nullptr;
        BranchTreeManager* branchTree = nullptr;
        AnalysisResultsPanel* analysisResults = nullptr;
        MenuWindowController* menuWindow = nullptr;
        JosekiWindowController* joseki = nullptr;
    };

    explicit DockCreationService(const Deps& deps, QObject* parent = nullptr);

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
    QDockWidget* analysisResultsDock() const { return m_analysisResultsDock; }

private:
    // 共通のドック設定ヘルパー
    void setupDockFeatures(QDockWidget* dock, const QString& objectName);
    void addToggleActionToMenu(QDockWidget* dock, const QString& title);
    void restoreDockState(QDockWidget* dock,
                          const QByteArray& geometry,
                          bool wasFloating,
                          bool wasVisible);

    const Deps m_deps;
    QMainWindow* m_mainWindow = nullptr;
    QMenu* m_displayMenu = nullptr;

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
