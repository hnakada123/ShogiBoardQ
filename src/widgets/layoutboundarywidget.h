#ifndef LAYOUTBOUNDARYWIDGET_H
#define LAYOUTBOUNDARYWIDGET_H

/// @file layoutboundarywidget.h
/// @brief 中身の大きさの条件が変わったときだけ、親に配置のやり直しを伝える入れ物

#include <QWidget>

/**
 * @brief 中身の大きさの条件が変わったときだけ、親に配置のやり直しを伝える入れ物
 *
 * Qt のレイアウトは、中の部品の大きさが変わると、自分の大きさの条件（推奨の大きさ・最小の
 * 大きさなど）が変わらなくても親に配置のやり直しを伝える（QLayout::activate）。ドックの中身
 * では、その先のメインウィンドウの配置の計算（すべてのドックのタイトルの大きさの計算を含む）
 * まで毎回起きる。
 *
 * 中身を自分の大きさいっぱいに置く。中身が大きさの条件の変化を知らせたとき（updateGeometry）、
 * その場で前回と比べ、変わったときだけ親に伝える。伝える時点と、外から見た大きさの条件は、
 * 中身を親に直接置いた場合と同じ。中身は隠さずに使う。
 */
class LayoutBoundaryWidget : public QWidget
{
    Q_OBJECT

public:
    /// content を子にして、自分の大きさいっぱいに置く
    explicit LayoutBoundaryWidget(QWidget* content, QWidget* parent = nullptr);

    QWidget* content() const;
};

#endif // LAYOUTBOUNDARYWIDGET_H
