/// @file applicationstyle.cpp
/// @brief アプリ全体で使うスタイル（Fusion）の実装

#include "applicationstyle.h"

#include <QCoreApplication>
#include <QHash>
#include <QIcon>
#include <QProxyStyle>
#include <QStyleFactory>
#include <QStyleOptionViewItem>

namespace {
// Fusion を包み、内蔵の画像から作るアイコンと、表や一覧の項目の各部の位置を使い回すスタイル。
// ほかはすべて Fusion に任せる。
class ApplicationFusionStyle final : public QProxyStyle
{
public:
    ApplicationFusionStyle()
        : QProxyStyle(QStyleFactory::create(QStringLiteral("Fusion")))
    {
    }

    QIcon standardIcon(StandardPixmap standardIcon, const QStyleOption* option,
                       const QWidget* widget) const override
    {
        switch (standardIcon) {
        // QFusionStyle::iconFromTheme が内蔵の画像から作るもの（部品や設定によらない）
        case SP_TitleBarNormalButton:
        case SP_TitleBarMinButton:
        case SP_TitleBarCloseButton:
        case SP_DockWidgetCloseButton: {
            const auto it = m_icons.constFind(standardIcon);
            if (it != m_icons.cend()) return it.value();
            const QIcon icon = QProxyStyle::standardIcon(standardIcon, option, widget);
            m_icons.insert(standardIcon, icon);
            return icon;
        }
        default:
            return QProxyStyle::standardIcon(standardIcon, option, widget);
        }
    }

    QRect subElementRect(SubElement element, const QStyleOption* option, const QWidget* widget) const override
    {
        switch (element) {
        case SE_ItemViewItemCheckIndicator:
        case SE_ItemViewItemDecoration:
        case SE_ItemViewItemText:
        case SE_ItemViewItemFocusRect:
            if (const auto* item = qstyleoption_cast<const QStyleOptionViewItem*>(option);
                item && !item->text.isEmpty()
                && (item->decorationPosition == QStyleOptionViewItem::Left
                    || item->decorationPosition == QStyleOptionViewItem::Right)) {
                return itemViewRect(element, *item, widget);
            }
            break;
        default:
            break;
        }
        return QProxyStyle::subElementRect(element, option, widget);
    }

private:
    // 表や一覧の項目を描くとき、Qt は各部（チェック・絵・文字・焦点枠）の位置を求めるたびに文字全体の
    // 配置を計算する。絵を文字の左右に置く配置（既定）では、位置は文字の大きさによらず「文字があるか」
    // だけで決まる（QCommonStylePrivate::viewItemLayout）。位置を決める値（大きさ・チェックと絵の有無と
    // 大きさ・配置・文字の向き・解像度）ごとに、項目の左上からの位置を覚えて使い回す。
    struct ItemRectKey {
        int element = 0;
        QSize size;
        uint features = 0;
        QSize decorationSize;
        int decorationPosition = 0;
        uint decorationAlignment = 0;
        int direction = 0;
        qreal dpi = 0;      // Fusion はチェックの大きさをこの解像度で拡大する（QStyleHelper::dpi）
        bool use96Dpi = false;
        bool operator==(const ItemRectKey& other) const
        {
            return element == other.element && size == other.size && features == other.features
                && decorationSize == other.decorationSize && decorationPosition == other.decorationPosition
                && decorationAlignment == other.decorationAlignment && direction == other.direction
                && dpi == other.dpi && use96Dpi == other.use96Dpi;
        }
        friend size_t qHash(const ItemRectKey& key, size_t seed = 0)
        {
            return qHashMulti(seed, key.element, key.size.width(), key.size.height(), key.features,
                              key.decorationSize.width(), key.decorationSize.height(), key.decorationPosition,
                              key.decorationAlignment, key.direction, key.dpi, key.use96Dpi);
        }
    };

    QRect itemViewRect(SubElement element, const QStyleOptionViewItem& item, const QWidget* widget) const
    {
        const ItemRectKey key{static_cast<int>(element), item.rect.size(), item.features.toInt(),
                              item.features.testFlag(QStyleOptionViewItem::HasDecoration) ? item.decorationSize : QSize(),
                              static_cast<int>(item.decorationPosition), item.decorationAlignment.toInt(),
                              static_cast<int>(item.direction), item.fontMetrics.fontDpi(),
                              QCoreApplication::testAttribute(Qt::AA_Use96Dpi)};
        if (const auto it = m_itemRects.constFind(key); it != m_itemRects.cend())
            return it->relative ? it->rect.translated(item.rect.topLeft()) : it->rect;
        // 初めての条件は Fusion に求めさせる（文字は英字1文字にして、配置の計算を軽くする）。
        // 項目をずらしてもう一度求め、位置が一緒に動くか（多くの部分）、動かないか（チェックの
        // 印の無い項目のチェックの位置）を確かめてから覚える
        QStyleOptionViewItem shortened(item);
        shortened.text = QStringLiteral("x");
        const QRect rect = QProxyStyle::subElementRect(element, &shortened, widget);
        const QPoint probe(1000, 1000);
        shortened.rect.translate(probe);
        const QRect moved = QProxyStyle::subElementRect(element, &shortened, widget);
        const bool relative = moved == rect.translated(probe);
        if (!relative && moved != rect) return rect;
        constexpr qsizetype kMaxItemRects = 1024;
        if (m_itemRects.size() >= kMaxItemRects) m_itemRects.clear();
        m_itemRects.insert(key, {relative ? rect.translated(-item.rect.topLeft()) : rect, relative});
        return rect;
    }

    struct CachedItemRect {
        QRect rect;
        bool relative = true;  ///< 項目の左上からの位置か（false なら項目の位置によらない）
    };
    mutable QHash<StandardPixmap, QIcon> m_icons;
    mutable QHash<ItemRectKey, CachedItemRect> m_itemRects;
};
} // namespace

QStyle* ApplicationStyle::create()
{
    return new ApplicationFusionStyle;
}
