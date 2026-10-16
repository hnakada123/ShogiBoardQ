/// @file dockcreationservice.cpp
/// @brief ドック生成サービスクラスの実装

#include "dockcreationservice.h"

#include <QMenu>
#include <QAction>

#include "evaluationchartwidget.h"
#include "branchtreemanager.h"
#include <QGraphicsView>
#include "commenteditorpanel.h"
#include "considerationtabmanager.h"
#include "csalogpanel.h"
#include "thinkingpanel.h"
#include "usilogpanel.h"
#include "gameinfopane.h"
#include "analysisresultspanel.h"
#include "menuwindowcontroller.h"
#include "josekiwindowcontroller.h"
#include "menuwindow.h"
#include "josekiwindow.h"
#include "docksettings.h"
#include "layoutboundarywidget.h"
#include "opaquebackground.h"

DockCreationService::DockCreationService(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
    , m_mainWindow(deps.mainWindow)
    , m_displayMenu(deps.displayMenu)
{
    if (m_mainWindow) {
        m_mainWindow->setStyleSheet(m_mainWindow->styleSheet() + QStringLiteral(
            "QMainWindow > QTabBar::tab { background: #edf0f3; color: #596571;"
            " border: none; border-bottom: 2px solid transparent; padding: 6px 12px; }"
            "QMainWindow > QTabBar::tab:hover { background: #e7edf3; color: #243b53; }"
            "QMainWindow > QTabBar::tab:selected { background: #ffffff; color: #243b53;"
            " border-bottom-color: #52799c; }"));
    }
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
        "QDockWidget::title { padding: 6px 8px; background-color: #edf0f3; }"));
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
    if (!m_deps.evalChart || !m_mainWindow) return nullptr;

    m_evalChartDock = new QDockWidget(tr("評価値グラフ"), m_mainWindow);
    setupDockFeatures(m_evalChartDock, QStringLiteral("EvalChartDock"));
    m_evalChartDock->setWidget(m_deps.evalChart);

    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, m_evalChartDock);

    // 下部タブエリアにタブ化
    if (m_branchTreeDock) {
        m_mainWindow->tabifyDockWidget(m_branchTreeDock, m_evalChartDock);
    }

    if (m_displayMenu) {
        addToggleActionToMenu(m_evalChartDock, tr("評価値グラフ"));
    }

    // 保存された状態を復元
    restoreDockState(m_evalChartDock,
                     DockSettings::evalChartDockGeometry(),
                     DockSettings::evalChartDockFloating(),
                     DockSettings::evalChartDockVisible());

    // ドッキング状態変更時にEvaluationChartWidgetに通知
    connect(m_evalChartDock, &QDockWidget::topLevelChanged,
            m_deps.evalChart, &EvaluationChartWidget::setFloating);

    // 初期状態を同期
    m_deps.evalChart->setFloating(DockSettings::evalChartDockFloating());

    return m_evalChartDock;
}

QDockWidget* DockCreationService::createRecordPaneDock()
{
    if (!m_deps.recordPane || !m_mainWindow) {
        qWarning() << "[DockCreationService] createRecordPaneDock: recordPane or mainWindow is null";
        return nullptr;
    }

    m_recordPaneDock = new QDockWidget(tr("棋譜"), m_mainWindow);
    setupDockFeatures(m_recordPaneDock, QStringLiteral("RecordPaneDock"));
    m_recordPaneDock->setWidget(m_deps.recordPane);
    // 着手のたびに更新する棋譜欄の描き直しを、ドックとメインウィンドウまで広げない
    OpaqueBackground::keep(m_deps.recordPane);

    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_recordPaneDock);
    addToggleActionToMenu(m_recordPaneDock, tr("棋譜"));

    restoreDockState(m_recordPaneDock,
                     DockSettings::recordPaneDockGeometry(),
                     DockSettings::recordPaneDockFloating(),
                     DockSettings::recordPaneDockVisible());

    return m_recordPaneDock;
}

void DockCreationService::createAnalysisDocks()
{
    if (!m_deps.thinking || !m_deps.consideration || !m_deps.usiLog || !m_deps.csaLog || !m_deps.comment
        || !m_deps.branchTree || !m_mainWindow) {
        qWarning() << "[DockCreationService] createAnalysisDocks: a panel or mainWindow is null";
        return;
    }

    // ドック共通の設定を適用するヘルパー
    auto setupDock = [this](QDockWidget* dock, QWidget* content, const QString& title, const QString& objectName,
                            bool layoutBoundary = false) {
        dock->setObjectName(objectName);
        // 思考情報のように中身の大きさが頻繁に変わるドックは、ドックの大きさの条件が変わらない
        // 限り、メインウィンドウの配置（すべてのドックのタイトルの大きさの計算を含む）をやり直さない
        dock->setWidget(layoutBoundary ? new LayoutBoundaryWidget(content, dock) : content);
        // 思考・ログなど頻繁に更新する中身の描き直しを、ドックのタイトル（Fusion の
        // アイコンの読み込みを含む）とメインウィンドウまで広げない
        OpaqueBackground::keep(content);
        dock->setMinimumWidth(0);
        dock->setMinimumHeight(100);
        dock->setFeatures(
            QDockWidget::DockWidgetMovable |
            QDockWidget::DockWidgetFloatable |
            QDockWidget::DockWidgetClosable);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        // タイトルバーの高さと色を設定（ドラッグ領域を認識しやすくする）
        dock->setStyleSheet(QStringLiteral(
            "QDockWidget::title { padding: 6px 8px; background-color: #edf0f3; }"));
        m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, dock);
        addToggleActionToMenu(dock, title);
    };

    // 0. 対局情報ドック
    if (m_deps.gameInfo) {
        m_gameInfoDock = new QDockWidget(tr("対局情報"), m_mainWindow);
        setupDock(m_gameInfoDock, m_deps.gameInfo,
                  tr("対局情報"), QStringLiteral("GameInfoDock"));
    }

    // 1. 思考ドック
    m_thinkingDock = new QDockWidget(tr("思考"), m_mainWindow);
    setupDock(m_thinkingDock, m_deps.thinking, tr("思考"), QStringLiteral("ThinkingDock"), true);

    // 2. 検討ドック
    m_considerationDock = new QDockWidget(tr("検討"), m_mainWindow);
    QWidget* considerationPage = m_deps.consideration->buildUi(m_considerationDock);
    setupDock(m_considerationDock, considerationPage, tr("検討"), QStringLiteral("ConsiderationDock"), true);

    // 3. USI通信ログドック
    m_usiLogDock = new QDockWidget(tr("USI通信ログ"), m_mainWindow);
    QWidget* usiLogPage = m_deps.usiLog->buildUi(m_usiLogDock);
    setupDock(m_usiLogDock, usiLogPage, tr("USI通信ログ"), QStringLiteral("UsiLogDock"));

    // 4. CSA通信ログドック
    m_csaLogDock = new QDockWidget(tr("CSA通信ログ"), m_mainWindow);
    QWidget* csaLogPage = m_deps.csaLog->buildUi(m_csaLogDock);
    setupDock(m_csaLogDock, csaLogPage, tr("CSA通信ログ"), QStringLiteral("CsaLogDock"));

    // 5. 棋譜コメントドック
    m_commentDock = new QDockWidget(tr("棋譜コメント"), m_mainWindow);
    QWidget* commentPage = m_deps.comment->buildCommentUi(m_commentDock);
    setupDock(m_commentDock, commentPage, tr("棋譜コメント"), QStringLiteral("CommentDock"));

    // 6. 分岐ツリードック
    m_branchTreeDock = new QDockWidget(tr("分岐ツリー"), m_mainWindow);
    QWidget* branchTreePage = m_deps.branchTree->buildView(m_branchTreeDock);
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
}

QDockWidget* DockCreationService::createMenuWindowDock()
{
    if (!m_deps.menuWindow || !m_mainWindow) {
        qWarning() << "[DockCreationService] createMenuWindowDock: menuWindowController or mainWindow is null";
        return nullptr;
    }

    m_deps.menuWindow->ensureMenuWindow();
    MenuWindow* menuWindow = m_deps.menuWindow->menuWindow();
    if (!menuWindow) {
        qWarning() << "[DockCreationService] createMenuWindowDock: MenuWindow is null";
        return nullptr;
    }

    m_menuWindowDock = new QDockWidget(tr("メニュー"), m_mainWindow);
    setupDockFeatures(m_menuWindowDock, QStringLiteral("MenuWindowDock"));
    m_menuWindowDock->setWidget(menuWindow);

    m_mainWindow->addDockWidget(Qt::RightDockWidgetArea, m_menuWindowDock);

    // 棋譜ドックとタブ化
    if (m_recordPaneDock) {
        m_mainWindow->tabifyDockWidget(m_recordPaneDock, m_menuWindowDock);
    }

    addToggleActionToMenu(m_menuWindowDock, tr("メニュー"));

    restoreDockState(m_menuWindowDock,
                     DockSettings::menuWindowDockGeometry(),
                     DockSettings::menuWindowDockFloating(),
                     DockSettings::menuWindowDockVisible());

    return m_menuWindowDock;
}

QDockWidget* DockCreationService::createJosekiWindowDock()
{
    if (!m_deps.joseki || !m_mainWindow) {
        qWarning() << "[DockCreationService] createJosekiWindowDock: josekiWindow or mainWindow is null";
        return nullptr;
    }

    m_deps.joseki->ensureJosekiWindow();
    JosekiWindow* josekiWindow = m_deps.joseki->josekiWindow();
    if (!josekiWindow) {
        qWarning() << "[DockCreationService] createJosekiWindowDock: JosekiWindow is null";
        return nullptr;
    }

    m_josekiWindowDock = new QDockWidget(tr("定跡"), m_mainWindow);
    setupDockFeatures(m_josekiWindowDock, QStringLiteral("JosekiWindowDock"));
    // 1手ごとに表と局面のラベルが変わる。思考ドックと同じく、ドックの大きさの条件が変わらない
    // 限りメインウィンドウの配置をやり直さず、中身の描き直しをドックのタイトルまで広げない
    m_josekiWindowDock->setWidget(new LayoutBoundaryWidget(josekiWindow, m_josekiWindowDock));
    OpaqueBackground::keep(josekiWindow);
    josekiWindow->setDockWidget(m_josekiWindowDock);

    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, m_josekiWindowDock);

    // 下部タブエリアにタブ化
    if (m_branchTreeDock) {
        m_mainWindow->tabifyDockWidget(m_branchTreeDock, m_josekiWindowDock);
    }

    addToggleActionToMenu(m_josekiWindowDock, tr("定跡"));

    restoreDockState(m_josekiWindowDock,
                     DockSettings::josekiWindowDockGeometry(),
                     DockSettings::josekiWindowDockFloating(),
                     DockSettings::josekiWindowDockVisible());

    return m_josekiWindowDock;
}

QDockWidget* DockCreationService::createAnalysisResultsDock()
{
    if (!m_deps.analysisResults || !m_mainWindow) {
        qWarning() << "[DockCreationService] createAnalysisResultsDock: analysisResults or mainWindow is null";
        return nullptr;
    }

    m_analysisResultsDock = new QDockWidget(tr("棋譜解析"), m_mainWindow);
    setupDockFeatures(m_analysisResultsDock, QStringLiteral("AnalysisResultsDock"));
    m_analysisResultsDock->setWidget(m_deps.analysisResults->containerWidget());

    m_deps.analysisResults->setDockWidget(m_analysisResultsDock);

    m_mainWindow->addDockWidget(Qt::BottomDockWidgetArea, m_analysisResultsDock);

    // 分岐ツリードックの後ろにタブ化
    if (m_branchTreeDock) {
        m_mainWindow->tabifyDockWidget(m_branchTreeDock, m_analysisResultsDock);
    }

    addToggleActionToMenu(m_analysisResultsDock, tr("棋譜解析"));

    restoreDockState(m_analysisResultsDock,
                     DockSettings::kifuAnalysisResultsDockGeometry(),
                     DockSettings::kifuAnalysisResultsDockFloating(),
                     DockSettings::kifuAnalysisResultsDockVisible());

    return m_analysisResultsDock;
}
