/// @file applicationrendering.cpp
/// @brief アプリ全体の描画の設定の実装

#include "applicationrendering.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QByteArray>
#include <QEvent>
#include <QPalette>
#include <QPixmapCache>
#include <QtGlobal>

namespace {
// アプリのパレットの「後ろのとき」（Inactive）の色を「前面のとき」（Active）の色に合わせ続ける。
// 合わせた色は明示した色として残るので、デスクトップの配色が変わったとき（Active だけが新しい色になる）は
// 合わせ直す。
class InactiveColorKeeper final : public QObject
{
public:
    explicit InactiveColorKeeper(QObject* parent)
        : QObject(parent)
    {
        apply();
        parent->installEventFilter(this);
    }

protected:
    bool eventFilter(QObject* watched, QEvent* event) override
    {
        if (watched == parent() && event->type() == QEvent::ApplicationPaletteChange) apply();
        return QObject::eventFilter(watched, event);
    }

private:
    void apply()
    {
        if (m_applying) return;
        QPalette palette = QGuiApplication::palette();
        bool changed = false;
        for (int index = 0; index < QPalette::NColorRoles; ++index) {
            const auto role = static_cast<QPalette::ColorRole>(index);
            if (role == QPalette::NoRole) continue;
            const QBrush active = palette.brush(QPalette::Active, role);
            if (palette.brush(QPalette::Inactive, role) == active) continue;
            palette.setBrush(QPalette::Inactive, role, active);
            changed = true;
        }
        if (!changed) return;
        m_applying = true;
        QGuiApplication::setPalette(palette);
        m_applying = false;
    }

    bool m_applying = false;
};

constexpr char kInactiveColorKeeperName[] = "sbqInactiveColorKeeper";
} // namespace

void ApplicationRendering::paintOnGuiThreadOnly()
{
    // Qt は値を見ず、あるかどうかだけで判断する。
    static constexpr char kNoGuiThreadPool[] = "QT_NO_GUI_THREADPOOL";
    if (!qEnvironmentVariableIsSet(kNoGuiThreadPool)) qputenv(kNoGuiThreadPool, "1");
}

void ApplicationRendering::reservePixmapCacheForPieces()
{
    if (QPixmapCache::cacheLimit() < kPixmapCacheLimitKb) QPixmapCache::setCacheLimit(kPixmapCacheLimitKb);
}

void ApplicationRendering::useActiveColorsWhenInactive()
{
    QCoreApplication* app = QCoreApplication::instance();
    if (!app || app->findChild<QObject*>(QLatin1String(kInactiveColorKeeperName), Qt::FindDirectChildrenOnly)) return;
    (new InactiveColorKeeper(app))->setObjectName(QLatin1String(kInactiveColorKeeperName));
}
