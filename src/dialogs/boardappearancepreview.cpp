#include "boardappearancepreview.h"
#include "boardappearance.h"
#include "boardviewerwidget.h"
#include "pieceimageprovider.h"
#include "shogiboard.h"
#include "shogiview.h"
#include "sfenutils.h"

#include <QPainter>

BoardAppearancePreview::BoardAppearancePreview(QWidget* parent)
    : QWidget(parent), m_viewer(new BoardViewerWidget(BoardViewerWidget::NoFeatures,
                                                      BoardViewerWidget::ButtonLabels::Symbols, nullptr, this))
{
    setObjectName(QStringLiteral("appearancePreview"));
    setAccessibleName(tr("選択した組み合わせのプレビュー"));
    setMinimumSize(300, 280);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_viewer->hide();
    m_viewer->view()->configureFixedSizing(70);
    m_viewer->view()->setClockEnabled(true);
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
    if (index == 0) m_viewer->board()->setSfen(SfenUtils::hirateSfen());
    else m_viewer->board()->setSfen(QStringLiteral("4k4/9/3+r+b+s+n+l+p/9/9/9/+P+L+N+S+B+R3/9/4K4 b 2GSNL8P2gsnl8p 1"));
    m_viewer->view()->applyBoardAndRender(m_viewer->board());
    m_viewer->view()->setActiveSide(true);
    m_viewer->view()->setBlackClockText(QStringLiteral("09:58"));
    m_viewer->view()->setWhiteClockText(QStringLiteral("10:00"));
    m_viewer->view()->resize(m_viewer->view()->sizeHint());
    refresh();
}

void BoardAppearancePreview::setFlipped(bool flipped)
{
    m_viewer->view()->setFlipMode(flipped);
    m_viewer->view()->applyBoardAndRender(m_viewer->board());
    m_viewer->view()->setActiveSide(true);
    refresh();
}

void BoardAppearancePreview::refresh()
{
    m_image = m_viewer->view()->toImage();
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
