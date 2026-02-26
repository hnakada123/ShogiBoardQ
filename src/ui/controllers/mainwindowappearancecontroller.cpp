/// @file mainwindowappearancecontroller.cpp
/// @brief UI外観/ウィンドウ表示制御コントローラの実装

#include "mainwindowappearancecontroller.h"

#include <QFontDatabase>
#include <QMainWindow>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

#include "apptooltipfilter.h"
#include "matchcoordinator.h"
#include "settingsservice.h"
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
    bool visible = SettingsService::toolbarVisible();
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
    for (QToolBar* tb : toolbars) {
        const auto buttons = tb->findChildren<QToolButton*>();
        for (QToolButton* b : buttons) {
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
    const bool flipped = !view->getFlipMode();
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
    SettingsService::setToolbarVisible(visible);
}

void MainWindowAppearanceController::onTabCurrentChanged(int index)
{
    SettingsService::setLastSelectedTabIndex(index);
    qCDebug(lcApp).noquote() << "onTabCurrentChanged: saved tab index =" << index;
}
