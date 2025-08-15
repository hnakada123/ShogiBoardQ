#ifndef SOLIDTOOLTIP_H
#define SOLIDTOOLTIP_H

#pragma once
#include <QFrame>

class QLabel;
class QPoint;
class QString;

/**
 * 透過せず・枠なしで表示する自前ツールチップ
 * （標準の QToolTip が透ける環境の回避用）
 */
class SolidToolTip : public QFrame {
public:
    explicit SolidToolTip(QWidget* parent = nullptr);

    void setPointSizeF(qreal pt);                  // 文字サイズ(pt)
    void setCompact(bool on = true);               // 小さめ表示に切替（padding等も調整）
    void showText(const QPoint& globalPos, const QString& plainText); // 表示
    void hideTip();                                // 非表示

private:
    QLabel* m_label = nullptr;
};

#endif // SOLIDTOOLTIP_H
