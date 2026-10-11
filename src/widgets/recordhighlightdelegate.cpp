/// @file recordhighlightdelegate.cpp
/// @brief 棋譜欄・分岐候補欄の現在行の目印とセルの大きさのキャッシュの実装

#include "recordhighlightdelegate.h"
#include "kifubranchlistmodel.h"
#include "kifurecordlistmodel.h"
#include "tablestyles.h"

#include <QApplication>
#include <QPainter>

bool RecordHighlightDelegate::SizeKey::operator==(const SizeKey& other) const
{
    return text == other.text && font == other.font && style == other.style && features == other.features
        && state == other.state && decorationPosition == other.decorationPosition && wrapWidth == other.wrapWidth;
}

size_t qHash(const RecordHighlightDelegate::SizeKey& key, size_t seed)
{
    return qHashMulti(seed, key.text, key.font, quintptr(key.style), key.features, key.state,
                      key.decorationPosition, key.wrapWidth);
}

// 列幅の自動調整（ResizeToContents）は、行を足すたびや現在行の強調を変えるたびに
// 全行の大きさを測り直す。文字の配置の計算は重いので、同じ条件の結果を使い回す。
QSize RecordHighlightDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const
{
    const QVariant hint = index.data(Qt::SizeHintRole);
    if (hint.isValid()) return qvariant_cast<QSize>(hint);

    QStyleOptionViewItem opt = option;
    initStyleOption(&opt, index);
    const QWidget* widget = option.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    const bool wrap = opt.features.testFlag(QStyleOptionViewItem::WrapText);
    const SizeKey key{opt.text, opt.font, style, int(opt.features), int(opt.state),
                      int(opt.decorationPosition), wrap ? opt.rect.width() : 0};
    if (const auto it = m_sizeCache.constFind(key); it != m_sizeCache.cend()) return it.value();

    const QSize size = style->sizeFromContents(QStyle::CT_ItemViewItem, &opt, QSize(), widget);
    if (m_sizeCache.size() >= kMaxCachedSizes) m_sizeCache.clear();
    m_sizeCache.insert(key, size);
    return size;
}

void RecordHighlightDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    QStyledItemDelegate::paint(painter, option, index);
    if (index.column() != 0) return;

    const auto* recordModel = qobject_cast<const KifuRecordListModel*>(index.model());
    const auto* branchModel = qobject_cast<const KifuBranchListModel*>(index.model());
    const bool current = (recordModel && recordModel->currentHighlightRow() == index.row())
        || (branchModel && branchModel->currentHighlightRow() == index.row());
    if (current || (option.state & QStyle::State_Selected)) {
        painter->fillRect(QRect(option.rect.left(), option.rect.top(), 3, option.rect.height()),
                          TableStyles::selectionAccent());
    }
}
