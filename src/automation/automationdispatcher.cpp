/// @file automationdispatcher.cpp
/// @brief 自動化 API の JSON-RPC 2.0 解析とメソッド表の実装

#include "automationdispatcher.h"
#include "logcategories.h"

#include <QJsonDocument>
#include <QJsonParseError>

#include <exception>

void AutomationDispatcher::registerMethod(const QString& name, Handler handler)
{
    m_handlers.insert(name, std::move(handler));
}

QStringList AutomationDispatcher::methodNames() const
{
    QStringList names = m_handlers.keys();
    names.sort();
    return names;
}

QJsonObject AutomationDispatcher::makeResult(const QJsonValue& id, const QJsonValue& result)
{
    QJsonObject response;
    response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    response[QStringLiteral("id")] = id;
    response[QStringLiteral("result")] = result;
    return response;
}

QJsonObject AutomationDispatcher::makeError(const QJsonValue& id, int code, const QString& message, const QString& hint)
{
    QJsonObject error;
    error[QStringLiteral("code")] = code;
    error[QStringLiteral("message")] = message;
    if (!hint.isEmpty()) {
        QJsonObject data;
        data[QStringLiteral("hint")] = hint;
        error[QStringLiteral("data")] = data;
    }
    QJsonObject response;
    response[QStringLiteral("jsonrpc")] = QStringLiteral("2.0");
    response[QStringLiteral("id")] = id.isUndefined() ? QJsonValue(QJsonValue::Null) : id;
    response[QStringLiteral("error")] = error;
    return response;
}

QByteArray AutomationDispatcher::handleLine(const QByteArray& line) const
{
    const QByteArray trimmed = line.trimmed();
    if (trimmed.isEmpty()) return {};

    QJsonParseError parseError{};
    const QJsonDocument document = QJsonDocument::fromJson(trimmed, &parseError);
    QJsonObject response;
    if (parseError.error != QJsonParseError::NoError) {
        response = makeError(QJsonValue::Null, AutomationErrorCode::ParseError,
                             QStringLiteral("Parse error: %1").arg(parseError.errorString()));
    } else if (document.isArray()) {
        response = makeError(QJsonValue::Null, AutomationErrorCode::InvalidRequest,
                             QStringLiteral("Batch requests are not supported; send one request per line"));
    } else if (!document.isObject()) {
        response = makeError(QJsonValue::Null, AutomationErrorCode::InvalidRequest,
                             QStringLiteral("A JSON-RPC request must be an object"));
    } else {
        response = handleRequest(document.object());
        if (response.isEmpty()) return {};
    }
    return QJsonDocument(response).toJson(QJsonDocument::Compact) + '\n';
}

QJsonObject AutomationDispatcher::handleRequest(const QJsonObject& request) const
{
    const QJsonValue id = request.value(QStringLiteral("id"));
    const bool isNotification = !request.contains(QStringLiteral("id"));

    if (request.value(QStringLiteral("jsonrpc")).toString() != QLatin1String("2.0")) {
        return makeError(id, AutomationErrorCode::InvalidRequest, QStringLiteral("jsonrpc must be \"2.0\""));
    }
    const QJsonValue methodValue = request.value(QStringLiteral("method"));
    if (!methodValue.isString() || methodValue.toString().isEmpty()) {
        return makeError(id, AutomationErrorCode::InvalidRequest, QStringLiteral("method must be a non-empty string"));
    }
    const QJsonValue paramsValue = request.value(QStringLiteral("params"));
    if (!paramsValue.isUndefined() && !paramsValue.isNull() && !paramsValue.isObject()) {
        return makeError(id, AutomationErrorCode::InvalidParams, QStringLiteral("params must be an object"));
    }

    const QString method = methodValue.toString();
    const auto it = m_handlers.constFind(method);
    if (it == m_handlers.constEnd()) {
        if (isNotification) return {};
        return makeError(id, AutomationErrorCode::MethodNotFound,
                         QStringLiteral("Unknown method \"%1\"").arg(method),
                         QStringLiteral("Available methods: %1").arg(methodNames().join(QStringLiteral(", "))));
    }

    QJsonObject response;
    try {
        const QJsonValue result = it.value()(paramsValue.toObject());
        response = makeResult(id, result);
    } catch (const AutomationError& error) {
        qCDebug(lcApp) << "automation" << method << "failed:" << error.code() << error.message();
        response = makeError(id, error.code(), error.message(), error.hint());
    } catch (const std::exception& error) {
        qCWarning(lcApp) << "automation" << method << "threw:" << error.what();
        response = makeError(id, AutomationErrorCode::InternalError,
                             QStringLiteral("Internal error: %1").arg(QString::fromUtf8(error.what())));
    }
    return isNotification ? QJsonObject() : response;
}
