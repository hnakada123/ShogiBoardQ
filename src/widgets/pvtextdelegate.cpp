/// @file pvtextdelegate.cpp
/// @brief 読み筋の列を描くデリゲートの実装

#include "pvtextdelegate.h"

#include <QApplication>
#include <QFontMetrics>
#include <QStyle>
#include <QTextLayout>

namespace {
// これより短い文字列は切らない（切る位置を覚える手間の方が大きい）
constexpr int kMinimumLength = 32;
// 覚えておく文字列の数の上限（思考表の行数より十分多い）
constexpr int kMaxCachedTexts = 1024;

// 改行や書字方向の制御文字があると、Qt は行の分け方や省略の仕方を変えるので切らない
bool hasLayoutControl(const QString& text)
{
    for (const QChar c : text) {
        const char16_t u = c.unicode();
        if (u == u'\n' || u == u'\r' || u == QChar::LineSeparator || u == QChar::ParagraphSeparator
            || u == 0x200e || u == 0x200f || (u >= 0x202a && u <= 0x202e) || (u >= 0x2066 && u <= 0x2069))
            return true;
    }
    return false;
}

// Qt の項目の描画と同じく、折り返さずに1行に並べる
QTextLine layoutSingleLine(QTextLayout& layout, int width)
{
    QTextOption option;
    option.setWrapMode(QTextOption::ManualWrap);
    layout.setTextOption(option);
    layout.beginLayout();
    QTextLine line = layout.createLine();
    line.setLineWidth(width);
    layout.endLayout();
    return line;
}

// 表示幅 width のセルで、省略の結果を変えずに切れる位置（切れなければ -1）
int cutPosition(const QString& text, const QFont& font, int width)
{
    // 省略の位置は表示幅の内側で決まる。切った所の字形の幅の違い（隣の文字との詰め）が
    // 届かないよう、表示幅より最大の文字幅の数個分先で切る。
    const int limit = width + 4 * QFontMetrics(font).maxWidth();
    QTextLayout full(text, font);
    const QTextLine fullLine = layoutSingleLine(full, width);
    if (fullLine.naturalTextWidth() <= limit) return -1;
    const int length = fullLine.xToCursor(limit);
    if (length <= 0 || length >= text.size()) return -1;

    // 切った後ろにだけ別のフォントの文字があると行の高さが変わり、縦の位置がずれる。
    // 切った文字列が表示幅に収まると省略されなくなる。どちらの場合も切らない。
    QTextLayout head(text.left(length), font);
    const QTextLine headLine = layoutSingleLine(head, width);
    if (headLine.height() != fullLine.height() || headLine.naturalTextWidth() <= width) return -1;
    return length;
}
} // namespace

void PvTextDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    // QStyledItemDelegate::paint と同じ手順で、渡す文字列だけを切る
    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);
    if ((opt.features & QStyleOptionViewItem::HasDisplay)
        && !(opt.features & QStyleOptionViewItem::WrapText)
        && opt.textElideMode == Qt::ElideRight && opt.direction == Qt::LeftToRight) {
        opt.text = textForWidth(opt.text, opt.font, opt.rect.width());
    }
    const QWidget* widget = opt.widget;
    const QStyle* style = widget ? widget->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &opt, painter, widget);
}

QString PvTextDelegate::textForWidth(const QString& text, const QFont& font, int width) const
{
    if (text.size() < kMinimumLength || width <= 0 || hasLayoutControl(text)) return text;

    const QString fontKey = font.key();
    auto it = m_cuts.constFind(text);
    if (it == m_cuts.cend() || it->fontKey != fontKey || it->width != width) {
        if (m_cuts.size() >= kMaxCachedTexts) m_cuts.clear();
        it = m_cuts.insert(text, Cut{fontKey, width, cutPosition(text, font, width)});
    }
    return it->length < 0 ? text : text.left(it->length);
}
