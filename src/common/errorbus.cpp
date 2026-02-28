/// @file errorbus.cpp
/// @brief アプリケーション全体のエラー通知を一元管理するイベントバスの実装

#include "errorbus.h"

// ============================================================================
// 公開API
// ============================================================================

ErrorBus& ErrorBus::instance() {
    static ErrorBus inst;
    return inst;
}

void ErrorBus::postMessage(ErrorLevel level, const QString& message)
{
    emit messagePosted(level, message);

    // 後方互換: Error/Critical は旧シグナルも発火
    if (level == ErrorLevel::Error || level == ErrorLevel::Critical) {
        emit errorOccurred(message);
    }
}

void ErrorBus::postError(const QString& message)
{
    postMessage(ErrorLevel::Error, message);
}
