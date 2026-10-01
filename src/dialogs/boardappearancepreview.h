#ifndef BOARDAPPEARANCEPREVIEW_H
#define BOARDAPPEARANCEPREVIEW_H

#include <QImage>
#include <QWidget>

class ShogiBoard;
class ShogiView;

/// 実際の対局画面と同じ描画を使う、操作できない独立した見本盤。
class BoardAppearancePreview : public QWidget
{
    Q_OBJECT
public:
    explicit BoardAppearancePreview(QWidget* parent = nullptr);
    QSize sizeHint() const override;
    const QImage& image() const { return m_image; }

public slots:
    void setPosition(int index);
    void setFlipped(bool flipped);
    void refresh();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ShogiBoard* m_board;
    ShogiView* m_view;
    QImage m_image;
};

#endif
