/// @file opaquebackground.cpp
/// @brief ドックの中身などを不透明なウィジェットにする処理の実装

#include "opaquebackground.h"

#include <QBrush>
#include <QEvent>
#include <QPalette>
#include <QWidget>

namespace {
// ウィジェットへのスタイルの適用・パレットの変更・親の変更・表示を見張り、塗るかどうかを
// 判断し直す。QStyleSheetStyle は背景の指定があるウィジェットの自動の塗りつぶしを止めるので、
// そのあとで戻す。ドックを切り離す・戻すときは、ドックが表示し直されて中身にも表示の
// 通知が届く。
class FillKeeper final : public QObject
{
public:
    explicit FillKeeper(QWidget* target)
        : QObject(target), m_target(target)
    {
        target->installEventFilter(this);
        restoreFill();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        const QEvent::Type type = event->type();
        if (type == QEvent::Polish || type == QEvent::StyleChange || type == QEvent::PaletteChange
            || type == QEvent::ParentChange || type == QEvent::Show)
            restoreFill();
        return QObject::eventFilter(watched, event);
    }

private:
    bool canFill() const
    {
        // 背景が不透明な色のときだけ塗る（透明な背景の指定はそのまま）
        const QBrush brush = m_target->palette().brush(m_target->backgroundRole());
        if (!brush.isOpaque()) return false;
        // 下に見えるのはメインウィンドウの背景。ドックを切り離している間は、ドックの枠が
        // 別の色で背景を描くので塗らない
        const QWidget* window = m_target->window();
        if (window == m_target || !window->inherits("QMainWindow")
            || window->palette().brush(window->backgroundRole()) != brush)
            return false;
        // 親と違う色を塗ると、枠の角など下が透けて見える所の色が変わる
        const QWidget* parent = m_target->parentWidget();
        return parent && parent->palette().brush(parent->backgroundRole()) == brush;
    }

    void restoreFill()
    {
        if (canFill()) {
            m_target->setAutoFillBackground(true);
            m_filled = true;
        } else if (m_filled) {
            m_target->setAutoFillBackground(false);
            m_filled = false;
        }
    }

    QWidget* m_target;
    bool m_filled = false;
};

constexpr char kKeptProperty[] = "_sbq_opaqueBackground";
} // namespace

void OpaqueBackground::keep(QWidget* widget)
{
    if (!widget || widget->property(kKeptProperty).toBool()) return;
    widget->setProperty(kKeptProperty, true);
    new FillKeeper(widget);
}
