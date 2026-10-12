/// @file itemtextpainter.cpp
/// @brief 表の項目の長い文字列を描く処理の実装

#include "itemtextpainter.h"

#include <QApplication>
#include <QFontMetrics>
#include <QPainter>
#include <QStyle>
#include <QStyleOptionViewItem>
#include <QTextLayout>
#include <QTextOption>

namespace {
// 覚えておく文字列の数の上限（表に見えている行より十分多い）
constexpr std::size_t kMaxPreparedTexts = 256;

// Qt が項目の文字を並べるときの設定（QCommonStylePrivate::viewItemDrawText と同じ）
QTextOption itemTextOption(const QStyleOptionViewItem& option)
{
    QTextOption textOption;
    textOption.setWrapMode(QTextOption::ManualWrap);
    textOption.setTextDirection(option.direction);
    textOption.setAlignment(QStyle::visualAlignment(option.direction, option.displayAlignment));
    return textOption;
}

// 行の幅を決めて並べる（QCommonStyle の viewItemTextLayout と同じ）
void layoutLines(QTextLayout& layout, int lineWidth)
{
    qreal height = 0;
    layout.beginLayout();
    while (true) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) break;
        line.setLineWidth(lineWidth);
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    layout.endLayout();
}
} // namespace

ItemTextPainter::ItemTextPainter() = default;
ItemTextPainter::~ItemTextPainter() = default;

bool ItemTextPainter::canDrawText(const QStyleOptionViewItem& option)
{
    const QStyleOptionViewItem::ViewItemFeatures unsupported =
        QStyleOptionViewItem::WrapText | QStyleOptionViewItem::HasDecoration | QStyleOptionViewItem::HasCheckIndicator;
    // 文字の範囲がアイコンの位置に左右されない並べ方だけを扱う（上下に置くと文字の高さで範囲が変わる）
    const bool sideDecoration = option.decorationPosition == QStyleOptionViewItem::Left
        || option.decorationPosition == QStyleOptionViewItem::Right;
    // Qt はフォーカスの枠を文字の範囲に合わせて描くので、フォーカスのある項目は Qt に任せる。
    // U+009C は省略で長さの違う候補を区切る文字（QFontMetrics::elidedText が特別に扱う）
    return (option.features & QStyleOptionViewItem::HasDisplay) && !(option.features & unsupported)
        && !option.text.isEmpty() && option.textElideMode == Qt::ElideRight
        && option.direction == Qt::LeftToRight && sideDecoration
        && !(option.state & QStyle::State_HasFocus)
        && !ItemTextCutter::hasLayoutControl(option.text) && !option.text.contains(QChar(0x9c));
}

void ItemTextPainter::paint(QPainter* painter, const QStyleOptionViewItem& option) const
{
    const QWidget* widget = option.widget;
    const QStyle* style = widget ? widget->style() : QApplication::style();
    if (!canDrawText(option)) {
        QStyleOptionViewItem opt(option);
        m_cutter.apply(opt);
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
        return;
    }

    // 文字を並べる範囲は Qt と同じく、項目の文字の範囲から左右の余白を除いたもの。
    // アイコン・チェックが無ければ、文字の範囲は文字があるかどうかだけで決まるので、短い文字で求める
    QStyleOptionViewItem frame(option);
    frame.text = QStringLiteral("M");
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &frame, widget);
    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, widget) + 1;
    const QRect lineRect = textRect.adjusted(margin, 0, -margin, 0);

    // 枠・背景・選択の色は Qt が描く
    frame.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &frame, painter, widget);

    // 文字は Qt と同じ位置・色・切り抜きで描く。縦の位置は、Qt と同じく省略前の（切った）文字列の
    // 配置の大きさで決める
    const Prepared& prepared = prepare(option, lineRect.width());
    const QRect layoutRect = QStyle::alignedRect(Qt::LayoutDirectionAuto, option.displayAlignment,
                                                 prepared.textSize, lineRect);
    QPalette::ColorGroup group = (option.state & QStyle::State_Enabled) ? QPalette::Normal : QPalette::Disabled;
    if (group == QPalette::Normal && !(option.state & QStyle::State_Active)) group = QPalette::Inactive;
    const QPalette::ColorRole role =
        (option.state & QStyle::State_Selected) ? QPalette::HighlightedText : QPalette::Text;

    painter->save();
    painter->setClipRect(option.rect, Qt::IntersectClip);
    painter->setPen(option.palette.color(group, role));
    prepared.layout->draw(painter, QPointF(lineRect.x(), layoutRect.top()));
    painter->restore();
}

const ItemTextPainter::Prepared& ItemTextPainter::prepare(const QStyleOptionViewItem& option, int lineWidth) const
{
    const int cellWidth = option.rect.width();
    const int alignment = static_cast<int>(option.displayAlignment);
    auto it = m_prepared.find(option.text);
    if (it != m_prepared.end()) {
        const Prepared& found = it->second;
        if (found.lineWidth == lineWidth && found.cellWidth == cellWidth && found.alignment == alignment
            && found.font == option.font)
            return found;
    } else if (m_prepared.size() >= kMaxPreparedTexts) {
        m_prepared.clear();
    }

    Prepared prepared;
    prepared.font = option.font;
    prepared.cellWidth = cellWidth;
    prepared.lineWidth = lineWidth;
    prepared.alignment = alignment;

    // Qt に渡していたのと同じく、表示幅より十分先で切った文字列を使う（省略の結果は全文と同じ）
    const QString cut = m_cutter.textForWidth(option.text, option.font, cellWidth);
    const QTextOption textOption = itemTextOption(option);
    QTextLayout cutLayout(cut, option.font);
    cutLayout.setTextOption(textOption);
    layoutLines(cutLayout, lineWidth);
    prepared.textSize = cutLayout.boundingRect().size().toSize();

    // 表示幅に収まらなければ、Qt と同じ方法（QCommonStylePrivate::calculateElidedText）で右端を省略する
    const bool elide = cutLayout.lineCount() > 0 && cutLayout.lineAt(0).naturalTextWidth() > lineWidth;
    const QString text = elide ? QFontMetrics(option.font).elidedText(cut, option.textElideMode, lineWidth) : cut;
    prepared.layout = std::make_unique<QTextLayout>(text, option.font);
    prepared.layout->setTextOption(textOption);
    prepared.layout->setCacheEnabled(true);  // 描くたびに字形を求め直さない
    layoutLines(*prepared.layout, lineWidth);
    return m_prepared.insert_or_assign(option.text, std::move(prepared)).first->second;
}
