/// @file mainwindowappearancecontroller.cpp
/// @brief UI外観/ウィンドウ表示制御コントローラの実装

#include "mainwindowappearancecontroller.h"

#include <QEvent>
#include <QLayout>
#include <QMainWindow>
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
        // 通常表示の固定幅が全画面表示・最大化時のサイズ変更を妨げないようにする（macOS の最大化は fillsScreen() の説明）。
        if (fillsScreen()) {
            m_mainWindow->setMinimumWidth(0);
            m_mainWindow->setMaximumWidth(QWIDGETSIZE_MAX);
        }
        // 通常表示に戻った際は、現在の盤面・ドックに合わせた横幅に戻す。
        scheduleWindowSizeUpdate();
    }
    // Resize: 決めた大きさと違う大きさにされたとき（updateWindowSize の説明）に合わせ直す
    if (watched == m_mainWindow
        && (event->type() == QEvent::LayoutRequest || event->type() == QEvent::Show
            || event->type() == QEvent::Resize)) {
        scheduleWindowSizeUpdate();
    }
    return QObject::eventFilter(watched, event);
}

bool MainWindowAppearanceController::fillsScreen() const
{
    if (m_mainWindow->isFullScreen()) return true;
#ifdef Q_OS_MACOS
    // macOS の最大化（ズーム）は、最大の大きさまでしか広げない: 横幅を固定したウィンドウは、幅はそのままで
    // 高さだけが画面いっぱいになる。そのうえ、画面より高いウィンドウの幅を変えただけでも最大化と報告される
    // （上端のドックを盤の横へ移した直後など。OS は画面に収まる高さに縮め、幅も前の値に戻す）。ここで横幅の
    // 固定をやめると、配置に足りない幅のまま残り、右のドックがウィンドウの外に出て見えなくなる
    return false;
#else
    return m_mainWindow->isMaximized();
#endif
}

void MainWindowAppearanceController::scheduleWindowSizeUpdate()
{
    if (m_windowSizeUpdatePending) return;
    m_windowSizeUpdatePending = true;
    QTimer::singleShot(0, this, &MainWindowAppearanceController::updateWindowSize);
}

void MainWindowAppearanceController::updateWindowSize()
{
    m_windowSizeUpdatePending = false;
    // ドックのドラッグ中は変えない: Qt はドラッグが終わるまでウィンドウの大きさの変更を配置に反映しないので、
    // 途中で縮めると中身が元の大きさのまま残る。終わったら endDockDrag() から合わせ直す
    if (m_dockDragActive) return;
    if (!m_mainWindow || !m_mainWindow->isVisible() || !m_mainWindow->layout()) return;
    // setFixedWidth() によるリサイズは全画面表示・最大化を解除してしまう。
    if (fillsScreen() || m_mainWindow->isMinimized()) return;

    // 棋譜の列幅・ドック配置の再計算後、内容に必要な横幅に固定する。
    // 高さは従来どおり変更でき、盤の拡縮やフォント変更時には横幅も追従する。
    QLayout* layout = m_mainWindow->layout();
    layout->activate();
    const int width = layout->minimumSize().width();
    if (width <= 0) return;
    // 高さは利用者が決める。今の配置に足りないときだけ、必要な高さにする
    const QSize size(width, qMax(m_mainWindow->height(), layout->totalMinimumSize().height()));
    if (m_mainWindow->minimumWidth() != width || m_mainWindow->maximumWidth() != width) {
        m_mainWindow->setFixedWidth(width);
        m_sizeCorrections = 0;
        if (m_mainWindow->size() != size) m_mainWindow->resize(size);
        return;
    }
    // Wayland では、ウィンドウが自分で変えた大きさを、コンポジターが古い大きさで戻すことがある（前面・後ろの
    // 切り替えなどの知らせが、行き違いで前の大きさを運んでくる。ドックを離した直後・切り離した直後に起きる）。
    // 決めた大きさと違っていたら合わせ直す。大きさを譲らない環境と押し合いにならないよう、回数を限る
    if (m_mainWindow->size() == size) {
        m_sizeCorrections = 0;
    } else if (m_sizeCorrections < kMaxSizeCorrections) {
        ++m_sizeCorrections;
        m_mainWindow->resize(size);
    }
}

void MainWindowAppearanceController::beginDockDrag()
{
    if (m_dockDragActive) return;
    m_dockDragActive = true;
    // Qt は、ドックを入れた配置の最小の大きさが今のウィンドウに収まる場所だけをドロップ先にする。横幅は配置の
    // 最小に固定しているので、そのままでは幅の増える場所（盤の横）に戻せず、高さの足りないウィンドウでは
    // 下段・上端にも入れられない。ドラッグの間だけ、盤の領域を縮めてよいことにする（盤は欠けて見える）。
    // ドックが入ったら、endDockDrag() で盤の領域を元に戻し、ウィンドウを必要な大きさへ広げる。
    // ドラッグの間はウィンドウの大きさを変えない: Wayland（KWin）は、大きさが変わったウィンドウを動かし、
    // その移動をドラッグの位置の知らせに反映しないので、ドロップ先がカーソルとずれる
    // （盤の領域の最小の大きさは、中の配置の最小の大きさから決まるので、大きさの方針を Ignored にして外す。
    // 最大は盤の大きさのままなので、盤より広がることはない）
    m_centralPolicyToRestore = m_central->sizePolicy();
    m_central->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_central->setMinimumSize(0, 0);
    // ドックをはめ込むアニメーションの間、Qt の配置は「ドックは入ったが、まだ浮動のウィンドウ」という
    // 途中の状態になり、必要な大きさが一時的に小さく出る。ドラッグの間はアニメーションを切り、
    // 離した時点ではめ込みが終わっているようにする
    m_dockAnimationToRestore = m_mainWindow->isAnimated();
    m_mainWindow->setAnimated(false);
}

void MainWindowAppearanceController::endDockDrag()
{
    if (!m_dockDragActive) return;
    m_dockDragActive = false;
    m_central->setSizePolicy(m_centralPolicyToRestore);
    fitCentralToBoard();
    m_mainWindow->setAnimated(m_dockAnimationToRestore);
    scheduleWindowSizeUpdate();
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

