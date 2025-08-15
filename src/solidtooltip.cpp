#include "solidtooltip.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPoint>
#include <QString>

SolidToolTip::SolidToolTip(QWidget* parent)
    : QFrame(parent, Qt::ToolTip | Qt::FramelessWindowHint)
{
    setAttribute(Qt::WA_ShowWithoutActivating);
    setFrameShape(QFrame::NoFrame);
    setFrameShadow(QFrame::Plain);

    // デフォルトの見た目（main.cpp の配色と合わせています）
    setStyleSheet(
        "background-color:#FFF9C4;"
        "color:#333;"
        "padding:10px 12px;"
        "border-radius:6px;"
        );

    auto* lay = new QHBoxLayout(this);
    lay->setContentsMargins(10, 10, 10, 10);

    m_label = new QLabel(this);
    m_label->setFrameStyle(QFrame::NoFrame);
    lay->addWidget(m_label);

    setPointSizeF(14.0); // 初期フォント
}

void SolidToolTip::setPointSizeF(qreal pt)
{
    QFont f = m_label->font();
    f.setPointSizeF(pt);
    m_label->setFont(f);

    // 一部環境のスタイル上書き対策として明示
    m_label->setStyleSheet(QStringLiteral("font-size:%1pt;").arg(pt));
    adjustSize();
}

void SolidToolTip::setCompact(bool on)
{
    const int padV   = on ? 4 : 6;
    const int padH   = on ? 4 : 6;
    const int radius = on ? 4 : 6;
    const qreal pt   = on ? 12.0 : 14.0;

    setPointSizeF(pt);

    if (auto* lay = qobject_cast<QHBoxLayout*>(layout()))
        lay->setContentsMargins(padH, padV, padH, padV);

    setStyleSheet(QStringLiteral(
                      "background-color:#FFF9C4;"
                      "color:#333;"
                      "padding:%1px %2px;"
                      "border-radius:%3px;"
                      ).arg(padV).arg(padH).arg(radius));

    adjustSize();
}

void SolidToolTip::showText(const QPoint& globalPos, const QString& plainText)
{
    // HTML解釈させない
    m_label->setText(plainText.toHtmlEscaped());
    adjustSize();

    // 少し右下にオフセット
    move(globalPos + QPoint(12, 16));
    show();
}

void SolidToolTip::hideTip()
{
    hide();
}
