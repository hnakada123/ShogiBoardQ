#include "boardappearancepreview.h"
#include "boardappearance.h"
#include "pieceimageprovider.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "sfenutils.h"

#include <QPainter>

BoardAppearancePreview::BoardAppearancePreview(QWidget* parent)
    : QWidget(parent), m_board(new ShogiBoard(9, 9, this)), m_view(new ShogiView(this))
{
    setObjectName(QStringLiteral("appearancePreview"));
    setAccessibleName(tr("選択した組み合わせのプレビュー"));
    setMinimumSize(300, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_view->hide();
    m_view->configureFixedSizing(70);
    m_view->setBlackPlayerName(tr("先手"));
    m_view->setWhitePlayerName(tr("後手"));
    m_view->setClockEnabled(true);
    // ShogiView の更新後に画像を取得する（同じ通知先への接続順を維持）。
    connect(&BoardAppearance::instance(), &BoardAppearance::colorsChanged, this, &BoardAppearancePreview::refresh);
    connect(&BoardAppearance::instance(), &BoardAppearance::visualsChanged, this, &BoardAppearancePreview::refresh);
    connect(&PieceImageProvider::instance(), &PieceImageProvider::styleChanged, this, &BoardAppearancePreview::refresh);
    setPosition(0);
}

QSize BoardAppearancePreview::sizeHint() const
{
    return {720, 560};
}

void BoardAppearancePreview::setPosition(int index)
{
    if (index == 0) m_board->setSfen(SfenUtils::hirateSfen());
    else m_board->setSfen(QStringLiteral("4k4/9/3+r+b+s+n+l+p/9/9/9/+P+L+N+S+B+R3/9/4K4 b 2GSNL8P2gsnl8p 1"));
    m_view->applyBoardAndRender(m_board);
    m_view->setActiveSide(true);
    m_view->setBlackClockText(QStringLiteral("09:58"));
    m_view->setWhiteClockText(QStringLiteral("10:00"));
    m_view->resize(m_view->sizeHint());
    refresh();
}

void BoardAppearancePreview::setFlipped(bool flipped)
{
    m_view->setFlipMode(flipped);
    m_view->applyBoardAndRender(m_board);
    m_view->setActiveSide(true);
    refresh();
}

void BoardAppearancePreview::refresh()
{
    m_image = m_view->toImage();
    update();
}

void BoardAppearancePreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.fillRect(rect(), BoardAppearance::instance().colors().background);
    if (m_image.isNull()) return;
    const QSize fitted = m_image.size().scaled(size() - QSize(16, 16), Qt::KeepAspectRatio);
    const QRect target(QPoint((width() - fitted.width()) / 2, (height() - fitted.height()) / 2), fitted);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    painter.drawImage(target, m_image);
}
