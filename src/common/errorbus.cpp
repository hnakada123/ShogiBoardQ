/// @file errorbus.cpp
/// @brief アプリケーション全体のエラー通知を一元管理するイベントバスの実装
/// @todo remove コメントスタイルガイド適用済み

#include "errorbus.h"

// ============================================================================
// 公開API
// ============================================================================

/// @todo remove コメントスタイルガイド適用済み
ErrorBus& ErrorBus::instance() {
    static ErrorBus inst;
    return inst;
}
