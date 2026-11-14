/// @file opaquebackground.cpp
/// @brief ドックの中身などを不透明なウィジェットにする処理の実装

#include "opaquebackground.h"

#include <QAbstractScrollArea>
#include <QBrush>
#include <QEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QWidget>
#include <cmath>
#include <optional>

namespace {
/// 背景の役割の色をそのまま全体に塗るウィンドウか（メインウィンドウとダイアログ。切り離したドックは枠が別の色を塗る）
bool fillsPlainBackground(const QWidget* window)
{
    return window->inherits("QMainWindow") || window->inherits("QDialog");
}

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
        // 下に見えるのはメインウィンドウ（またはダイアログ）の背景。ドックを切り離している間は、
        // ドックの枠が別の色で背景を描くので塗らない
        const QWidget* window = m_target->window();
        if (window == m_target || !fillsPlainBackground(window)
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

// スクロールバーの入れ物を、決めた色で塗る（入れ物が描かれる前に塗り、入れ物を不透明として扱わせる）
class ContainerFill final : public QObject
{
public:
    explicit ContainerFill(QWidget* container)
        : QObject(container)
    {
        container->installEventFilter(this);
    }

    void setBrush(const std::optional<QBrush>& brush)
    {
        if (brush == m_brush) return;
        m_brush = brush;
        auto* container = static_cast<QWidget*>(parent());
        container->setAttribute(Qt::WA_OpaquePaintEvent, brush.has_value());
        container->update();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (m_brush && event->type() == QEvent::Paint) {
            auto* container = static_cast<QWidget*>(watched);
            QPainter(container).fillRect(static_cast<QPaintEvent*>(event)->rect(), *m_brush);
        }
        return QObject::eventFilter(watched, event);
    }

private:
    std::optional<QBrush> m_brush;
};

constexpr char kContainerFillName[] = "sbqScrollBarContainerFill";

// スクロールバーの入れ物を、表示領域の背景色で塗らせ続ける
class ScrollBarFillKeeper final : public QObject
{
public:
    explicit ScrollBarFillKeeper(QAbstractScrollArea* area)
        : QObject(area), m_area(area)
    {
        area->installEventFilter(this);
        restoreFill();
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        const QEvent::Type type = event->type();
        if (type == QEvent::Polish || type == QEvent::StyleChange || type == QEvent::PaletteChange
            || type == QEvent::ParentChange || type == QEvent::Show || type == QEvent::DevicePixelRatioChange)
            restoreFill();
        return QObject::eventFilter(watched, event);
    }

private:
    void restoreFill()
    {
        const QWidget* viewport = m_area->viewport();
        const QBrush brush = viewport->palette().brush(viewport->backgroundRole());
        const qreal ratio = m_area->devicePixelRatioF();
        const bool integral = std::abs(ratio - std::round(ratio)) < 1e-9;
        // 表示領域が自分の背景色を塗っているとき（スタイルシートに止められていないとき）だけ
        const bool fill = integral && brush.isOpaque() && viewport->autoFillBackground();
        OpaqueBackground::fillScrollBarContainers(m_area, fill ? std::optional<QBrush>(brush) : std::nullopt);
    }

    QAbstractScrollArea* m_area;
};

constexpr char kKeptProperty[] = "_sbq_opaqueBackground";
constexpr char kScrollBarsKeptProperty[] = "_sbq_opaqueScrollBars";
} // namespace

void OpaqueBackground::keep(QWidget* widget)
{
    if (!widget || widget->property(kKeptProperty).toBool()) return;
    widget->setProperty(kKeptProperty, true);
    new FillKeeper(widget);
}

void OpaqueBackground::keepScrollBars(QAbstractScrollArea* area)
{
    if (!area || area->property(kScrollBarsKeptProperty).toBool()) return;
    area->setProperty(kScrollBarsKeptProperty, true);
    new ScrollBarFillKeeper(area);
}

void OpaqueBackground::fillScrollBarContainers(QAbstractScrollArea* area, const std::optional<QBrush>& brush)
{
    for (const QScrollBar* bar : {area->horizontalScrollBar(), area->verticalScrollBar()}) {
        QWidget* container = bar ? bar->parentWidget() : nullptr;
        if (!container || container == area) continue;
        auto* fill = static_cast<ContainerFill*>(
            container->findChild<QObject*>(QLatin1String(kContainerFillName), Qt::FindDirectChildrenOnly));
        if (!fill) {
            if (!brush) continue;
            fill = new ContainerFill(container);
            fill->setObjectName(QLatin1String(kContainerFillName));
        }
        fill->setBrush(brush);
    }
}

