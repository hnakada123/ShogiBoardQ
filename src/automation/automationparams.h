#ifndef AUTOMATIONPARAMS_H
#define AUTOMATIONPARAMS_H

/// @file automationparams.h
/// @brief 自動化 API のメソッド実装が使う params 取り出しヘルパ

#include "automationdispatcher.h"

#include <QJsonObject>
#include <QString>

namespace AutomationParams {

/// 必須の文字列（空文字列は不可）
inline QString requireString(const QJsonObject& params, const QString& key)
{
    const QJsonValue value = params.value(key);
    if (!value.isString() || value.toString().isEmpty()) {
        throw AutomationError(AutomationErrorCode::InvalidParams,
                              QStringLiteral("\"%1\" must be a non-empty string").arg(key));
    }
    return value.toString();
}

/// 任意の文字列（未指定なら既定値）
inline QString optionalString(const QJsonObject& params, const QString& key, const QString& defaultValue = QString())
{
    const QJsonValue value = params.value(key);
    if (value.isUndefined() || value.isNull()) return defaultValue;
    if (!value.isString()) {
        throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("\"%1\" must be a string").arg(key));
    }
    return value.toString();
}

/// 任意の真偽値
inline bool optionalBool(const QJsonObject& params, const QString& key, bool defaultValue = false)
{
    const QJsonValue value = params.value(key);
    if (value.isUndefined() || value.isNull()) return defaultValue;
    if (!value.isBool()) {
        throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("\"%1\" must be a boolean").arg(key));
    }
    return value.toBool();
}

/// 任意の整数（範囲外はエラー）
inline int optionalInt(const QJsonObject& params, const QString& key, int defaultValue, int minimum, int maximum)
{
    const QJsonValue value = params.value(key);
    if (value.isUndefined() || value.isNull()) return defaultValue;
    if (!value.isDouble()) {
        throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("\"%1\" must be an integer").arg(key));
    }
    const double d = value.toDouble();
    if (d < minimum || d > maximum || d != static_cast<double>(static_cast<int>(d))) {
        throw AutomationError(AutomationErrorCode::InvalidParams,
                              QStringLiteral("\"%1\" must be an integer between %2 and %3").arg(key).arg(minimum).arg(maximum));
    }
    return static_cast<int>(d);
}

/// 必須の整数
inline int requireInt(const QJsonObject& params, const QString& key, int minimum, int maximum)
{
    if (!params.contains(key)) {
        throw AutomationError(AutomationErrorCode::InvalidParams, QStringLiteral("\"%1\" is required").arg(key));
    }
    return optionalInt(params, key, minimum, minimum, maximum);
}

} // namespace AutomationParams

#endif // AUTOMATIONPARAMS_H
