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

#include "errorkinds.h"

/// メソッド実装が投げるエラー。分類（ErrorKind）が JSON-RPC のコードと `data.slug` を決め、`data.hint` に対処方法を入れる
class AutomationError
{
public:
    AutomationError(ErrorKind kind, QString message, QString hint = QString())
        : m_kind(kind), m_message(std::move(message)), m_hint(std::move(hint)) {}
    /// 分類つきの失敗（GUI に依存しないサービスが返したもの）をそのまま写す
    explicit AutomationError(const Failure& failure)
        : m_kind(failure.kind), m_message(failure.message), m_hint(failure.hint) {}
    ErrorKind kind() const { return m_kind; }
    int code() const { return ErrorKinds::rpcCode(m_kind); }
    const QString& message() const { return m_message; }
    const QString& hint() const { return m_hint; }

private:
    ErrorKind m_kind;
    QString m_message;
    QString m_hint;
};

/**
 * @brief 改行区切り JSON-RPC 2.0 のリクエストを解析し、登録済みメソッドへ振り分ける
 *
 * バッチリクエストは未対応（Invalid Request）。通知（id 無し）には応答しない。
 * AutomationMethodSpecs の表にあるメソッドは、呼ぶ前に引数を検査して既定値を補う。
 * メソッド実装は `AutomationError` を投げてエラー応答にできる。
 */
class AutomationDispatcher
{
public:
    using Handler = std::function<QJsonValue(const QJsonObject& params)>;

    void registerMethod(const QString& name, Handler handler);
    QStringList methodNames() const;

    /// 1 行（1 メッセージ）を処理して応答（改行付き）を返す。通知なら空
    QByteArray handleLine(const QByteArray& line) const;
    /// 解析済みのリクエストを処理して応答オブジェクトを返す。通知なら空オブジェクト
    QJsonObject handleRequest(const QJsonObject& request) const;

    static QJsonObject makeResult(const QJsonValue& id, const QJsonValue& result);
    /// エラー応答（`error.code` は分類の JSON-RPC のコード、`error.data.slug` は分類の語）
    static QJsonObject makeError(const QJsonValue& id, ErrorKind kind, const QString& message,
                                 const QString& hint = QString());

private:
    QHash<QString, Handler> m_handlers;
    /// 実行中のハンドラ数。ハンドラ内の入れ子のイベントループで次の要求が届いたときの判定に使う
    mutable int m_activeHandlers = 0;
};

#endif // AUTOMATIONDISPATCHER_H
