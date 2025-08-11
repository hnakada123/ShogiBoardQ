#include "elidelabel.h"
#include <QPainter>
#include <QMouseEvent>

ElideLabel::ElideLabel(QWidget* parent)
    : QLabel(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    // タイマーでオフセットを進める
    connect(&m_timer, &QTimer::timeout, this, [this]{
        m_offset += m_pxPerTick;
        update();
    });
}

void ElideLabel::setFullText(const QString& t) {
    if (m_fullText == t) return;
    m_fullText = t;
    setToolTip(m_fullText);
    updateElidedText();
}

QString ElideLabel::fullText() const { return m_fullText; }

void ElideLabel::setElideMode(Qt::TextElideMode m) {
    if (m_mode == m) return;
    m_mode = m;
    updateElidedText();
}

Qt::TextElideMode ElideLabel::elideMode() const { return m_mode; }

void ElideLabel::setSlideOnHover(bool on) { m_slideOnHover = on; startSlideIfNeeded(); }
void ElideLabel::setSlideSpeed(int pxPerTick) { m_pxPerTick = qMax(1, pxPerTick); }
void ElideLabel::setSlideInterval(int ms) { m_intervalMs = qMax(1, ms); if (m_timer.isActive()) m_timer.start(m_intervalMs); }
void ElideLabel::setUnderline(bool on) { m_underline = on; update(); }
void ElideLabel::setManualPanEnabled(bool on) { m_manualPan = on; update(); }

QSize ElideLabel::sizeHint() const {
    QSize sz = QLabel::sizeHint();
    // 高さはフォントに合わせて、幅はレイアウト側で決まる想定
    return sz;
}

void ElideLabel::updateElidedText() {
    QFontMetrics fm(font());
    m_elidedText = fm.elidedText(m_fullText, m_mode, contentsRect().width());
    update();
    startSlideIfNeeded();
}

bool ElideLabel::isOverflowing() const {
    QFontMetrics fm(font());
    return fm.horizontalAdvance(m_fullText) > contentsRect().width();
}

void ElideLabel::startSlideIfNeeded() {
    if (m_slideOnHover && underMouse() && isOverflowing() && !m_dragging) {
        if (!m_timer.isActive()) {
            m_timer.start(m_intervalMs);
            setCursor(m_manualPan ? Qt::OpenHandCursor : cursor());
        }
    } else {
        stopSlide();
    }
}

void ElideLabel::stopSlide() {
    if (m_timer.isActive()) m_timer.stop();
    if (!m_dragging) setCursor(Qt::ArrowCursor);
}

#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
void ElideLabel::enterEvent(QEnterEvent*) { startSlideIfNeeded(); }
#else
void ElideLabel::enterEvent(QEvent*) { startSlideIfNeeded(); }
#endif

void ElideLabel::leaveEvent(QEvent*) {
    stopSlide();
    m_offset = 0;
    update();
}

void ElideLabel::resizeEvent(QResizeEvent* e) {
    QLabel::resizeEvent(e);
    updateElidedText();
}

void ElideLabel::mousePressEvent(QMouseEvent* ev) {
    if (m_manualPan && ev->button() == Qt::LeftButton && isOverflowing()) {
        m_dragging = true;
        m_lastDragX = ev->pos().x();
        setCursor(Qt::ClosedHandCursor);
        stopSlide(); // ドラッグ中は自動スクロール停止
        ev->accept();
        return;
    }
    QLabel::mousePressEvent(ev);
}

void ElideLabel::mouseMoveEvent(QMouseEvent* ev) {
    if (m_dragging) {
        int dx = ev->pos().x() - m_lastDragX;
        m_lastDragX = ev->pos().x();
        m_offset = qMax(0, m_offset - dx); // 右へドラッグで左へスクロール
        update();
        ev->accept();
        return;
    }
    QLabel::mouseMoveEvent(ev);
}

void ElideLabel::mouseReleaseEvent(QMouseEvent* ev) {
    if (m_dragging && ev->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(underMouse() ? Qt::OpenHandCursor : Qt::ArrowCursor);
        startSlideIfNeeded(); // 必要なら自動スクロール再開
        ev->accept();
        return;
    }
    QLabel::mouseReleaseEvent(ev);
}

void ElideLabel::paintEvent(QPaintEvent*){
    QPainter p(this);
    p.setRenderHint(QPainter::TextAntialiasing, true);

    const QRect cr = contentsRect();
    QFontMetrics fm(font());
    const int textW = fm.horizontalAdvance(m_fullText);
    const int baseY = cr.y() + (cr.height() + fm.ascent() - fm.descent())/2;

    const bool overflow = textW > cr.width();

    if ((m_timer.isActive() || m_dragging) && overflow) {
        // 連続ループ描画
        const int startX = cr.x() + 2;  // 少し余白
        const int period = textW + m_gap;
        const int off = (m_offset % period + period) % period;

        p.drawText(startX - off, baseY, m_fullText);
        p.drawText(startX - off + period, baseY, m_fullText);
    } else {
        // 通常はエリプシス描画（中央・左・右など QLabel の alignment() を利用）
        p.drawText(cr, alignment() | Qt::AlignVCenter, overflow ? m_elidedText : m_fullText);
    }

    if (m_underline) {
        QPen pen(palette().color(QPalette::Mid));
        p.setPen(pen);
        p.drawLine(cr.left(), cr.bottom()-1, cr.right(), cr.bottom()-1);
    }
}
