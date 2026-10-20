/// @file recordhighlightdelegate.cpp
/// @brief 棋譜欄・分岐候補欄の現在行の目印とセルの大きさのキャッシュの実装

#include "recordhighlightdelegate.h"
#include "kifubranchlistmodel.h"
#include "kifurecordlistmodel.h"
#include "movefont.h"
#include "tablestyles.h"

#include <QApplication>
#include <QFontMetricsF>
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
    QSize size = textSize(opt);
    if (qobject_cast<const KifuRecordListModel*>(index.model())) {
        // 内容だけで幅を決めると、初めての「成」「不成」や手数の桁増加で棋譜欄と
        // ウィンドウが伸びる。通常の指し手・時間の幅は開始局面から確保する。
        if (index.column() == KifuRecordListModel::MoveColumn) {
            opt.font = MoveFont::font(opt.font);
            opt.fontMetrics = QFontMetrics(opt.font);
            opt.text = MoveFont::usesJapaneseFont()
                ? QStringLiteral("9999 △９九銀不成(99)+")
                : QStringLiteral("9999 △+B9ix9h= [+]");
        } else if (index.column() == KifuRecordListModel::TimeColumn) {
            opt.text = QStringLiteral("99:99/99:99:99");
        } else {
            return size;
        }
        // プロポーショナル数字を持つ書体でも、数字の入れ替わりで幅を変えない。
        opt.text.replace(QLatin1Char('9'), widestDigit(opt.font));
        size.setWidth(qMax(size.width(), textSize(opt).width()));
    }
    return size;
}

QChar RecordHighlightDelegate::widestDigit(const QFont& font) const
{
    if (const auto it = m_widestDigits.constFind(font); it != m_widestDigits.cend()) return it.value();
    // 整数へ丸めて比べると同じ幅に見える数字も、文字列全体では数ピクセル差が出る。
    const QFontMetricsF metrics(font);
    QChar widest = QLatin1Char('0');
    for (const QChar digit : QStringLiteral("123456789")) {
        if (metrics.horizontalAdvance(digit) > metrics.horizontalAdvance(widest)) widest = digit;
    }
    if (m_widestDigits.size() >= kMaxCachedSizes) m_widestDigits.clear();
    m_widestDigits.insert(font, widest);
    return widest;
}

QSize RecordHighlightDelegate::textSize(const QStyleOptionViewItem& option) const
{
    const QWidget* widget = option.widget;
    QStyle* style = widget ? widget->style() : QApplication::style();
    const bool wrap = option.features.testFlag(QStyleOptionViewItem::WrapText);
    const SizeKey key{option.text, option.font, style, int(option.features), int(option.state),
                      int(option.decorationPosition), wrap ? option.rect.width() : 0};
    if (const auto it = m_sizeCache.constFind(key); it != m_sizeCache.cend()) return it.value();

    const QSize size = style->sizeFromContents(QStyle::CT_ItemViewItem, &option, QSize(), widget);
    if (m_sizeCache.size() >= kMaxCachedSizes) m_sizeCache.clear();
    m_sizeCache.insert(key, size);
    return size;
}

void RecordHighlightDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    // 項目は ItemTextDelegate と同じく描く（文字は配置を覚えて描く。棋譜のコメントは列幅の
    // 数倍の長さになることがあり、表示幅より十分先で切る）
    ItemTextDelegate::paint(painter, option, index);
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
