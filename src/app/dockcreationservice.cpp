/// @file dockcreationservice.cpp
/// @brief ドック生成サービスクラスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "dockcreationservice.h"

#include <QMenu>
#include <QAction>

#include "evaluationchartwidget.h"
#include "recordpane.h"
#include "engineanalysistab.h"
#include "gameinfopanecontroller.h"
#include "analysisresultspresenter.h"
#include "menuwindowwiring.h"
#include "josekiwindowwiring.h"
#include "menuwindow.h"
#include "josekiwindow.h"
#include "settingsservice.h"
#include "shogienginethinkingmodel.h"
#include "usicommlogmodel.h"

DockCreationService::DockCreationService(QMainWindow* mainWindow, QObject* parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
{
}

void DockCreationService::setModels(ShogiEngineThinkingModel* think1,
                                     ShogiEngineThinkingModel* think2,
                                     UsiCommLogModel* log1,
                                     UsiCommLogModel* log2)
{
    m_modelThinking1 = think1;
    m_modelThinking2 = think2;
    m_lineEditModel1 = log1;
    m_lineEditModel2 = log2;
}

void DockCreationService::setupDockFeatures(QDockWidget* dock, const QString& objectName)
{
    dock->setObjectName(objectName);
    dock->setFeatures(
        QDockWidget::DockWidgetMovable |
        QDockWidget::DockWidgetFloatable |
        QDockWidget::DockWidgetClosable);
    dock->setAllowedAreas(Qt::AllDockWidgetAreas);
    // タイトルバーの高さと色を設定（ドラッグ領域を認識しやすくする）
    dock->setStyleSheet(QStringLiteral(
        "QDockWidget::title { padding: 8px; background-color: #dcdcdc; }"));
}

void DockCreationService::addToggleActionToMenu(QDockWidget* dock, const QString& title)
{
    if (!m_displayMenu) return;

    QAction* toggleAction = dock->toggleViewAction();
    toggleAction->setText(title);
    m_displayMenu->addAction(toggleAction);
}

void DockCreationService::restoreDockState(QDockWidget* dock,
                                            const QByteArray& geometry,
                                            bool wasFloating,
                                            bool wasVisible)
{
    if (!geometry.isEmpty()) {
        dock->restoreGeometry(geometry);
    }
    dock->setFloating(wasFloating);
    dock->setVisible(wasVisible);
}

QDockWidget* DockCreationService::createEvalChartDock()
{
    if (!m_evalChart || !m_mainWindow) return nullptr;

    m_evalChartDock = new QDockWidget(tr("評価値グラフ"), m_mainWindow);
    setupDockFeatures(m_evalChartDock, QStringLiteral("EvalChartDock"));
    m_evalChartDock->setWidget(m_evalChart);

    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, m_evalChartDock);

    if (m_displayMenu) {
        m_displayMenu->addSeparator();
        addToggleActionToMenu(m_evalChartDock, tr("評価値グラフ"));
    }

    // 保存された状態を復元
    restoreDockState(m_evalChartDock,
                     SettingsService::evalChartDockGeometry(),
                     SettingsService::evalChartDockFloating(),
                     SettingsService::evalChartDockVisible());

    // ドッキング状態変更時にEvaluationChartWidgetに通知
    connect(m_evalChartDock, &QDockWidget::topLevelChanged,
            m_evalChart, &EvaluationChartWidget::setFloating);

    // 初期状態を同期
    m_evalChart->setFloating(SettingsService::evalChartDockFloating());

    return m_evalChartDock;
}

QDockWidget* DockCreationService::createRecordPaneDock()
{
    if (!m_recordPane || !m_mainWindow) {
        qWarning() << "[DockCreationService] createRecordPaneDock: recordPane or mainWindow is null";
        return nullptr;
    }

    m_recordPaneDock = new QDockWidget(tr("棋譜"), m_mainWindow);
    setupDockFeatures(m_recordPaneDock, QStringLiteral("RecordPaneDock"));
    m_recordPaneDock->setWidget(m_recordPane);

    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_recordPaneDock);
    addToggleActionToMenu(m_recordPaneDock, tr("棋譜"));

    restoreDockState(m_recordPaneDock,
                     SettingsService::recordPaneDockGeometry(),
                     SettingsService::recordPaneDockFloating(),
                     SettingsService::recordPaneDockVisible());

    return m_recordPaneDock;
}

void DockCreationService::createAnalysisDocks()
{
    if (!m_analysisTab || !m_mainWindow) {
        qWarning() << "[DockCreationService] createAnalysisDocks: analysisTab or mainWindow is null";
        return;
    }

    // ドック共通の設定を適用するヘルパー
    auto setupDock = [this](QDockWidget* dock, QWidget* content, const QString& title, const QString& objectName) {
        dock->setObjectName(objectName);
        dock->setWidget(content);
        dock->setMinimumWidth(0);
        dock->setMinimumHeight(100);
        dock->setFeatures(
            QDockWidget::DockWidgetMovable |
            QDockWidget::DockWidgetFloatable |
            QDockWidget::DockWidgetClosable);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        // タイトルバーの高さと色を設定（ドラッグ領域を認識しやすくする）
        dock->setStyleSheet(QStringLiteral(
            "QDockWidget::title { padding: 8px; background-color: #dcdcdc; }"));
        m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, dock);
        addToggleActionToMenu(dock, title);
    };

    // 0. 対局情報ドック
    if (m_gameInfoController) {
        m_gameInfoDock = new QDockWidget(tr("対局情報"), m_mainWindow);
        setupDock(m_gameInfoDock, m_gameInfoController->containerWidget(),
                  tr("対局情報"), QStringLiteral("GameInfoDock"));
    }

    // 1. 思考ドック
    m_thinkingDock = new QDockWidget(tr("思考"), m_mainWindow);
    QWidget* thinkingPage = m_analysisTab->createThinkingPage(m_thinkingDock);
    setupDock(m_thinkingDock, thinkingPage, tr("思考"), QStringLiteral("ThinkingDock"));

    // 2. 検討ドック
    m_considerationDock = new QDockWidget(tr("検討"), m_mainWindow);
    QWidget* considerationPage = m_analysisTab->createConsiderationPage(m_considerationDock);
    setupDock(m_considerationDock, considerationPage, tr("検討"), QStringLiteral("ConsiderationDock"));

    // 3. USI通信ログドック
    m_usiLogDock = new QDockWidget(tr("USI通信ログ"), m_mainWindow);
    QWidget* usiLogPage = m_analysisTab->createUsiLogPage(m_usiLogDock);
    setupDock(m_usiLogDock, usiLogPage, tr("USI通信ログ"), QStringLiteral("UsiLogDock"));

    // 4. CSA通信ログドック
    m_csaLogDock = new QDockWidget(tr("CSA通信ログ"), m_mainWindow);
    QWidget* csaLogPage = m_analysisTab->createCsaLogPage(m_csaLogDock);
    setupDock(m_csaLogDock, csaLogPage, tr("CSA通信ログ"), QStringLiteral("CsaLogDock"));

    // 5. 棋譜コメントドック
    m_commentDock = new QDockWidget(tr("棋譜コメント"), m_mainWindow);
    QWidget* commentPage = m_analysisTab->createCommentPage(m_commentDock);
    setupDock(m_commentDock, commentPage, tr("棋譜コメント"), QStringLiteral("CommentDock"));

    // 6. 分岐ツリードック
    m_branchTreeDock = new QDockWidget(tr("分岐ツリー"), m_mainWindow);
    QWidget* branchTreePage = m_analysisTab->createBranchTreePage(m_branchTreeDock);
    setupDock(m_branchTreeDock, branchTreePage, tr("分岐ツリー"), QStringLiteral("BranchTreeDock"));

    // ドックをタブ化して配置
    if (m_gameInfoDock && m_thinkingDock) {
        m_mainWindow->tabifyDockWidget(m_gameInfoDock, m_thinkingDock);
    }
    m_mainWindow->tabifyDockWidget(m_thinkingDock, m_considerationDock);
    m_mainWindow->tabifyDockWidget(m_considerationDock, m_usiLogDock);
    m_mainWindow->tabifyDockWidget(m_usiLogDock, m_csaLogDock);
    m_mainWindow->tabifyDockWidget(m_csaLogDock, m_commentDock);
    m_mainWindow->tabifyDockWidget(m_commentDock, m_branchTreeDock);

    // 対局情報ドックをアクティブにする
    if (m_gameInfoDock) {
        m_gameInfoDock->raise();
    } else if (m_thinkingDock) {
        m_thinkingDock->raise();
    }

    // モデルを設定
    if (m_modelThinking1 && m_modelThinking2 && m_lineEditModel1 && m_lineEditModel2) {
        m_analysisTab->setModels(m_modelThinking1, m_modelThinking2, m_lineEditModel1, m_lineEditModel2);
    }
}

QDockWidget* DockCreationService::createMenuWindowDock()
{
    if (!m_menuWiring || !m_mainWindow) {
        qWarning() << "[DockCreationService] createMenuWindowDock: menuWiring or mainWindow is null";
        return nullptr;
    }

    m_menuWiring->ensureMenuWindow();
    MenuWindow* menuWindow = m_menuWiring->menuWindow();
    if (!menuWindow) {
        qWarning() << "[DockCreationService] createMenuWindowDock: MenuWindow is null";
        return nullptr;
    }

    m_menuWindowDock = new QDockWidget(tr("メニュー"), m_mainWindow);
    setupDockFeatures(m_menuWindowDock, QStringLiteral("MenuWindowDock"));
    m_menuWindowDock->setWidget(menuWindow);

    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_menuWindowDock);
    addToggleActionToMenu(m_menuWindowDock, tr("メニュー"));

    restoreDockState(m_menuWindowDock,
                     SettingsService::menuWindowDockGeometry(),
                     SettingsService::menuWindowDockFloating(),
                     SettingsService::menuWindowDockVisible());

    return m_menuWindowDock;
}

QDockWidget* DockCreationService::createJosekiWindowDock()
{
    if (!m_josekiWiring || !m_mainWindow) {
        qWarning() << "[DockCreationService] createJosekiWindowDock: josekiWiring or mainWindow is null";
        return nullptr;
    }

    m_josekiWiring->ensureJosekiWindow();
    JosekiWindow* josekiWindow = m_josekiWiring->josekiWindow();
    if (!josekiWindow) {
        qWarning() << "[DockCreationService] createJosekiWindowDock: JosekiWindow is null";
        return nullptr;
    }

    m_josekiWindowDock = new QDockWidget(tr("定跡"), m_mainWindow);
    setupDockFeatures(m_josekiWindowDock, QStringLiteral("JosekiWindowDock"));
    m_josekiWindowDock->setWidget(josekiWindow);

    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_josekiWindowDock);
    addToggleActionToMenu(m_josekiWindowDock, tr("定跡"));

    restoreDockState(m_josekiWindowDock,
                     SettingsService::josekiWindowDockGeometry(),
                     SettingsService::josekiWindowDockFloating(),
                     SettingsService::josekiWindowDockVisible());

    return m_josekiWindowDock;
}

QDockWidget* DockCreationService::createAnalysisResultsDock()
{
    if (!m_analysisPresenter || !m_mainWindow) {
        qWarning() << "[DockCreationService] createAnalysisResultsDock: analysisPresenter or mainWindow is null";
        return nullptr;
    }

    m_analysisResultsDock = new QDockWidget(tr("棋譜解析"), m_mainWindow);
    setupDockFeatures(m_analysisResultsDock, QStringLiteral("AnalysisResultsDock"));
    m_analysisResultsDock->setWidget(m_analysisPresenter->containerWidget());

    m_analysisPresenter->setDockWidget(m_analysisResultsDock);

    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, m_analysisResultsDock);

    // 分岐ツリードックの後ろにタブ化
    if (m_branchTreeDock) {
        m_mainWindow->tabifyDockWidget(m_branchTreeDock, m_analysisResultsDock);
    }

    addToggleActionToMenu(m_analysisResultsDock, tr("棋譜解析"));

    restoreDockState(m_analysisResultsDock,
                     SettingsService::kifuAnalysisResultsDockGeometry(),
                     SettingsService::kifuAnalysisResultsDockFloating(),
                     SettingsService::kifuAnalysisResultsDockVisible());

    return m_analysisResultsDock;
}
