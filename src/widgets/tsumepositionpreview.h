#ifndef TSUMEPOSITIONPREVIEW_H
#define TSUMEPOSITIONPREVIEW_H

#include <QWidget>
#include "position.h"

/// 一覧専用の軽量な初期局面表示。操作用の盤や対局コントローラは生成しない。
class TsumePositionPreview : public QWidget
{
    Q_OBJECT
public:
    explicit TsumePositionPreview(const QString& sfen, QWidget* parent = nullptr);
    QSize sizeHint() const override { return {330, 260}; }
protected:
    void paintEvent(QPaintEvent*) override;
private:
    void appearanceChanged();
    shogi::Position m_position;
};

#endif
