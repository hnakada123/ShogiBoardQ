#ifndef GLOBALTOOLTIP_H
#define GLOBALTOOLTIP_H

#pragma once
#include <QFrame>

class QLabel;
class QPoint;
class QString;

class GlobalToolTip : public QFrame {
public:
    explicit GlobalToolTip(QWidget* parent = nullptr);

    // ツールチップの文字サイズ（pt）を後から変更したい場合に呼ぶ
    void setPointSizeF(qreal pt);

    // ツールチップを表示
    void showText(const QPoint& globalPos, const QString& plainText);

    // ツールチップを非表示
    void hideTip();

    void setCompact(bool on = true);

private:
    QLabel* m_label = nullptr;
};

#endif // GLOBALTOOLTIP_H
