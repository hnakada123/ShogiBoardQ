/// @file boardviewerwidget.cpp
/// @brief 盤と、拡大・縮小・回転・局面送りの操作をまとめた部品の実装

#include "boardviewerwidget.h"
#include "buttonstyles.h"
#include "opaquebackground.h"
#include "shogiboard.h"
#include "shogiview.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QShowEvent>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>

namespace {
QPushButton* makeButton(const QString& text, const QString& toolTip, const QString& style, QWidget* parent)
{
    auto* button = new QPushButton(text, parent);
    button->setToolTip(toolTip);
    button->setAccessibleName(toolTip);
    button->setStyleSheet(style);
    button->setAutoDefault(false);
    return button;
}
} // namespace

BoardViewerWidget::BoardViewerWidget(Features features, ButtonLabels labels, ShogiBoard* externalBoard,
                                     QWidget* parent)
    : QWidget(parent)
    , m_features(features)
    , m_board(externalBoard ? externalBoard : new ShogiBoard(this))
    , m_view(new ShogiView(this))
{
    setObjectName(QStringLiteral("boardViewer"));
    // 盤上の文字・対局者名は ShogiView がマスの大きさに合わせて調整する（ダイアログの文字サイズの対象外）
    m_view->setProperty("dialogFontScaleExcluded", true);
    m_view->chrome()->card(Turn::Black)->setName(tr("先手"));
    m_view->chrome()->card(Turn::White)->setName(tr("後手"));

    // 部品を何も持たない見本用は、盤とモデルだけを持つ。配置は呼び出し側が決め、
    // ShogiView::setBoard() で局面を渡して画像に描いて使う
    const bool bare = !(features & (ZoomToolbar | Navigation | ScrollArea));
    if (bare) return;

    m_view->setBoard(m_board);
    m_view->chrome()->setClockEnabled(false);
    // 拡大・縮小（ボタン・Ctrl+ホイール）で盤の大きさが変わったら、ウィンドウを盤に合わせる
    connect(m_view, &ShogiView::fieldSizeChanged, this, &BoardViewerWidget::onBoardZoomed);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    if (features & ZoomToolbar) buildToolbar(labels);
    if (features & ScrollArea) {
        m_scroll = new QScrollArea(this);
        m_scroll->setObjectName(QStringLiteral("boardScrollArea"));
        m_scroll->setFrameShape(QFrame::NoFrame);
        m_scroll->setAlignment(Qt::AlignCenter);
        m_scroll->setMinimumSize(300, 300);
        m_scroll->setWidget(m_view);
        m_scroll->installEventFilter(this);
        layout->addWidget(m_scroll, 1);
        m_fitTimer = new QTimer(this);
        m_fitTimer->setSingleShot(true);
        m_fitTimer->setInterval(300);
        connect(m_fitTimer, &QTimer::timeout, this, &BoardViewerWidget::onFitTimeout);
    } else {
        layout->addWidget(m_view, 1, Qt::AlignHCenter);
    }
    if (features & Navigation) buildNavigation(labels);
}

void BoardViewerWidget::buildToolbar(ButtonLabels labels)
{
    m_toolbar = new QHBoxLayout;
    m_toolbar->setSpacing(4);
    const bool symbols = labels == ButtonLabels::Symbols;
    const QString style = ButtonStyles::secondaryNeutral();
    // 絵文字の ➖➕ はカラー絵文字フォントにしか無く、Qt 6.7（Linux 版の AppImage）では
    // 代替されずに ☒ で表示される。通常の日本語フォントにある全角の －＋ を使う。
    m_reduce = makeButton(symbols ? QStringLiteral("－") : tr("縮小"), tr("将棋盤を縮小する"), style, this);
    m_enlarge = makeButton(symbols ? QStringLiteral("＋") : tr("拡大"), tr("将棋盤を拡大する"), style, this);
    m_flip = makeButton(tr("盤面の回転"), tr("盤面を回転する"), style, this);
    connect(m_reduce, &QPushButton::clicked, this, &BoardViewerWidget::reduceBoard);
    connect(m_enlarge, &QPushButton::clicked, this, &BoardViewerWidget::enlargeBoard);
    connect(m_flip, &QPushButton::clicked, this, &BoardViewerWidget::toggleFlip);
    m_toolbar->addWidget(m_reduce);
    m_toolbar->addWidget(m_enlarge);
    m_toolbar->addWidget(m_flip);
    m_toolbar->addStretch();
    static_cast<QVBoxLayout*>(layout())->addLayout(m_toolbar);
}

void BoardViewerWidget::buildNavigation(ButtonLabels labels)
{
    auto* mainLayout = static_cast<QVBoxLayout*>(layout());
    m_positionLabel = new QLabel(this);
    m_positionLabel->setObjectName(QStringLiteral("boardViewerPosition"));
    m_positionLabel->setWordWrap(true);
    m_positionLabel->setAlignment(Qt::AlignCenter);
    m_positionLabel->setStyleSheet(QStringLiteral("QLabel { font-weight: bold; padding: 4px; }"));
    // 局面を送るたびに描き直す。下にあるこの部品とダイアログまで同じ範囲を描き直さない
    OpaqueBackground::keep(m_positionLabel);
    mainLayout->addWidget(m_positionLabel);

    // ⏮⏭ は多くの日本語フォントに無く、代替フォントで小さく（環境によっては絵文字で）描かれるため、
    // ◀▶ と縦棒を組み合わせて4つのボタンの記号の大きさを揃える（棋譜欄の ▲| ▼| と同じ考え方）
    const bool symbols = labels == ButtonLabels::Symbols;
    const QString style = ButtonStyles::wideNavigationButton();
    m_first = makeButton(symbols ? QStringLiteral("|◀") : tr("最初"), tr("最初の局面に移動"), style, this);
    m_back = makeButton(symbols ? QStringLiteral("◀") : tr("前へ"), tr("前の局面に移動"), style, this);
    m_forward = makeButton(symbols ? QStringLiteral("▶") : tr("次へ"), tr("次の局面に移動"), style, this);
    m_last = makeButton(symbols ? QStringLiteral("▶|") : tr("最後"), tr("最後の局面に移動"), style, this);
    m_first->setMinimumWidth(80);
    m_back->setMinimumWidth(symbols ? 100 : 80);
    m_forward->setMinimumWidth(symbols ? 100 : 80);
    m_last->setMinimumWidth(80);
    connect(m_first, &QPushButton::clicked, this, &BoardViewerWidget::goFirst);
    connect(m_back, &QPushButton::clicked, this, &BoardViewerWidget::goBack);
    connect(m_forward, &QPushButton::clicked, this, &BoardViewerWidget::goForward);
    connect(m_last, &QPushButton::clicked, this, &BoardViewerWidget::goLast);
    auto* row = new QHBoxLayout;
    row->setSpacing(8);
    for (auto* button : {m_first, m_back, m_forward, m_last}) row->addWidget(button);
    mainLayout->addLayout(row);
    updateNavigationButtons();
}

void BoardViewerWidget::setNavigationToolTips(const QString& first, const QString& back,
                                              const QString& forward, const QString& last)
{
    if (!m_first) return;
    const QList<QPair<QPushButton*, QString>> tips = {{m_first, first}, {m_back, back}, {m_forward, forward}, {m_last, last}};
    for (const auto& tip : tips) {
        tip.first->setToolTip(tip.second);
        tip.first->setAccessibleName(tip.second);
    }
}

void BoardViewerWidget::setSfenList(const QStringList& sfens)
{
    m_sfens = sfens;
    m_index = 0;
    showCurrent();
}

QString BoardViewerWidget::currentSfen() const
{
    return (m_index >= 0 && m_index < m_sfens.size()) ? m_sfens.at(m_index) : QString();
}

void BoardViewerWidget::setCurrentIndex(int index)
{
    if (m_sfens.isEmpty()) return;
    const int clamped = qBound(0, index, count() - 1);
    if (clamped == m_index) return;
    m_index = clamped;
    showCurrent();
}

void BoardViewerWidget::goFirst() { setCurrentIndex(0); }
void BoardViewerWidget::goBack() { setCurrentIndex(m_index - 1); }
void BoardViewerWidget::goForward() { setCurrentIndex(m_index + 1); }
void BoardViewerWidget::goLast() { setCurrentIndex(count() - 1); }

void BoardViewerWidget::showCurrent()
{
    const QString sfen = currentSfen();
    if (!sfen.isEmpty()) {
        // 盤は変わったマスだけを描き直すので、全体の描き直しは頼まない
        // 手番の印は盤の手番に従う
        m_board->setSfen(sfen);
    }
    updateNavigationButtons();
    if (!sfen.isEmpty()) emit currentIndexChanged(m_index);
}

void BoardViewerWidget::updateNavigationButtons()
{
    if (!m_first) return;
    const bool canGoBack = !m_sfens.isEmpty() && m_index > 0;
    const bool canGoForward = !m_sfens.isEmpty() && m_index < count() - 1;
    m_first->setEnabled(canGoBack);
    m_back->setEnabled(canGoBack);
    m_forward->setEnabled(canGoForward);
    m_last->setEnabled(canGoForward);
}

void BoardViewerWidget::setPositionText(const QString& text)
{
    if (m_positionLabel) m_positionLabel->setText(text);
}

void BoardViewerWidget::setFlipped(bool flipped)
{
    m_view->setFlipMode(flipped);
    m_view->update();
}

bool BoardViewerWidget::isFlipped() const
{
    return m_view->flipMode();
}

void BoardViewerWidget::toggleFlip()
{
    setFlipped(!isFlipped());
    emit boardTransformed();
    emit flipToggled(isFlipped());
}

void BoardViewerWidget::enlargeBoard() { m_view->enlargeBoard(); }
void BoardViewerWidget::reduceBoard() { m_view->reduceBoard(); }

void BoardViewerWidget::onBoardZoomed()
{
    if (m_features & FitWindowOnZoom) fitToBoard();
    emit boardTransformed();
}

void BoardViewerWidget::fitToBoard()
{
    if (!m_scroll) {
        if (m_features & FitWindowOnZoom) window()->adjustSize();
        return;
    }
    // スクロール領域の中の盤は自動では大きさが変わらないため、マスの大きさに合わせる
    const QSize boardSize = m_view->sizeHint();
    // ウィンドウを合わせるのは表示してから。表示の前は部品の配置が決まっていないので、盤以外の部分の
    // 寸法が分からない（外れた大きさで1度描いてから、合わせ直して描くことになる）。表示するときに合わせる
    if (!(m_features & FitWindowOnZoom) || !m_shown) {
        m_view->resize(boardSize);
        return;
    }
    // 通常表示は盤全体が入る寸法を優先し、小さい画面ではスクロールで補う。
    // 拡大・縮小の直後はスクロールバーが出ていることがあるため、ビューポートではなく
    // スクロール領域（枠なし）の大きさから盤以外の部分の寸法を求める。
    QWidget* top = window();
    QLayout* topLayout = top->layout();
    if (topLayout) topLayout->activate();
    const QSize controls = top->size() - m_scroll->size();
    QSize target = boardSize + controls;
    // 折り返す部品（読み筋の帯など）の高さはウィンドウの幅で変わるので、合わせた後の幅での高さにする
    if (topLayout && topLayout->hasHeightForWidth())
        target.rheight() += topLayout->totalHeightForWidth(target.width()) - topLayout->totalHeightForWidth(top->width());
    target = target.boundedTo(availableWindowSize()).expandedTo(top->minimumSize());
    // 部品が詰められた配置（ウィンドウが推奨の大きさより小さい）で測ると、盤以外の部分を小さく見積もる。
    // 合わせた後で盤が収まっていなければ、足りない分だけ1回広げ直す（growToShow）
    m_fitRetryAllowed = true;

    const bool boardOverflows = boardSize.width() > m_scroll->width() || boardSize.height() > m_scroll->height();
    const bool windowFixed = top->isMaximized() || top->isFullScreen();
    if (target == top->size() || !boardOverflows || windowFixed) {
        // ウィンドウが変わらないか、盤が今のスクロール領域に収まる（縮小）: 盤を先に合わせる
        finishFit(boardSize);
        top->resize(target);
        // 広げ直せるのは、この合わせ直しの間だけ（利用者が後でウィンドウを縮めても広げない）
        if (m_fitRetryAllowed) m_fitTimer->start();
        return;
    }
    // 盤が今のスクロール領域より大きくなる（拡大・文字を大きくした後）: 盤を先に大きくすると、ウィンドウが
    // 広がるまでの間にスクロールバー付きで1度描き、広がってからもう1度描く。ウィンドウを先に広げ、
    // スクロール領域が広がったときに盤を合わせる（その間は盤を描かず、スクロールバーも出さない）
    holdBoard();
    m_pendingBoardSize = boardSize;
    top->resize(target);
    // ウィンドウの大きさの変化が届かない場合（ウィンドウマネージャが断るなど）に備える
    if (m_pendingBoardSize.isValid()) m_fitTimer->start();
}

QSize BoardViewerWidget::availableWindowSize() const
{
    return window()->screen()->availableGeometry().size() - QSize(40, 80);
}

bool BoardViewerWidget::growToShow(const QSize& boardSize)
{
    const QSize lacking = (boardSize - m_scroll->size()).expandedTo(QSize(0, 0));
    if (lacking.isNull()) return false;
    QWidget* top = window();
    if (top->isMaximized() || top->isFullScreen()) return false;
    const QSize target = (top->size() + lacking).boundedTo(availableWindowSize()).expandedTo(top->minimumSize());
    if (target == top->size()) return false;
    m_fitRetryAllowed = false;
    top->resize(target);
    return true;
}

void BoardViewerWidget::fitToBoardLater()
{
    // 配置が決まるまでの間に、縮んだスクロール領域で盤を描かせない
    if (m_scroll && (m_features & FitWindowOnZoom) && m_shown) holdBoard();
    QTimer::singleShot(0, this, &BoardViewerWidget::fitToBoard);
}

void BoardViewerWidget::holdBoard()
{
    if (m_boardHeld) return;
    m_boardHeld = true;
    m_view->setUpdatesEnabled(false);
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    // 合わせる処理が来ない場合に備える
    m_fitTimer->start();
}

void BoardViewerWidget::finishFit(QSize boardSize)
{
    m_fitTimer->stop();
    m_pendingBoardSize = QSize();
    m_view->resize(boardSize);
    if (!m_boardHeld) return;
    m_boardHeld = false;
    m_scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_scroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_view->setUpdatesEnabled(true);
}

void BoardViewerWidget::onFitTimeout()
{
    m_fitRetryAllowed = false;
    finishFit(m_view->sizeHint());
}

void BoardViewerWidget::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    if (event->spontaneous() || m_shown) return;
    // ここで初めて、部品が今のウィンドウの大きさで配置されている。ウィンドウが画面に出る前に盤に合わせる
    m_shown = true;
    if (m_scroll && (m_features & FitWindowOnZoom)) fitToBoard();
}

bool BoardViewerWidget::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_scroll && event->type() == QEvent::Resize && (m_pendingBoardSize.isValid() || m_fitRetryAllowed)) {
        // ウィンドウの大きさの変化がスクロール領域に届いた。盤がまだ収まらなければ、足りない分だけ1回広げ直して
        // 次の変化を待つ。収まっていれば（広げられなければ）、待たせていた盤の大きさを合わせる
        const QSize boardSize = m_pendingBoardSize.isValid() ? m_pendingBoardSize : m_view->size();
        const bool grown = m_fitRetryAllowed && growToShow(boardSize);
        if (!grown && m_pendingBoardSize.isValid()) finishFit(m_pendingBoardSize);
    }
    return QWidget::eventFilter(watched, event);
}
