/// @file notificationpresenter.cpp
/// @brief エラー・情報の知らせをダイアログで出す NotificationPresenter の実装

#include "notificationpresenter.h"
#include <QMessageBox>

namespace {
// 通知はメインスレッドからのみ表示する（UI アクセスはメインスレッド限定）
NotificationPresenter::ScopedCapture* s_activeCapture = nullptr;
} // namespace

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

void NotificationPresenter::displayWarningMessage(const QString& message)
{
    displayMessage(ErrorBus::ErrorLevel::Warning, message);
}

void NotificationPresenter::displayMessage(ErrorBus::ErrorLevel level, const QString& message)
{
    // エラーを表示しても盤面の描画と操作は止めない（棋譜の読み込み失敗などは表示中の局面に影響しない）
    switch (level) {
    case ErrorBus::ErrorLevel::Info:
        information(m_deps.parentWidget, tr("Information"), message);
        break;
    case ErrorBus::ErrorLevel::Warning:
        warning(m_deps.parentWidget, tr("Warning"), message);
        break;
    case ErrorBus::ErrorLevel::Error:
    case ErrorBus::ErrorLevel::Critical:
        critical(m_deps.parentWidget, tr("Error"), message);
        break;
    }
}

void NotificationPresenter::showInformation(const QString& title, const QString& message)
{
    information(m_deps.parentWidget, title, message);
}

void NotificationPresenter::information(QWidget* parent, const QString& title, const QString& message, Wait wait)
{
    show(parent, Kind::Information, title, message, wait);
}

void NotificationPresenter::warning(QWidget* parent, const QString& title, const QString& message)
{
    show(parent, Kind::Warning, title, message, Wait::UntilClosed);
}

void NotificationPresenter::critical(QWidget* parent, const QString& title, const QString& message)
{
    show(parent, Kind::Critical, title, message, Wait::UntilClosed);
}

void NotificationPresenter::show(QWidget* parent, Kind kind, const QString& title, const QString& message, Wait wait)
{
    // 自動化 API の呼び出し中はダイアログを出さずに記録する
    if (s_activeCapture != nullptr) {
        (kind == Kind::Critical ? s_activeCapture->m_errors : s_activeCapture->m_notices).append(message);
        return;
    }
    const QMessageBox::Icon icon = kind == Kind::Critical ? QMessageBox::Critical
        : kind == Kind::Warning ? QMessageBox::Warning : QMessageBox::Information;
    auto* box = new QMessageBox(icon, title, message, QMessageBox::Ok, parent);
    box->setAttribute(Qt::WA_DeleteOnClose);
    // 知らせの文には CSA サーバーの受信行や棋譜の行・ファイル名が入るので、HTML として解釈させない
    // （QMessageBox の静的関数は文を自動判定し、リンクを外部のアプリで開く）
    box->setTextFormat(Qt::PlainText);
    if (wait == Wait::No) {
        box->setModal(false);
        box->show();
        return;
    }
    box->exec();
}
