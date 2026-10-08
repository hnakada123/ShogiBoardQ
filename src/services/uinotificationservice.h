#ifndef UINOTIFICATIONSERVICE_H
#define UINOTIFICATIONSERVICE_H

/// @file uinotificationservice.h
/// @brief エラー通知ダイアログ表示サービスの定義

#include "errorbus.h"
#include <QObject>
#include <QStringList>

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
        QWidget* parentWidget = nullptr;
    };

    /**
     * @brief 生存期間中の通知をダイアログで出さずに記録する（自動化 API 用）
     *
     * 自動化 API の呼び出し中にモーダルダイアログを開くと、応答が返らず、
     * 入れ子のイベントループで次の要求が処理されてしまう。
     * 呼び出し側は記録した内容をエラー応答として返す。入れ子にできる。
     */
    class ScopedCapture
    {
    public:
        ScopedCapture();
        ~ScopedCapture();
        ScopedCapture(const ScopedCapture&) = delete;
        ScopedCapture& operator=(const ScopedCapture&) = delete;

        /// エラー・重大エラーの内容
        const QStringList& errors() const { return m_errors; }
        /// 情報・警告の内容
        const QStringList& notices() const { return m_notices; }

    private:
        friend class UiNotificationService;
        QStringList m_errors;
        QStringList m_notices;
        ScopedCapture* m_previous = nullptr;
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
