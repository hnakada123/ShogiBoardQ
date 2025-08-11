#ifndef ELIDELABEL_H
#define ELIDELABEL_H

#include <QLabel>
#include <QTimer>

class ElideLabel : public QLabel
{
    Q_OBJECT
public:
    explicit ElideLabel(QWidget* parent = nullptr);

    void setFullText(const QString& text);
    QString fullText() const;

    void setElideMode(Qt::TextElideMode mode);
    Qt::TextElideMode elideMode() const;

    // 追加：ホバー自動スクロール / ドラッグ手動パン / 下線
    void setSlideOnHover(bool on);      // 幅不足のときだけホバーで自動スクロール
    void setSlideSpeed(int pxPerTick);  // 1〜（既定2）
    void setSlideInterval(int ms);      // 既定16ms（約60fps）
    void setUnderline(bool on);         // 下線を描く
    void setManualPanEnabled(bool on);  // 左ドラッグで手動パン

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    void enterEvent(QEnterEvent*) override;
#else
    void enterEvent(QEvent*) override;
#endif
    void leaveEvent(QEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;

private:
    void updateElidedText();
    void startSlideIfNeeded();
    void stopSlide();
    bool isOverflowing() const;

    QString m_fullText;
    QString m_elidedText;
    Qt::TextElideMode m_mode = Qt::ElideMiddle;

    // スライド用
    bool  m_slideOnHover = false;
    bool  m_manualPan    = false;
    bool  m_underline    = false;
    QTimer m_timer;
    int   m_offset       = 0;   // 描画オフセット(px)
    int   m_gap          = 24;  // ループ時の空白
    int   m_pxPerTick    = 2;   // 自動スクロール速度
    int   m_intervalMs   = 16;  // タイマー間隔
    int   m_lastDragX    = 0;
    bool  m_dragging     = false;
};

#endif // ELIDELABEL_H
