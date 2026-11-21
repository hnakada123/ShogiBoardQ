/// @file p2pchoicecard.cpp
/// @brief 選択肢の枠の実装

#include "p2pchoicecard.h"

#include <QGridLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRadioButton>
#include <QStyle>
#include <QVBoxLayout>

#include "dialogutils.h"

namespace {

constexpr int kPadding = 9;
constexpr qreal kRadius = 5.0;

} // namespace

P2pChoiceCard::P2pChoiceCard(const QString& title, QWidget* parent)
    : QFrame(parent)
{
    setFrameShape(QFrame::NoFrame);
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPadding, kPadding, kPadding, kPadding);
    layout->setSpacing(4);
    m_radio = new QRadioButton(title, this);
    QFont bold = m_radio->font();
    bold.setBold(true);
    m_radio->setFont(bold);
    layout->addWidget(m_radio);
    m_grid = new QGridLayout;
    // 説明は、ラジオボタンの文字の位置にそろえる
    const int indent = style()->pixelMetric(QStyle::PM_ExclusiveIndicatorWidth, nullptr, m_radio)
                       + style()->pixelMetric(QStyle::PM_RadioButtonLabelSpacing, nullptr, m_radio);
    m_grid->setContentsMargins(indent, 0, 0, 0);
    m_grid->setHorizontalSpacing(8);
    m_grid->setVerticalSpacing(2);
    m_grid->setColumnStretch(1, 1);
    layout->addLayout(m_grid);
    layout->addStretch();
    connect(m_radio, &QRadioButton::toggled, this, &P2pChoiceCard::onToggled);
}

void P2pChoiceCard::setRows(const QList<Row>& rows)
{
    if (m_labels.size() != rows.size() * 2) {
        qDeleteAll(m_labels);
        m_labels.clear();
        for (int row = 0; row < rows.size(); ++row) {
            auto* label = new QLabel(this);
            label->setTextFormat(Qt::PlainText);
            label->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            auto* value = new QLabel(this);
            value->setTextFormat(Qt::PlainText);
            value->setWordWrap(true);
            value->setAlignment(Qt::AlignLeft | Qt::AlignTop);
            m_labels << label << value;
        }
    }
    for (int row = 0; row < rows.size(); ++row) {
        QLabel* label = m_labels.at(row * 2);
        QLabel* value = m_labels.at(row * 2 + 1);
        const bool wide = rows.at(row).label.isEmpty();
        label->setText(rows.at(row).label);
        label->setVisible(!wide);
        DialogUtils::setWrappedText(value, rows.at(row).value);
        m_grid->removeWidget(label);
        m_grid->removeWidget(value);
        if (wide) {
            m_grid->addWidget(value, row, 0, 1, 2);
        } else {
            m_grid->addWidget(label, row, 0);
            m_grid->addWidget(value, row, 1);
        }
    }
}

void P2pChoiceCard::setLocked(bool locked)
{
    m_locked = locked;
    m_radio->setEnabled(!locked);
}

void P2pChoiceCard::mousePressEvent(QMouseEvent* event)
{
    if (!m_locked && event->button() == Qt::LeftButton) {
        m_radio->setChecked(true);
        m_radio->setFocus(Qt::MouseFocusReason);
    }
    QFrame::mousePressEvent(event);
}

void P2pChoiceCard::onToggled()
{
    update();
}

void P2pChoiceCard::paintEvent(QPaintEvent* event)
{
    QFrame::paintEvent(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const bool selected = m_radio->isChecked();
    // 選んだ枠は、強調色の太い線と薄い塗りで示す（色だけでなく線の太さも変える）
    const QColor accent = palette().color(QPalette::Highlight);
    QColor fill = accent;
    fill.setAlpha(selected ? 28 : 0);
    const qreal width = selected ? 2.0 : 1.0;
    painter.setPen(QPen(selected ? accent : palette().color(QPalette::Mid), width));
    painter.setBrush(fill);
    const qreal inset = width / 2.0;
    painter.drawRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset), kRadius, kRadius);
}
