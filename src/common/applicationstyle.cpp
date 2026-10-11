/// @file applicationstyle.cpp
/// @brief アプリ全体で使うスタイル（Fusion）の実装

#include "applicationstyle.h"

#include <QHash>
#include <QIcon>
#include <QProxyStyle>
#include <QStyleFactory>

namespace {
// Fusion を包み、内蔵の画像から作るアイコンを使い回すスタイル。ほかはすべて Fusion に任せる。
class CachedIconFusionStyle final : public QProxyStyle
{
public:
    CachedIconFusionStyle()
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
            auto it = m_icons.constFind(standardIcon);
            if (it == m_icons.cend())
                it = m_icons.insert(standardIcon, QProxyStyle::standardIcon(standardIcon, option, widget));
            return it.value();
        }
        default:
            return QProxyStyle::standardIcon(standardIcon, option, widget);
        }
    }

private:
    mutable QHash<int, QIcon> m_icons;
};
} // namespace

QStyle* ApplicationStyle::create()
{
    return new CachedIconFusionStyle;
}
