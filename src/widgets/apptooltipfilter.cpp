/// @file apptooltipfilter.cpp
/// @brief アプリケーションツールチップフィルタクラスの実装

#include "apptooltipfilter.h"
#include "globaltooltip.h"

#include <QEvent>
#include <QHelpEvent>
#include <QWidget>
#include <QMenuBar>
#include <QToolBar>
#include <QToolButton>
#include <QMenu>
#include <QAction>

/*
 * AppToolTipFilter
 * アプリ全体で「標準の QToolTip」を抑止し、代わりに GlobalToolTip を表示するためのイベントフィルタ。
 * 役割：対象ウィジェット（メニューバー/ツールバー/ボタン/メニュー等）に install して、
 *       ToolTip イベント発生時に GlobalToolTip を表示・非表示する。
 * 備考：既存の子ウィジェットに一括装着するユーティリティ（installOn）を提供。
 */

// コンストラクタ。
// 役割：イベントフィルタ本体を生成し、描画用の GlobalToolTip を用意する。
AppToolTipFilter::AppToolTipFilter(QWidget* parent)
    : QObject(parent),
    m_tip(new GlobalToolTip(parent))
{
}

// 表示フォントサイズ（pt）の変更。
// 役割：内部の GlobalToolTip へ委譲してサイズを更新。
// 注意：m_tip が nullptr の場合は何もしない（安全弁）。
void AppToolTipFilter::setPointSizeF(qreal pt) {
    if (m_tip) m_tip->setPointSizeF(pt);
}

// 指定ルートウィジェット配下にイベントフィルタを一括装着。
// 役割：ルート自身＋既存の子（メニューバー/ツールバー/メニュー/ツールボタン）へ install。
// 注意：この時点で存在する子のみ対象。動的に追加される子は別途 install が必要。
void AppToolTipFilter::installOn(QWidget* root) {
    if (!root) return;

    // ルート自身
    root->installEventFilter(this);

    // 既存の子ウィジェットへ一括装着
    const auto menuBars = root->findChildren<QMenuBar*>();
    for (QMenuBar* mb : std::as_const(menuBars))
        installOnMenuBar(mb);

    const auto toolBars = root->findChildren<QToolBar*>();
    for (QToolBar* tb : std::as_const(toolBars))
        installOnToolBar(tb);

    const auto menus = root->findChildren<QMenu*>();
    for (QMenu* menu : std::as_const(menus))
        installOnMenu(menu);

    const auto buttons = root->findChildren<QToolButton*>();
    for (QToolButton* btn : std::as_const(buttons))
        btn->installEventFilter(this);
}

// メニューバーにイベントフィルタを装着。
// 役割：QMenuBar 自体の ToolTip イベントを捕捉できるようにする。
void AppToolTipFilter::installOnMenuBar(QMenuBar* mb) {
    if (!mb) return;
    mb->installEventFilter(this);
}

// ツールバーにイベントフィルタを装着。
// 役割：QToolBar 自体および既存の QToolButton にもフィルタを適用する。
// 注意：findChildren で取得したボタン群に対して const 反復で detach を避ける。
void AppToolTipFilter::installOnToolBar(QToolBar* tb) {
    if (!tb) return;

    tb->installEventFilter(this);

    // 既存のツールボタンにも（detach回避：const 反復）
    const auto buttons = tb->findChildren<QToolButton*>();
    for (QToolButton* btn : std::as_const(buttons))
        btn->installEventFilter(this);
}

// メニューにイベントフィルタを装着。
// 役割：QMenu の ToolTip イベントを捕捉してカスタム表示に差し替える。
void AppToolTipFilter::installOnMenu(QMenu* menu) {
    if (!menu) return;
    menu->installEventFilter(this);
}

// 指定ウィジェットのツールチップ文字列を選ぶヘルパ。
// 役割：メニューバーの場合は actionAt() の結果に応じ、アイコン無し項目は非表示（空文字）とする。
//       それ以外は QWidget::toolTip() を採用し、空ならトリムして空のまま返す。
// 注意：空文字を返すと表示しない扱い。
QString AppToolTipFilter::pickTextForWidget(QWidget* w, const QPoint& localPos) const {
    if (!w) return {};

    // QMenuBar 分岐：アクションごとに判定
    if (auto* mb = qobject_cast<QMenuBar*>(w)) {
        if (QAction* act = mb->actionAt(localPos)) {
            // アイコンが無い項目はツールチップを出さない
            if (act->icon().isNull())
                return QString();  // 空を返すと表示しない

            const QString t = act->toolTip().trimmed();
            return t.isEmpty() ? act->text().trimmed() : t;
        }
    }

    // それ以外：ウィジェット自身の toolTip を採用
    return w->toolTip().trimmed();
}

// イベントフィルタ本体。
// 役割：QEvent::ToolTip を横取りし、GlobalToolTip を使って表示/非表示を制御する。
// 動作：
//  - ToolTip 受信時：表示対象テキストを pickTextForWidget で選び、非空なら表示、空なら非表示。
//  - Leave/Hide/Close/FocusOut/WindowDeactivate/MouseButtonPress で非表示にする。
// 注意：表示時は true を返して既定の QToolTip を抑止する。
bool AppToolTipFilter::eventFilter(QObject* obj, QEvent* ev) {
    // ツールチップ表示
    if (ev->type() == QEvent::ToolTip) {
        auto* w  = qobject_cast<QWidget*>(obj);
        auto* he = static_cast<QHelpEvent*>(ev);
        if (!w || !he) return QObject::eventFilter(obj, ev);

        const QString text = pickTextForWidget(w, he->pos());
        if (!text.isEmpty()) {
            m_tip->showText(he->globalPos(), text);
            return true;                 // 既定の QToolTip を抑止
        } else {
            m_tip->hideTip();
        }
    }
    // 非表示トリガ
    else if (ev->type() == QEvent::Leave      ||
             ev->type() == QEvent::Hide       ||
             ev->type() == QEvent::Close      ||
             ev->type() == QEvent::FocusOut   ||
             ev->type() == QEvent::WindowDeactivate ||
             ev->type() == QEvent::MouseButtonPress)
    {
        if (m_tip) m_tip->hideTip();
    }

    return QObject::eventFilter(obj, ev);
}

// コンパクト表示の切り替え。
// 役割：内部の GlobalToolTip に on/off を委譲し、フォント/余白/角丸を一括調整。
void AppToolTipFilter::setCompact(bool on) {
    if (m_tip) m_tip->setCompact(on);
}
