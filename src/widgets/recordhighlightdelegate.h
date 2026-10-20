#ifndef RECORDHIGHLIGHTDELEGATE_H
#define RECORDHIGHLIGHTDELEGATE_H

/// @file recordhighlightdelegate.h
/// @brief 棋譜欄・分岐候補欄の現在行の目印とセルの大きさのキャッシュ

#include "itemtextdelegate.h"

#include <QFont>
#include <QHash>

/// Qt の選択行に加え、操作が無効な対局中もモデルの現在行に目印を描く。
/// 列幅の自動調整で何度も測るセルの大きさは、同じ条件の結果を使い回す。
/// 文字は配置を覚えて描き、長いコメントは表示幅より十分先で切る（ItemTextDelegate。見た目は変わらない）。
class RecordHighlightDelegate : public ItemTextDelegate
{
public:
    using ItemTextDelegate::ItemTextDelegate;

    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    void paint(QPainter* painter, const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;

    /// セルの大きさを決める条件（文字・書体・スタイル・状態・配置）
    struct SizeKey {
        QString text;
        QFont font;
        const QStyle* style = nullptr;
        int features = 0;
        int state = 0;
        int decorationPosition = 0;
        int wrapWidth = 0;

        bool operator==(const SizeKey& other) const;
    };

private:
    QSize textSize(const QStyleOptionViewItem& option) const;
    QChar widestDigit(const QFont& font) const;

    static constexpr qsizetype kMaxCachedSizes = 4096;
    mutable QHash<SizeKey, QSize> m_sizeCache;
    mutable QHash<QFont, QChar> m_widestDigits;
};

size_t qHash(const RecordHighlightDelegate::SizeKey& key, size_t seed = 0);

#endif // RECORDHIGHLIGHTDELEGATE_H
