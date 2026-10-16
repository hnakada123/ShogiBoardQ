/// @file elidelabel.cpp
/// @brief テキスト省略表示ラベルクラスの実装

#include "elidelabel.h"
#include <QPainter>
#include <QTextLayout>

ElideLabel::ElideLabel(QWidget* parent)
    : QLabel(parent)
{
    // 横方向はレイアウトに合わせて広がり、縦方向はフォントに依存する固定高さにする。
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
}

void ElideLabel::setFullText(const QString& t)
{
    if (m_fullText == t) return;
    m_fullText = t;
    setProperty("fullText", m_fullText); // 汎用のUI検査でも省略前の文字列を取得できるようにする
    setToolTip(m_fullText);
    update();
}

QString ElideLabel::fullText() const { return m_fullText; }

void ElideLabel::setElideMode(Qt::TextElideMode m)
{
    if (m_mode == m) return;
    m_mode = m;
    update();
}

Qt::TextElideMode ElideLabel::elideMode() const { return m_mode; }

QStringList ElideLabel::displayLines() const
{
    if (!wordWrap() || m_fullText.isEmpty()) return {m_fullText};
    QTextLayout layout(m_fullText, font());
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    if (!line.isValid()) {
        layout.endLayout();
        return {m_fullText};
    }
    line.setLineWidth(qMax(1, contentsRect().width()));
    const int split = line.textLength();
    layout.endLayout();
    if (split >= m_fullText.size()) return {m_fullText};
    return {m_fullText.left(split).trimmed(), m_fullText.mid(split).trimmed()};
}

void ElideLabel::paintEvent(QPaintEvent* event)
{
    // ラベルの背景・枠線は QLabel に描画させ、文字だけを省略して描く。
    QLabel::paintEvent(event);
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect cr = contentsRect();
    p.setClipRect(cr);
    const QFontMetrics fm(font());
    const QStringList lines = displayLines();
    const QString& lastLine = lines.last();
    QRect lastRect = cr;
    if (lines.size() > 1) {
        const int top = cr.top() + qMax(0, (cr.height() - 2 * fm.height()) / 2);
        p.drawText(QRect(cr.left(), top, cr.width(), fm.height()),
                   static_cast<int>(alignment()) | Qt::AlignVCenter, lines.first());
        lastRect = QRect(cr.left(), top + fm.height(), cr.width(), fm.height());
    }
    // 最終行だけを必要に応じて省略（alignment は QLabel 設定を流用）
    const bool overflow = fm.horizontalAdvance(lastLine) > cr.width();
    p.drawText(lastRect, static_cast<int>(alignment()) | Qt::AlignVCenter,
               overflow ? fm.elidedText(lastLine, m_mode, cr.width()) : lastLine);
}
