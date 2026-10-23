#ifndef APPTOOLTIPFILTER_H
#define APPTOOLTIPFILTER_H

/// @file apptooltipfilter.h
/// @brief アプリケーションツールチップフィルタクラスの定義

#include <QObject>

class QWidget;
class QEvent;
class GlobalToolTip;

/**
 * @brief ツールバーのボタンのツールチップを自前の GlobalToolTip で出すイベントフィルタ
 *
 * 対象のウィジェット（ツールバーのボタン）に installEventFilter で付け、
 * ToolTip イベントの発生・消滅に合わせて GlobalToolTip を表示／非表示にする。
 * シグナルは持たないため Q_OBJECT は不要。
 */
class AppToolTipFilter : public QObject {
public:
    /// イベントフィルタを初期化し、内部で使用する GlobalToolTip を生成する
    explicit AppToolTipFilter(QWidget* parent = nullptr);

    /// ツールチップの文字サイズ（pt）
    void setPointSizeF(qreal pt);

    /// コンパクト表示（フォント／余白／角丸を小さめにする。既定：true）
    void setCompact(bool on = true);

protected:
    /// QEvent::ToolTip を横取りして GlobalToolTip を表示し（標準の QToolTip は出さない）、
    /// Leave/Hide/Close/FocusOut/WindowDeactivate/MouseButtonPress では非表示にする
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    GlobalToolTip* m_tip = nullptr;   ///< parent のツリーにぶら下げる
};

#endif // APPTOOLTIPFILTER_H
