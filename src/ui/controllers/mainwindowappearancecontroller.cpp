/// @file mainwindowappearancecontroller.cpp
/// @brief UI外観/ウィンドウ表示制御コントローラの実装

#include "mainwindowappearancecontroller.h"

#include <QFontDatabase>
#include <QEvent>
#include <QMainWindow>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

#include "apptooltipfilter.h"
#include "matchcoordinator.h"
#include "appsettings.h"
#include "shogiview.h"
#include "timedisplaypresenter.h"

#include "logcategories.h"

MainWindowAppearanceController::MainWindowAppearanceController(QObject* parent)
    : QObject(parent)
{
}

void MainWindowAppearanceController::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void MainWindowAppearanceController::setupCentralWidgetContainer(QWidget* centralWidget)
{
    m_central = centralWidget;
    m_centralLayout = new QVBoxLayout(centralWidget);
    m_centralLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->setSpacing(0);

    m_mainWindow = qobject_cast<QMainWindow*>(centralWidget->parentWidget());
    if (m_mainWindow) {
        m_mainWindow->installEventFilter(this);
    }
}

bool MainWindowAppearanceController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_mainWindow
        && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show)) {
        scheduleWindowWidthUpdate();
    }
    return QObject::eventFilter(watched, event);
}

void MainWindowAppearanceController::scheduleWindowWidthUpdate()
{
    if (m_windowWidthUpdatePending) return;
    m_windowWidthUpdatePending = true;
    QTimer::singleShot(0, this, &MainWindowAppearanceController::updateWindowWidth);
}

void MainWindowAppearanceController::updateWindowWidth()
{
    m_windowWidthUpdatePending = false;
    if (!m_mainWindow || !m_mainWindow->isVisible() || !m_mainWindow->layout()) return;

    // 棋譜の列幅・ドック配置の再計算後、内容に必要な横幅に固定する。
    // 高さは従来どおり変更でき、盤の拡縮やフォント変更時には横幅も追従する。
    m_mainWindow->layout()->activate();
    const int width = m_mainWindow->layout()->minimumSize().width();
    if (width > 0
        && (m_mainWindow->minimumWidth() != width || m_mainWindow->maximumWidth() != width)) {
        m_mainWindow->setFixedWidth(width);
    }
}

void MainWindowAppearanceController::configureToolBarFromUi(QToolBar* toolBar, QAction* actionToolBar)
{
    m_toolBar = toolBar;
    if (!toolBar) return;

    toolBar->setIconSize(QSize(18, 18));
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setStyleSheet(
        "QToolBar{margin:0px; padding:0px; spacing:2px;}"
        "QToolButton{margin:0px; padding:2px;}"
        );

    // 保存された設定からツールバーの表示状態を復元
    bool visible = AppSettings::toolbarVisible();
    toolBar->setVisible(visible);

    // メニュー内でアイコンを非表示にしてチェックマークを表示
    if (actionToolBar) {
        actionToolBar->setIconVisibleInMenu(false);
        actionToolBar->setChecked(visible);
    }
}

void MainWindowAppearanceController::installAppToolTips(QMainWindow* mainWindow)
{
    if (!mainWindow) return;

    auto* tipFilter = new AppToolTipFilter(mainWindow);
    tipFilter->setPointSizeF(12.0);
    tipFilter->setCompact(true);

    const auto toolbars = mainWindow->findChildren<QToolBar*>();
    for (QToolBar* tb : std::as_const(toolbars)) {
        const auto buttons = tb->findChildren<QToolButton*>();
        for (QToolButton* b : std::as_const(buttons)) {
            b->installEventFilter(tipFilter);
        }
    }
}

void MainWindowAppearanceController::setupBoardInCenter()
{
    ShogiView* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    if (!view) {
        qCWarning(lcApp) << "setupBoardInCenter: m_shogiView is null!";
        return;
    }

    if (!m_centralLayout) {
        qCWarning(lcApp) << "setupBoardInCenter: m_centralLayout is null!";
        return;
    }

    // 将棋盤をセントラルウィジェットに配置
    m_centralLayout->addWidget(view);

    // セントラルウィジェットのサイズを将棋盤に合わせて固定
    if (m_central) {
        const QSize boardSize = view->sizeHint();
        m_central->setFixedSize(boardSize);
    }

    // 将棋盤を表示
    view->show();
}

void MainWindowAppearanceController::setupNameAndClockFonts()
{
    ShogiView* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    if (!view) return;

    auto* n1 = view->blackNameLabel();
    auto* n2 = view->whiteNameLabel();
    auto* c1 = view->blackClockLabel();
    auto* c2 = view->whiteClockLabel();
    if (!n1 || !n2 || !c1 || !c2) return;

    QFont nameFont = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    nameFont.setPointSize(12);
    nameFont.setWeight(QFont::DemiBold);

    QFont clockFont = QFontDatabase::systemFont(QFontDatabase::FixedFont);
    clockFont.setPointSize(16);
    clockFont.setWeight(QFont::DemiBold);

    n1->setFont(nameFont);
    n2->setFont(nameFont);
    c1->setFont(clockFont);
    c2->setFont(clockFont);
}

void MainWindowAppearanceController::onBoardSizeChanged(QSize fieldSize)
{
    Q_UNUSED(fieldSize)

    ShogiView* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    if (m_central && view) {
        const QSize boardSize = view->sizeHint();
        m_central->setFixedSize(boardSize);
    }
}

void MainWindowAppearanceController::performDeferredEvalChartResize()
{
    ShogiView* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    if (view) {
        onBoardSizeChanged(view->fieldSize());
    }
}

void MainWindowAppearanceController::flipBoardAndUpdatePlayerInfo()
{
    qCDebug(lcApp) << "flipBoardAndUpdatePlayerInfo ENTER";
    ShogiView* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    if (!view) return;

    // 盤の表示向きをトグル
    const bool flipped = !view->flipMode();
    view->setFlipMode(flipped);
    if (flipped) view->setPiecesFlip();
    else         view->setPieces();

    TimeDisplayPresenter* tp = m_deps.timePresenter ? *m_deps.timePresenter : nullptr;
    if (tp && m_deps.lastP1Ms && m_deps.lastP2Ms && m_deps.lastP1Turn) {
        tp->onMatchTimeUpdated(
            *m_deps.lastP1Ms, *m_deps.lastP2Ms, *m_deps.lastP1Turn, /*urgencyMs(未使用)*/ 0);
    }

    view->update();
    qCDebug(lcApp) << "flipBoardAndUpdatePlayerInfo LEAVE";
}

void MainWindowAppearanceController::onBoardFlipped(bool /*nowFlipped*/)
{
    if (m_deps.bottomIsP1) {
        *m_deps.bottomIsP1 = !(*m_deps.bottomIsP1);
    }

    flipBoardAndUpdatePlayerInfo();
}

void MainWindowAppearanceController::onActionFlipBoardTriggered()
{
    MatchCoordinator* match = m_deps.match ? *m_deps.match : nullptr;
    if (match) match->flipBoard();
}

void MainWindowAppearanceController::onActionEnlargeBoardTriggered()
{
    ShogiView* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    if (!view) return;
    view->enlargeBoard(true);
}

void MainWindowAppearanceController::onActionShrinkBoardTriggered()
{
    ShogiView* view = m_deps.shogiView ? *m_deps.shogiView : nullptr;
    if (!view) return;
    view->reduceBoard(true);
}

void MainWindowAppearanceController::onToolBarVisibilityToggled(bool visible)
{
    if (m_toolBar) {
        m_toolBar->setVisible(visible);
    }
    AppSettings::setToolbarVisible(visible);
}

void MainWindowAppearanceController::onTabCurrentChanged(int index)
{
    AppSettings::setLastSelectedTabIndex(index);
    qCDebug(lcApp).noquote() << "onTabCurrentChanged: saved tab index =" << index;
}
