#ifndef NOTIFICATIONPRESENTER_H
#define NOTIFICATIONPRESENTER_H

/// @file notificationpresenter.h
/// @brief エラー・情報の知らせをダイアログで出す NotificationPresenter の定義

#include "errorbus.h"
#include <QObject>
#include <QStringList>

/**
 * @brief エラー・情報の知らせをダイアログで出す（知らせの表示の唯一の持ち主）
 *
 * ErrorBus・KifuLoader・CSA 通信対局・終局・検討の終了などの知らせを受け取り、
 * メッセージボックスで出す。自動化 API の呼び出し中は ScopedCapture で記録だけする。
 */
class NotificationPresenter : public QObject
{
    Q_OBJECT

public:
    struct Deps {
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
        friend class NotificationPresenter;
        QStringList m_errors;
        QStringList m_notices;
        ScopedCapture* m_previous = nullptr;
    };

    explicit NotificationPresenter(const Deps& deps, QObject* parent = nullptr);

public slots:
    void displayErrorMessage(const QString& message);
    void displayMessage(ErrorBus::ErrorLevel level, const QString& message);
    /// 題名付きの知らせ（終局・検討の終了・詰み探索の結果）
    void showInformation(const QString& title, const QString& message);

private:
    Deps m_deps;
};

#endif // NOTIFICATIONPRESENTER_H
