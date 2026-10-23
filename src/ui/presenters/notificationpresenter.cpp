/// @file notificationpresenter.cpp
/// @brief エラー・情報の知らせをダイアログで出す NotificationPresenter の実装

#include "notificationpresenter.h"
#include <QMessageBox>

namespace {
// 通知はメインスレッドからのみ表示する（UI アクセスはメインスレッド限定）
NotificationPresenter::ScopedCapture* s_activeCapture = nullptr;
}

NotificationPresenter::ScopedCapture::ScopedCapture()
    : m_previous(s_activeCapture)
{
    s_activeCapture = this;
}

NotificationPresenter::ScopedCapture::~ScopedCapture()
{
    s_activeCapture = m_previous;
}

NotificationPresenter::NotificationPresenter(const Deps& deps, QObject* parent)
    : QObject(parent)
    , m_deps(deps)
{
}

void NotificationPresenter::displayErrorMessage(const QString& message)
{
    displayMessage(ErrorBus::ErrorLevel::Error, message);
}

void NotificationPresenter::displayMessage(ErrorBus::ErrorLevel level, const QString& message)
{
    // エラーを表示しても盤面の描画と操作は止めない（棋譜の読み込み失敗などは表示中の局面に影響しない）
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

void NotificationPresenter::showInformation(const QString& title, const QString& message)
{
    if (s_activeCapture != nullptr) {
        s_activeCapture->m_notices.append(message);
        return;
    }
    QMessageBox::information(m_deps.parentWidget, title, message);
}
