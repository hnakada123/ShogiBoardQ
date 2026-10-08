/// @file uinotificationservice.cpp
/// @brief エラー通知ダイアログ表示サービスの実装

#include "uinotificationservice.h"
#include <QMessageBox>

namespace {
// 通知はメインスレッドからのみ表示する（UI アクセスはメインスレッド限定）
UiNotificationService::ScopedCapture* s_activeCapture = nullptr;
}

UiNotificationService::ScopedCapture::ScopedCapture()
    : m_previous(s_activeCapture)
{
    s_activeCapture = this;
}

UiNotificationService::ScopedCapture::~ScopedCapture()
{
    s_activeCapture = m_previous;
}

UiNotificationService::UiNotificationService(QObject* parent)
    : QObject(parent)
{
}

void UiNotificationService::updateDeps(const Deps& deps)
{
    m_deps = deps;
}

void UiNotificationService::displayErrorMessage(const QString& message)
{
    displayMessage(ErrorBus::ErrorLevel::Error, message);
}

void UiNotificationService::displayMessage(ErrorBus::ErrorLevel level, const QString& message)
{
    // エラーを表示しても盤面の描画と操作は止めない（棋譜の読み込み失敗などは表示中の局面に影響しない）
    if (level == ErrorBus::ErrorLevel::Error || level == ErrorBus::ErrorLevel::Critical) {
        if (m_deps.errorOccurred) {
            *m_deps.errorOccurred = true;
        }
    }

    if (s_activeCapture != nullptr) {
        const bool isError = level == ErrorBus::ErrorLevel::Error || level == ErrorBus::ErrorLevel::Critical;
        (isError ? s_activeCapture->m_errors : s_activeCapture->m_notices).append(message);
        return;
    }

    switch (level) {
    case ErrorBus::ErrorLevel::Info:
        QMessageBox::information(m_deps.parentWidget, tr("Information"), message);
        break;
    case ErrorBus::ErrorLevel::Warning:
        QMessageBox::warning(m_deps.parentWidget, tr("Warning"), message);
        break;
    case ErrorBus::ErrorLevel::Error:
    case ErrorBus::ErrorLevel::Critical:
        QMessageBox::critical(m_deps.parentWidget, tr("Error"), message);
        break;
    }
}
