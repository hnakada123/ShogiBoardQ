#ifndef APPTOOLTIPFILTER_H
#define APPTOOLTIPFILTER_H

#pragma once
#include <QObject>

class QWidget;
class QMenuBar;
class QToolBar;
class QMenu;
class GlobalToolTip;

/**
 * メニューバー／ツールバー／ツールボタン／メニューのツールチップを
 * 自前の GlobalToolTip に置き換えるイベントフィルタ
 */
class AppToolTipFilter : public QObject {
public:
    explicit AppToolTipFilter(QWidget* parent = nullptr);

    // ツールチップの文字サイズ（pt）を調整
    void setPointSizeF(qreal pt);

    // ルート（通常は MainWindow）以下に一括で装着
    void installOn(QWidget* root);

    // 個別に装着したい場合
    void installOnMenuBar(QMenuBar* mb);
    void installOnToolBar(QToolBar* tb);
    void installOnMenu(QMenu* menu);
    void setCompact(bool on = true);

protected:
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    GlobalToolTip* m_tip = nullptr;
    QString pickTextForWidget(QWidget* w, const QPoint& localPos) const;
};

#endif // APPTOOLTIPFILTER_H
