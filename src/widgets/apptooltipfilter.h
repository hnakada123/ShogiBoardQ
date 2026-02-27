#ifndef APPTOOLTIPFILTER_H
#define APPTOOLTIPFILTER_H

/// @file apptooltipfilter.h
/// @brief アプリケーションツールチップフィルタクラスの定義

#pragma once

#include <QObject>

class QWidget;
class QMenuBar;
class QToolBar;
class QMenu;
class QEvent;
class QPoint;
class QString;
class GlobalToolTip;

/**
 * @brief アプリ全体のツールチップ表示を自前の GlobalToolTip に置き換えるイベントフィルタ
 *
 * 対象ウィジェット（メニューバー／ツールバー／ツールボタン／メニュー等）に install し、
 * ToolTip イベントの発生・消滅に合わせて GlobalToolTip を表示／非表示にする。
 * シグナルは持たないため Q_OBJECT は不要。
 *
 */
class AppToolTipFilter : public QObject {
public:
    /// イベントフィルタを初期化し、内部で使用する GlobalToolTip を生成する
    explicit AppToolTipFilter(QWidget* parent = nullptr);

    // ツールチップの文字サイズ（pt）を調整
    // 役割：内部の GlobalToolTip に委譲してフォントサイズを変更。
    void setPointSizeF(qreal pt);

    // ルート（通常は MainWindow）以下に一括で装着
    // 役割：root 自身と、既に存在する子（QMenuBar/QToolBar/QMenu/QToolButton）へ eventFilter を install。
    // 注意：動的に追加される子には自動では適用されないため、必要に応じて都度 installOn* を呼ぶ。
    void installOn(QWidget* root);

    // 個別に装着したい場合（ユーティリティ）
    // 役割：特定のウィジェットに対し、このフィルタをインストールする。
    void installOnMenuBar(QMenuBar* mb);
    void installOnToolBar(QToolBar* tb);
    void installOnMenu(QMenu* menu);

    // コンパクト表示切り替え（既定：true）
    // 役割：GlobalToolTip 側のフォント／余白／角丸を小さめに一括調整。
    void setCompact(bool on = true);

protected:
    // イベントフィルタ本体
    // 役割：QEvent::ToolTip を横取りして GlobalToolTip を表示。
    //       Leave/Hide/Close/FocusOut/WindowDeactivate/MouseButtonPress では非表示にする。
    // 注意：ツールチップを表示した場合は true を返し、標準の QToolTip を抑止。
    bool eventFilter(QObject* obj, QEvent* ev) override;

private:
    // 内部の GlobalToolTip インスタンス（所有：this の parent ツリーにぶら下げる）
    GlobalToolTip* m_tip = nullptr;

    // 表示するテキストを選ぶヘルパ
    // 役割：QMenuBar の場合は actionAt() を用いてアイテムごとに判定。
    //       アイコン無しアクションはツールチップを出さない（空文字を返す）。
    //       それ以外は QWidget::toolTip() を採用（trim 済み）。
    QString pickTextForWidget(QWidget* w, const QPoint& localPos) const;
};

#endif // APPTOOLTIPFILTER_H
