#ifndef ERRORBUS_H
#define ERRORBUS_H

/// @file errorbus.h
/// @brief アプリケーション全体のエラー通知を一元管理するイベントバス

#include <QObject>
#include <QString>

/**
 * @brief シングルトンのエラー通知バス
 *
 * どこからでも postMessage() でメッセージを発行し、messagePosted シグナルで
 * 購読者へ一斉配信する。深刻度(ErrorLevel)に応じた表示の分岐が可能。
 *
 * 後方互換: postError() / errorOccurred() も引き続き使用可能。
 */
class ErrorBus final : public QObject {
    Q_OBJECT

public:
    /// メッセージの深刻度
    enum class ErrorLevel {
        Info,       ///< 情報通知（操作結果等）
        Warning,    ///< 続行可能だが注意が必要
        Error,      ///< 操作失敗（ファイルI/O失敗等）
        Critical    ///< アプリケーション継続に影響する致命的エラー
    };
    Q_ENUM(ErrorLevel)

    // --- 公開API ---

    /// シングルトンインスタンスを返す
    static ErrorBus& instance();

    /// 深刻度付きメッセージを発行する
    void postMessage(ErrorBus::ErrorLevel level, const QString& message);

    /// エラーメッセージを発行する（後方互換）
    void postError(const QString& message);

signals:
    // --- シグナル ---

    /// 深刻度付きメッセージが発行された
    void messagePosted(ErrorBus::ErrorLevel level, const QString& message);

    /// エラーが発生した（後方互換、Error/Critical で発火）
    void errorOccurred(const QString& message);

private:
    explicit ErrorBus(QObject* parent = nullptr) : QObject(parent) {}
    Q_DISABLE_COPY_MOVE(ErrorBus)
};

#endif // ERRORBUS_H
