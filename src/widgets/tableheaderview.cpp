/// @file tableheaderview.cpp
/// @brief 見出しの文字の測り直しと、見た目の変わらない描き直しを省く表の見出しの実装

#include "tableheaderview.h"

#include <QEvent>
#include <QIcon>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPixmap>
#include <QTableView>

bool TableHeaderView::SizeKey::operator==(const SizeKey& other) const
{
    return section == other.section && text == other.text && font == other.font && icon == other.icon
        && headerSize == other.headerSize && enabled == other.enabled && direction == other.direction
        && sortIndicator == other.sortIndicator && style == other.style && dpi == other.dpi;
}

size_t qHash(const TableHeaderView::SizeKey& key, size_t seed)
{
    return qHashMulti(seed, key.section, key.text, key.font, key.icon, key.headerSize.width(),
                      key.headerSize.height(), key.enabled, static_cast<int>(key.direction), key.sortIndicator,
                      reinterpret_cast<quintptr>(key.style), key.dpi);
}

TableHeaderView::TableHeaderView(Qt::Orientation orientation, QWidget* parent)
    : QHeaderView(orientation, parent)
{
    connect(this, &QHeaderView::sectionResized, this, &TableHeaderView::onSectionResized);
}

TableHeaderView* TableHeaderView::install(QTableView* view)
{
    auto* header = new TableHeaderView(Qt::Horizontal, view);
    header->setSectionsClickable(true);
    header->setHighlightSections(true);
    view->setHorizontalHeader(header);
    return header;
}

// 境目のドラッグは押してから離すまで、ダブルクリック（内容に合わせる）はその処理の間に列幅が変わる
void TableHeaderView::mousePressEvent(QMouseEvent* event)
{
    m_mouseOperating = true;
    QHeaderView::mousePressEvent(event);
}

void TableHeaderView::mouseReleaseEvent(QMouseEvent* event)
{
    QHeaderView::mouseReleaseEvent(event);
    m_mouseOperating = false;
}

void TableHeaderView::mouseDoubleClickEvent(QMouseEvent* event)
{
    m_mouseOperating = true;
    QHeaderView::mouseDoubleClickEvent(event);
    m_mouseOperating = false;
}

void TableHeaderView::onSectionResized(int logicalIndex, int oldSize, int newSize)
{
    if (m_mouseOperating) emit sectionResizedByUser(logicalIndex, oldSize, newSize);
}

// QHeaderView::sectionSizeFromContents が読む値（見出しのデータ・書体・見出しの大きさ・
// 有効かどうか・文字の向き・並べ替えの印・スタイル）を条件にして、結果を覚える
QSize TableHeaderView::sectionSizeFromContents(int logicalIndex) const
{
    const QAbstractItemModel* items = model();
    if (!items) return QHeaderView::sectionSizeFromContents(logicalIndex);
    const Qt::Orientation direction = orientation();
    const QVariant sizeHint = items->headerData(logicalIndex, direction, Qt::SizeHintRole);
    if (sizeHint.isValid()) return qvariant_cast<QSize>(sizeHint);

    ensurePolished();
    SizeKey key;
    key.section = logicalIndex;
    key.text = items->headerData(logicalIndex, direction, Qt::DisplayRole).toString();
    const QVariant font = items->headerData(logicalIndex, direction, Qt::FontRole);
    key.font = font.isValid() && font.canConvert<QFont>() ? qvariant_cast<QFont>(font) : this->font();
    const QVariant decoration = items->headerData(logicalIndex, direction, Qt::DecorationRole);
    QIcon icon = qvariant_cast<QIcon>(decoration);
    if (icon.isNull()) icon = qvariant_cast<QPixmap>(decoration);
    key.icon = icon.isNull() ? 0 : icon.cacheKey();
    key.headerSize = size();
    key.enabled = isEnabled();
    key.direction = layoutDirection();
    key.sortIndicator = isSortIndicatorShown();
    key.style = style();
    key.dpi = logicalDpiY();
    if (const auto it = m_sizes.constFind(key); it != m_sizes.cend()) return it.value();

    const QSize result = QHeaderView::sectionSizeFromContents(logicalIndex);
    if (m_sizes.size() >= kMaxCachedSizes) m_sizes.clear();
    m_sizes.insert(key, result);
    return result;
}

bool TableHeaderView::event(QEvent* event)
{
    switch (event->type()) {
    // スタイルシート（祖先やアプリ全体のものを含む）・書体・解像度が変わると、
    // 同じ条件でも大きさが変わりうる
    case QEvent::StyleChange:
    case QEvent::FontChange:
    case QEvent::LayoutDirectionChange:
    case QEvent::DevicePixelRatioChange:
    case QEvent::ScreenChangeInternal:
        m_sizes.clear();
        break;
    default:
        break;
    }
    return QHeaderView::event(event);
}

QRect TableHeaderView::sectionRect(int logicalIndex) const
{
    const int position = sectionViewportPosition(logicalIndex);
    const int size = sectionSize(logicalIndex);
    return orientation() == Qt::Horizontal ? QRect(position, 0, size, viewport()->height())
                                           : QRect(0, position, viewport()->width(), size);
}

// QHeaderView::initStyleOptionForIndex と同じ判定: 選択に掛かる（State_On）、全体が選ばれた（State_Sunken）
int TableHeaderView::selectionState(int logicalIndex) const
{
    const QItemSelectionModel* selection = selectionModel();
    if (!selection) return 0;
    const QModelIndex root = rootIndex();
    const bool horizontal = orientation() == Qt::Horizontal;
    int state = 0;
    if (horizontal ? selection->columnIntersectsSelection(logicalIndex, root)
                   : selection->rowIntersectsSelection(logicalIndex, root))
        state |= 1;
    if (horizontal ? selection->isColumnSelected(logicalIndex, root) : selection->isRowSelected(logicalIndex, root))
        state |= 2;
    return state;
}

void TableHeaderView::paintEvent(QPaintEvent* event)
{
    // 描く見出しの、選択による描き分けを覚える
    if (sectionsClickable() && highlightSections()) {
        const QRegion& region = event->region();
        for (int visual = 0; visual < count(); ++visual) {
            const int logical = logicalIndex(visual);
            if (isSectionHidden(logical)) continue;
            if (region.intersects(sectionRect(logical))) m_paintedSelectionStates.insert(logical, selectionState(logical));
        }
    }
    QHeaderView::paintEvent(event);
}

// QHeaderView は、選択が変わった範囲の見出しを描き直す（QAbstractItemView::selectionChanged）。
// 選択で見出しを描き分けないとき、または描き直す範囲の見出しの描き分けが描いたときと同じなら、
// 見た目は変わらないので描き直さない
void TableHeaderView::selectionChanged(const QItemSelection& selected, const QItemSelection& deselected)
{
    if (!sectionsClickable() || !highlightSections()) return;
    const QRegion changed = visualRegionForSelection(selected) | visualRegionForSelection(deselected);
    for (int visual = 0; visual < count(); ++visual) {
        const int logical = logicalIndex(visual);
        if (isSectionHidden(logical) || !changed.intersects(sectionRect(logical))) continue;
        const auto it = m_paintedSelectionStates.constFind(logical);
        if (it == m_paintedSelectionStates.cend() || it.value() != selectionState(logical)) {
            QHeaderView::selectionChanged(selected, deselected);
            return;
        }
    }
}
