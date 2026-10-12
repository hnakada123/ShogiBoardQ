/// @file layoutboundarywidget.cpp
/// @brief 中身の大きさの条件が変わったときだけ、親に配置のやり直しを伝える入れ物の実装

#include "layoutboundarywidget.h"

#include <QLayout>
#include <QSizePolicy>
#include <memory>

namespace {
// 中身1つを入れ物いっぱいに置くレイアウト。中身の updateGeometry は、表示中かどうかに
// かかわらず、その場で親（入れ物）のレイアウトの invalidate を呼ぶ（QWidgetPrivate::
// updateGeometry_helper）。そこで中身の大きさの条件を前回と比べ、変わったときだけ入れ物の
// updateGeometry を呼ぶ。中身を親に直接置いた場合に、中身の updateGeometry が親に伝わるのと
// 同じ時点になる。
class BoundaryLayout final : public QLayout
{
public:
    explicit BoundaryLayout(QWidget* parent)
        : QLayout(parent)
    {
        setContentsMargins(0, 0, 0, 0);
    }

    void addItem(QLayoutItem* item) override { m_item.reset(item); }
    int count() const override { return m_item ? 1 : 0; }
    QLayoutItem* itemAt(int index) const override { return index == 0 ? m_item.get() : nullptr; }
    QLayoutItem* takeAt(int index) override { return index == 0 ? m_item.release() : nullptr; }

    // 親のレイアウトには、前回伝えた中身の大きさの条件を見せる（中身を直接置いた場合に、親の
    // レイアウトが中身の updateGeometry まで前の値を使うのと同じ）
    QSize sizeHint() const override { return m_constraints.sizeHint; }
    QSize minimumSize() const override { return m_constraints.minimumSizeHint; }
    Qt::Orientations expandingDirections() const override { return m_constraints.expandingDirections; }
    bool hasHeightForWidth() const override { return m_constraints.heightForWidth; }
    int heightForWidth(int width) const override { return m_item ? m_item->heightForWidth(width) : -1; }

    void setGeometry(const QRect& rect) override
    {
        QLayout::setGeometry(rect);
        if (m_item) m_item->setGeometry(rect);
    }

    void invalidate() override
    {
        const Constraints constraints = contentConstraints();
        if (constraints == m_constraints) return;
        m_constraints = constraints;
        // QLayout::invalidate は呼ばない（入れ物のレイアウトを後でやり直すと、親にもう一度伝わる）。
        // 中身の位置と大きさは、入れ物の大きさが変わったときに setGeometry で合わせる。
        // 中身に指定した最小・最大の大きさと伸び縮みの方針は、入れ物にも写して親から同じに見せる
        QWidget* container = parentWidget();
        container->setSizePolicy(constraints.sizePolicy);
        container->setMinimumSize(constraints.minimumSize);
        container->setMaximumSize(constraints.maximumSize);
        container->updateGeometry();
    }

    /// addItem の後、親のレイアウトに入る前に、中身の大きさの条件を覚える
    void record() { invalidate(); }

private:
    /// 親のレイアウトが読む、中身の大きさの条件
    struct Constraints {
        QSize sizeHint;
        QSize minimumSizeHint;
        QSize minimumSize;
        QSize maximumSize;
        QSizePolicy sizePolicy;
        Qt::Orientations expandingDirections;
        bool heightForWidth = false;
        bool operator==(const Constraints& other) const
        {
            return sizeHint == other.sizeHint && minimumSizeHint == other.minimumSizeHint
                && minimumSize == other.minimumSize && maximumSize == other.maximumSize
                && sizePolicy == other.sizePolicy && expandingDirections == other.expandingDirections
                && heightForWidth == other.heightForWidth;
        }
    };

    Constraints contentConstraints() const
    {
        Constraints constraints;
        QWidget* content = m_item ? m_item->widget() : nullptr;
        if (!content) return constraints;
        constraints.sizeHint = content->sizeHint();
        constraints.minimumSizeHint = content->minimumSizeHint();
        constraints.minimumSize = content->minimumSize();
        constraints.maximumSize = content->maximumSize();
        constraints.sizePolicy = content->sizePolicy();
        constraints.expandingDirections = m_item->expandingDirections();
        constraints.heightForWidth = content->hasHeightForWidth();
        return constraints;
    }

    std::unique_ptr<QLayoutItem> m_item;
    Constraints m_constraints;
};
} // namespace

LayoutBoundaryWidget::LayoutBoundaryWidget(QWidget* content, QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new BoundaryLayout(this);
    content->setParent(this);
    layout->addWidget(content);
    layout->record();
}

QWidget* LayoutBoundaryWidget::content() const
{
    const QLayoutItem* item = layout()->itemAt(0);
    return item ? item->widget() : nullptr;
}
