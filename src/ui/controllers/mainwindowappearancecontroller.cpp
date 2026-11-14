/// @file mainwindowappearancecontroller.cpp
/// @brief UI外観/ウィンドウ表示制御コントローラの実装

#include "mainwindowappearancecontroller.h"

#include <QDockWidget>
#include <QEvent>
#include <QLayout>
#include <QMainWindow>
#include <QStyle>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <utility>

#include "apptooltipfilter.h"
#include "appsettings.h"
#include "shogiview.h"
#include "timedisplaypresenter.h"

MainWindowAppearanceController::MainWindowAppearanceController(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
    , m_mainWindow(deps.window)
{
    Q_ASSERT(m_mainWindow && deps.centralWidget && m_deps.shogiView);
    setupCentralWidgetContainer(deps.centralWidget);
    configureToolBar(deps.toolBar);
    // 本体の盤のマスの大きさは保存した値から始める
    m_deps.shogiView->setSquareSize(AppSettings::squareSize());
    setupBoardInCenter();
    // 拡大・縮小（メニュー・Ctrl+ホイール）で盤の大きさが変わったら、中央の領域を合わせて大きさを保存する
    connect(m_deps.shogiView, &ShogiView::fieldSizeChanged, this, &MainWindowAppearanceController::onBoardZoomed);
}

void MainWindowAppearanceController::setupCentralWidgetContainer(QWidget* centralWidget)
{
    m_central = centralWidget;
    m_centralLayout = new QVBoxLayout(centralWidget);
    m_centralLayout->setContentsMargins(0, 0, 0, 0);
    m_centralLayout->setSpacing(0);

    m_mainWindow->installEventFilter(this);
}

bool MainWindowAppearanceController::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_mainWindow && event->type() == QEvent::WindowStateChange) {
        // 通常表示の固定幅が全画面表示・最大化時のサイズ変更を妨げないようにする。
        if (m_mainWindow->isFullScreen() || m_mainWindow->isMaximized()) {
            m_mainWindow->setMinimumWidth(0);
            m_mainWindow->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        // 通常表示に戻った際は、現在の盤面・ドックに合わせた横幅に戻す。
        scheduleWindowWidthUpdate();
    }
    // Resize: 固定した横幅と違う幅にされたとき（updateWindowWidth の説明）に合わせ直す
    if (watched == m_mainWindow
        && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show
            || event->type() == QEvent::Resize)) {
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
    // ドックのドラッグ中は変えない: Qt はドラッグが終わるまでウィンドウの大きさの変更を配置に反映しないので、
    // 途中で縮めると中身が元の幅のまま残る。終わったら endDockDrag() から固定し直す
    if (m_dockDragActive) return;
    if (!m_mainWindow || !m_mainWindow->isVisible() || !m_mainWindow->layout()) return;
    // setFixedWidth() によるリサイズは全画面表示・最大化を解除してしまう。
    if (m_mainWindow->isFullScreen() || m_mainWindow->isMaximized()
        || m_mainWindow->isMinimized()) return;

    // 棋譜の列幅・ドック配置の再計算後、内容に必要な横幅に固定する。
    // 高さは従来どおり変更でき、盤の拡縮やフォント変更時には横幅も追従する。
    m_mainWindow->layout()->activate();
    const int width = m_mainWindow->layout()->minimumSize().width();
    if (width <= 0) return;
    if (m_mainWindow->minimumWidth() != width || m_mainWindow->maximumWidth() != width) {
        m_mainWindow->setFixedWidth(width);
        m_widthCorrections = 0;
        return;
    }
    // Wayland では、ウィンドウが自分で変えた大きさを、コンポジターが古い大きさで戻すことがある（前面・後ろの
    // 切り替えなどの知らせが、行き違いで前の大きさを運んでくる。ドックを離した直後・切り離した直後に起きる）。
    // 固定した幅と違う幅になっていたら合わせ直す。大きさを譲らない環境と押し合いにならないよう、回数を限る
    if (m_mainWindow->width() == width) {
        m_widthCorrections = 0;
    } else if (m_widthCorrections < kMaxWidthCorrections) {
        ++m_widthCorrections;
        m_mainWindow->resize(width, m_mainWindow->height());
    }
}

void MainWindowAppearanceController::beginDockDrag(QDockWidget* dock)
{
    if (!m_dockDragActive) {
        m_dockDragActive = true;
        // ドックをはめ込むアニメーションの間、Qt の配置は「ドックは入ったが、まだ浮動のウィンドウ」という
        // 途中の状態になり、必要な横幅が一時的に小さく出る。ドラッグの間はアニメーションを切り、
        // 離した時点ではめ込みが終わっているようにする
        m_dockAnimationToRestore = m_mainWindow->isAnimated();
        m_mainWindow->setAnimated(false);
    }
    // 収まっているドックを引き出すときは、今の横幅にそのドックの分が入っている
    if (!dock || !dock->isFloating()) return;
    if (!m_mainWindow->isVisible() || !m_mainWindow->layout()) return;
    if (m_mainWindow->isFullScreen() || m_mainWindow->isMaximized() || m_mainWindow->isMinimized()) return;

    // Qt は、ドックを入れた配置が今のウィンドウに収まる場所だけをドロップ先にする。横幅は配置の最小に
    // 固定しているので、そのままでは幅の増える場所（盤の横・ほかのドックの隣や重ね）に戻せない。
    // ドロップ先を探し始める前に、ドックの最小の幅と区切りの分だけ広げる
    m_mainWindow->layout()->activate();
    const int separator = m_mainWindow->style()->pixelMetric(QStyle::PM_DockWidgetSeparatorExtent, nullptr, m_mainWindow);
    const int dockWidth = qMax(dock->minimumWidth(), dock->minimumSizeHint().width());
    const int width = m_mainWindow->layout()->minimumSize().width() + separator + dockWidth;
    if (width > m_mainWindow->width()) m_mainWindow->setFixedWidth(width);
}

void MainWindowAppearanceController::endDockDrag()
{
    if (!m_dockDragActive) return;
    m_dockDragActive = false;
    m_mainWindow->setAnimated(m_dockAnimationToRestore);
    scheduleWindowWidthUpdate();
}

void MainWindowAppearanceController::configureToolBar(QToolBar* toolBar)
{
    if (!toolBar) return;

    toolBar->setIconSize(QSize(20, 20));
    toolBar->setToolButtonStyle(Qt::ToolButtonIconOnly);
    toolBar->setStyleSheet(
        "QToolBar { background: #f7f8fa; border: none; border-bottom: 1px solid #d8dee5;"
        " margin: 0px; padding: 3px 6px; spacing: 3px; }"
        "QToolBar::separator { background: #d8dee5; width: 1px; margin: 4px 7px; }"
        "QToolButton { margin: 0px; padding: 4px; border: 1px solid transparent; border-radius: 4px; }"
        "QToolButton:hover { background: #e7edf3; }"
        "QToolButton:pressed, QToolButton:checked { background: #dce7f1; border-color: #a7bbce; }"
        "QToolButton:focus { border-color: #52799c; }"
        );

    // 保存された設定からツールバーの表示状態を復元
    toolBar->setVisible(AppSettings::toolbarVisible());
}

bool MainWindowAppearanceController::isToolBarVisible() const
{
    return m_deps.toolBar && !m_deps.toolBar->isHidden();
}

void MainWindowAppearanceController::installAppToolTips()
{
    auto* tipFilter = new AppToolTipFilter(m_mainWindow);
    tipFilter->setPointSizeF(12.0);
    tipFilter->setCompact(true);

    const auto toolbars = m_mainWindow->findChildren<QToolBar*>();
    for (QToolBar* tb : std::as_const(toolbars)) {
        const auto buttons = tb->findChildren<QToolButton*>();
        for (QToolButton* b : std::as_const(buttons)) {
            b->installEventFilter(tipFilter);
        }
    }
}

void MainWindowAppearanceController::setupBoardInCenter()
{
    ShogiView* view = m_deps.shogiView;
    m_centralLayout->addWidget(view);
    fitCentralToBoard();
    view->show();
}

void MainWindowAppearanceController::fitCentralToBoard()
{
    m_central->setFixedSize(m_deps.shogiView->sizeHint());
}

void MainWindowAppearanceController::onBoardZoomed()
{
    fitCentralToBoard();
    AppSettings::squareSize.set(m_deps.shogiView->squareSize());
}

void MainWindowAppearanceController::flipBoard()
{
    ShogiView* view = m_deps.shogiView;
    if (!view) return;

    // 盤の表示向きをトグル（駒の画像も向きを合わせる）
    view->setFlipMode(!view->flipMode());

    // 反転で付け直した手番・緊急度の見た目を、最後に受け取った時計の値で戻す
    if (TimeDisplayPresenter* tp = m_deps.timePresenter) tp->refresh();

    view->update();
}

void MainWindowAppearanceController::onActionEnlargeBoardTriggered()
{
    ShogiView* view = m_deps.shogiView;
    if (!view) return;
    view->enlargeBoard();
}

void MainWindowAppearanceController::onActionShrinkBoardTriggered()
{
    ShogiView* view = m_deps.shogiView;
    if (!view) return;
    view->reduceBoard();
}

void MainWindowAppearanceController::onToolBarVisibilityToggled(bool visible)
{
    if (m_deps.toolBar) m_deps.toolBar->setVisible(visible);
    AppSettings::toolbarVisible.set(visible);
}

