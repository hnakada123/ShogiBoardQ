/// @file p2pstatusstrip.cpp
/// @brief 接続の状態の帯の実装

#include "p2pstatusstrip.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

#include "dialogutils.h"

namespace {

constexpr int kMarkSize = 12;

/// 状態の印（接続中は塗った丸、接続の途中は半分塗った丸、それ以外は輪）
QPixmap markPixmap(P2pStatusStrip::State state, const QPalette& palette, qreal ratio)
{
    QPixmap pixmap(QSize(kMarkSize, kMarkSize) * ratio);
    pixmap.setDevicePixelRatio(ratio);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF circle(1.5, 1.5, kMarkSize - 3.0, kMarkSize - 3.0);
    const QColor online(0x1e, 0x7b, 0x45);
    const QColor failed(0xa8, 0x28, 0x1e);
    const QColor idle = palette.color(QPalette::WindowText);
    switch (state) {
    case P2pStatusStrip::State::Online:
        painter.setPen(Qt::NoPen);
        painter.setBrush(online);
        painter.drawEllipse(circle.adjusted(-1.0, -1.0, 1.0, 1.0));
        break;
    case P2pStatusStrip::State::Starting:
        painter.setPen(QPen(online, 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(circle);
        painter.setPen(Qt::NoPen);
        painter.setBrush(online);
        painter.drawPie(circle, 90 * 16, -180 * 16);
        break;
    case P2pStatusStrip::State::Failed:
        painter.setPen(QPen(failed, 2.0));
        painter.drawLine(QPointF(2.5, 2.5), QPointF(kMarkSize - 2.5, kMarkSize - 2.5));
        painter.drawLine(QPointF(kMarkSize - 2.5, 2.5), QPointF(2.5, kMarkSize - 2.5));
        break;
    case P2pStatusStrip::State::Offline:
        painter.setPen(QPen(idle, 2.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(circle);
        break;
    }
    return pixmap;
}

} // namespace

P2pStatusStrip::P2pStatusStrip(QWidget* parent)
    : QFrame(parent)
{
    setObjectName(QStringLiteral("p2pStatusStrip"));
    setFrameShape(QFrame::StyledPanel);
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(10, 6, 10, 6);
    layout->setSpacing(10);
    m_mark = new QLabel(this);
    m_mark->setFixedSize(kMarkSize, kMarkSize);
    layout->addWidget(m_mark, 0, Qt::AlignVCenter);
    auto* texts = new QVBoxLayout;
    texts->setSpacing(0);
    m_title = new QLabel(this);
    m_title->setObjectName(QStringLiteral("p2pStatus"));
    m_title->setTextFormat(Qt::PlainText);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    // 自分や相手のアドレス（IP）は出さない。招待がどの経路で届くかだけを文で示す
    m_detail = new QLabel(this);
    m_detail->setObjectName(QStringLiteral("p2pRouteInfo"));
    m_detail->setTextFormat(Qt::PlainText);
    m_detail->setWordWrap(true);
    texts->addWidget(m_title);
    texts->addWidget(m_detail);
    layout->addLayout(texts, 1);
    m_button = new QPushButton(this);
    m_button->setObjectName(QStringLiteral("p2pConnect"));
    m_button->setAutoDefault(false);
    layout->addWidget(m_button, 0, Qt::AlignVCenter);
}

void P2pStatusStrip::setStatus(State state, const QString& title, const QString& detail)
{
    if (!m_drawn || state != m_state) {
        m_state = state;
        m_drawn = true;
        m_mark->setPixmap(markPixmap(state, palette(), devicePixelRatioF()));
    }
    if (m_title->text() != title) m_title->setText(title);
    DialogUtils::setWrappedText(m_detail, detail);
}
