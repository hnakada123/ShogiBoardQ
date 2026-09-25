#ifndef AUTOMATIONDISPATCHER_H
#define AUTOMATIONDISPATCHER_H

/// @file automationdispatcher.h
/// @brief 自動化 API の JSON-RPC 2.0 解析とメソッド表（GUI 非依存）

#include <QByteArray>
#include <QHash>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QStringList>
#include <functional>

/// JSON-RPC のエラーコード（標準 + アプリ固有）
namespace AutomationErrorCode {
constexpr int ParseError = -32700;
constexpr int InvalidRequest = -32600;
constexpr int MethodNotFound = -32601;
constexpr int InvalidParams = -32602;
constexpr int InternalError = -32603;
constexpr int NotAllowed = -32001;      ///< 許可リスト外の動作
constexpr int InvalidState = -32002;    ///< 現在の状態では実行できない
constexpr int FileError = -32003;       ///< パスが不正・既存ファイル・入出力失敗
constexpr int UnsavedChanges = -32004;  ///< 未保存の変更がある
constexpr int NotFound = -32005;        ///< 対象が見つからない
} // namespace AutomationErrorCode

/// メソッド実装が投げるエラー。`data.hint` に対処方法を入れる
class AutomationError
{
public:
    AutomationError(int code, QString message, QString hint = QString())
        : m_code(code), m_message(std::move(message)), m_hint(std::move(hint)) {}
    int code() const { return m_code; }
    const QString& message() const { return m_message; }
    const QString& hint() const { return m_hint; }

private:
    int m_code;
    QString m_message;
    QString m_hint;
};

/**
 * @brief 改行区切り JSON-RPC 2.0 のリクエストを解析し、登録済みメソッドへ振り分ける
 *
 * バッチリクエストは未対応（Invalid Request）。通知（id 無し）には応答しない。
 * メソッド実装は `AutomationError` を投げてエラー応答にできる。
 */
class AutomationDispatcher
{
public:
    using Handler = std::function<QJsonValue(const QJsonObject& params)>;

    void registerMethod(const QString& name, Handler handler);
    bool hasMethod(const QString& name) const { return m_handlers.contains(name); }
    QStringList methodNames() const;

    /// 1 行（1 メッセージ）を処理して応答（改行付き）を返す。通知なら空
    QByteArray handleLine(const QByteArray& line) const;
    /// 解析済みのリクエストを処理して応答オブジェクトを返す。通知なら空オブジェクト
    QJsonObject handleRequest(const QJsonObject& request) const;

    static QJsonObject makeResult(const QJsonValue& id, const QJsonValue& result);
    static QJsonObject makeError(const QJsonValue& id, int code, const QString& message,
                                 const QString& hint = QString());

private:
    QHash<QString, Handler> m_handlers;
};

#endif // AUTOMATIONDISPATCHER_H
