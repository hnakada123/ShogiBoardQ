/// @file uinotificationservice.cpp
/// @brief エラー通知ダイアログ表示サービスの実装

#include "uinotificationservice.h"
#include "shogiview.h"
#include <QMessageBox>

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
    if (m_deps.errorOccurred) {
        *m_deps.errorOccurred = true;
    }

    if (m_deps.shogiView) {
        m_deps.shogiView->setErrorOccurred(true);
    }

    QMessageBox::critical(m_deps.parentWidget, tr("Error"), message);
}
