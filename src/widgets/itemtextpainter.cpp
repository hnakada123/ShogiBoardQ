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
    const bool wrap = option.features.testFlag(QStyleOptionViewItem::WrapText);
    textOption.setWrapMode(wrap ? QTextOption::WordWrap : QTextOption::ManualWrap);
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
    // 折り返す設定（WrapText）は、1行に収まる文字列だけを自分で描く（prepare で確かめる）
    const QStyleOptionViewItem::ViewItemFeatures unsupported =
        QStyleOptionViewItem::HasDecoration | QStyleOptionViewItem::HasCheckIndicator;
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
    const auto drawByQt = [&] {
        QStyleOptionViewItem opt(option);
        m_cutter.apply(opt);
        style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
    };
    if (!canDrawText(option)) {
        drawByQt();
        return;
    }

    // 文字を並べる範囲は Qt と同じく、項目の文字の範囲から左右の余白を除いたもの。
    // アイコン・チェックが無ければ、文字の範囲は文字があるかどうかだけで決まるので、短い文字で求める
    QStyleOptionViewItem frame(option);
    frame.text = QStringLiteral("M");
    const QRect textRect = style->subElementRect(QStyle::SE_ItemViewItemText, &frame, widget);
    const int margin = style->pixelMetric(QStyle::PM_FocusFrameHMargin, &option, widget) + 1;
    const QRect lineRect = textRect.adjusted(margin, 0, -margin, 0);
    const Prepared& prepared = prepare(option, lineRect.width());
    if (!prepared.layout) {
        drawByQt();
        return;
    }

    // 枠・背景・選択の色は Qt が描く
    frame.text.clear();
    style->drawControl(QStyle::CE_ItemViewItem, &frame, painter, widget);

    // 文字は Qt と同じ位置・色・切り抜きで描く。縦の位置は、Qt と同じく省略前の（切った）文字列の
    // 配置の大きさで決める
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
    const bool wrap = option.features.testFlag(QStyleOptionViewItem::WrapText);
    auto it = m_prepared.find(option.text);
    if (it != m_prepared.end()) {
        const Prepared& found = it->second;
        if (found.lineWidth == lineWidth && found.cellWidth == cellWidth && found.alignment == alignment
            && found.wrap == wrap && found.font == option.font)
            return found;
    } else if (m_prepared.size() >= kMaxPreparedTexts) {
        m_prepared.clear();
    }

    Prepared prepared;
    prepared.font = option.font;
    prepared.cellWidth = cellWidth;
    prepared.lineWidth = lineWidth;
    prepared.alignment = alignment;
    prepared.wrap = wrap;

    // Qt に渡していたのと同じく、表示幅より十分先で切った文字列を使う（省略の結果は全文と同じ）。
    // 折り返す設定では切らない（切ると折り返しが変わる）
    const QString cut = wrap ? option.text : m_cutter.textForWidth(option.text, option.font, cellWidth);
    const QTextOption textOption = itemTextOption(option);
    const auto layoutText = [&](const QString& text) {
        auto layout = std::make_unique<QTextLayout>(text, option.font);
        layout->setTextOption(textOption);
        layout->setCacheEnabled(true);  // 描くたびに字形を求め直さない
        layoutLines(*layout, lineWidth);
        return layout;
    };
    prepared.layout = layoutText(cut);
    prepared.textSize = prepared.layout->boundingRect().size().toSize();

    // 折り返す設定で1行に収まらなければ（2行以上になる・切れ目が無くはみ出す）、Qt に描かせる。
    // 1行に収まれば、Qt も折り返さない場合と同じく、その1行をそのまま描く
    if (wrap) {
        if (prepared.layout->lineCount() != 1 || prepared.layout->lineAt(0).naturalTextWidth() > lineWidth)
            prepared.layout.reset();
        return m_prepared.insert_or_assign(option.text, std::move(prepared)).first->second;
    }

    // 表示幅に収まらなければ、Qt と同じ方法（QCommonStylePrivate::calculateElidedText）で右端を省略する。
    // 収まれば、切った文字列の配置をそのまま描く（数値などの短い文字列は、配置の計算が1回で済む）
    const QTextLayout& cutLayout = *prepared.layout;
    const bool elide = cutLayout.lineCount() > 0 && cutLayout.lineAt(0).naturalTextWidth() > lineWidth;
    if (elide)
        prepared.layout = layoutText(QFontMetrics(option.font).elidedText(cut, option.textElideMode, lineWidth));
    return m_prepared.insert_or_assign(option.text, std::move(prepared)).first->second;
}
