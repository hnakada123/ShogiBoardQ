#ifndef UINOTIFICATIONSERVICE_H
#define UINOTIFICATIONSERVICE_H

/// @file uinotificationservice.h
/// @brief エラー通知ダイアログ表示サービスの定義

#include "errorbus.h"
#include <QObject>

class ShogiView;

/**
 * @brief エラー通知表示を一元管理するサービス
 *
 * MainWindow から displayErrorMessage / onErrorBusOccurred のロジックを分離し、
 * ErrorBus / KifuLoadCoordinator / CsaGameWiring からのエラー通知を受け取る。
 */
class UiNotificationService : public QObject
{
    Q_OBJECT

public:
    struct Deps {
        bool* errorOccurred = nullptr;
        ShogiView* shogiView = nullptr;
        QWidget* parentWidget = nullptr;
    };

    explicit UiNotificationService(QObject* parent = nullptr);

    void updateDeps(const Deps& deps);

public slots:
    void displayErrorMessage(const QString& message);
    void displayMessage(ErrorBus::ErrorLevel level, const QString& message);

private:
    Deps m_deps;
};

#endif // UINOTIFICATIONSERVICE_H
