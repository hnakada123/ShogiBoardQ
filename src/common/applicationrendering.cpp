/// @file applicationrendering.cpp
/// @brief アプリ全体の描画の設定の実装

#include "applicationrendering.h"

#include <QByteArray>
#include <QPixmapCache>
#include <QtGlobal>

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
