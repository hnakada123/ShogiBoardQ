/// @file paramspec.cpp
/// @brief 操作の引数の表の実装（スキーマの生成と検査）

#include "paramspec.h"

#include <QJsonArray>

using Type = ParamSpec::Type;

const ParamSpec* OperationSpec::param(const QString& paramName) const
{
    for (const ParamSpec& spec : params)
        if (spec.name == paramName) return &spec;
    return nullptr;
}

namespace ParamSpecs {

namespace {
ParamSpec make(const QString& name, Type type, const QString& description)
{
    ParamSpec spec;
    spec.name = name;
    spec.type = type;
    spec.description = description;
    return spec;
}

QJsonObject propertySchema(const ParamSpec& spec)
{
    QJsonObject schema;
    switch (spec.type) {
    case Type::Integer:
        schema[QStringLiteral("type")] = QStringLiteral("integer");
        if (spec.minimum) schema[QStringLiteral("minimum")] = static_cast<double>(*spec.minimum);
        if (spec.maximum) schema[QStringLiteral("maximum")] = static_cast<double>(*spec.maximum);
        break;
    case Type::String:
        schema[QStringLiteral("type")] = QStringLiteral("string");
        if (!spec.choices.isEmpty()) schema[QStringLiteral("enum")] = QJsonArray::fromStringList(spec.choices);
        if (spec.minimum) schema[QStringLiteral("minLength")] = static_cast<double>(*spec.minimum);
        if (spec.maximum) schema[QStringLiteral("maxLength")] = static_cast<double>(*spec.maximum);
        break;
    case Type::Boolean:
        schema[QStringLiteral("type")] = QStringLiteral("boolean");
        break;
    case Type::StringArray:
    case Type::IntegerArray: {
        schema[QStringLiteral("type")] = QStringLiteral("array");
        QJsonObject items{{QStringLiteral("type"), spec.type == Type::StringArray ? QStringLiteral("string")
                                                                                   : QStringLiteral("integer")}};
        if (spec.itemMinimum) items[QStringLiteral("minimum")] = static_cast<double>(*spec.itemMinimum);
        schema[QStringLiteral("items")] = items;
        if (spec.minimum) schema[QStringLiteral("minItems")] = static_cast<double>(*spec.minimum);
        if (spec.maximum) schema[QStringLiteral("maxItems")] = static_cast<double>(*spec.maximum);
        break;
    }
    case Type::Object: {
        OperationSpec nested;
        nested.params = spec.properties;
        schema = inputSchema(nested);
        break;
    }
    case Type::Any:
        schema[QStringLiteral("type")] = QJsonArray{QStringLiteral("string"), QStringLiteral("number"),
                                                    QStringLiteral("boolean")};
        break;
    }
    if (!spec.description.isEmpty()) schema[QStringLiteral("description")] = spec.description;
    if (!spec.defaultValue.isUndefined()) schema[QStringLiteral("default")] = spec.defaultValue;
    return schema;
}

bool isInteger(const QJsonValue& value)
{
    if (!value.isDouble()) return false;
    const double number = value.toDouble();
    return number == static_cast<double>(static_cast<qint64>(number));
}

QString rangeText(const ParamSpec& spec)
{
    if (spec.minimum && spec.maximum) return QStringLiteral("between %1 and %2").arg(*spec.minimum).arg(*spec.maximum);
    if (spec.minimum) return QStringLiteral("at least %1").arg(*spec.minimum);
    return QStringLiteral("at most %1").arg(spec.maximum.value_or(0));
}

bool inRange(const ParamSpec& spec, qint64 value)
{
    return (!spec.minimum || value >= *spec.minimum) && (!spec.maximum || value <= *spec.maximum);
}

bool check(const ParamSpec& spec, const QJsonValue& value, QString* error)
{
    const auto fail = [error, &spec](const QString& what) {
        if (error) *error = QStringLiteral("\"%1\" %2").arg(spec.name, what);
        return false;
    };
    switch (spec.type) {
    case Type::Integer:
        if (!isInteger(value)) return fail(QStringLiteral("must be an integer"));
        if (!inRange(spec, static_cast<qint64>(value.toDouble())))
            return fail(QStringLiteral("must be an integer ") + rangeText(spec));
        return true;
    case Type::String: {
        if (!value.isString()) return fail(QStringLiteral("must be a string"));
        const QString text = value.toString();
        if (!spec.choices.isEmpty() && !spec.choices.contains(text))
            return fail(QStringLiteral("must be one of ") + spec.choices.join(QStringLiteral(", ")));
        if (!inRange(spec, text.size())) return fail(QStringLiteral("length must be ") + rangeText(spec));
        return true;
    }
    case Type::Boolean:
        return value.isBool() || fail(QStringLiteral("must be a boolean"));
    case Type::StringArray:
    case Type::IntegerArray: {
        if (!value.isArray()) return fail(QStringLiteral("must be an array"));
        const QJsonArray array = value.toArray();
        if (!inRange(spec, array.size())) return fail(QStringLiteral("must have a number of items ") + rangeText(spec));
        for (const QJsonValue& item : array) {
            if (spec.type == Type::StringArray && !item.isString())
                return fail(QStringLiteral("must contain only strings"));
            if (spec.type == Type::IntegerArray
                && (!isInteger(item) || (spec.itemMinimum && item.toDouble() < static_cast<double>(*spec.itemMinimum))))
                return fail(QStringLiteral("must contain only integers of at least %1").arg(spec.itemMinimum.value_or(0)));
        }
        return true;
    }
    case Type::Object: {
        if (!value.isObject()) return fail(QStringLiteral("must be an object"));
        OperationSpec nested;
        nested.params = spec.properties;
        QString nestedError;
        if (!validate(nested, value.toObject(), &nestedError))
            return fail(QStringLiteral("is invalid: ") + nestedError);
        return true;
    }
    case Type::Any:
        return (value.isString() || value.isDouble() || value.isBool())
               || fail(QStringLiteral("must be a string, number or boolean"));
    }
    return true;
}
} // namespace

ParamSpec integer(const QString& name, const QString& description, qint64 minimum, qint64 maximum)
{
    ParamSpec spec = make(name, Type::Integer, description);
    spec.range(minimum, maximum);
    spec.valueName = QStringLiteral("n");
    return spec;
}

ParamSpec string(const QString& name, const QString& description)
{
    ParamSpec spec = make(name, Type::String, description);
    spec.valueName = QStringLiteral("text");
    return spec;
}

ParamSpec boolean(const QString& name, const QString& description, bool defaultValue)
{
    return make(name, Type::Boolean, description).withDefault(defaultValue);
}

ParamSpec choice(const QString& name, const QString& description, const QStringList& choices)
{
    ParamSpec spec = make(name, Type::String, description);
    spec.choices = choices;
    spec.valueName = choices.join(QLatin1Char('|'));
    return spec;
}

ParamSpec stringArray(const QString& name, const QString& description)
{
    ParamSpec spec = make(name, Type::StringArray, description);
    spec.valueName = QStringLiteral("items");
    return spec;
}

ParamSpec integerArray(const QString& name, const QString& description, qint64 itemMinimum)
{
    ParamSpec spec = make(name, Type::IntegerArray, description);
    spec.itemMinimum = itemMinimum;
    return spec;
}

ParamSpec object(const QString& name, const QString& description, const QList<ParamSpec>& properties)
{
    ParamSpec spec = make(name, Type::Object, description);
    spec.properties = properties;
    return spec;
}

ParamSpec any(const QString& name, const QString& description)
{
    return make(name, Type::Any, description);
}

QJsonObject inputSchema(const OperationSpec& operation)
{
    QJsonObject properties;
    QJsonArray required;
    for (const ParamSpec& spec : operation.params) {
        properties[spec.name] = propertySchema(spec);
        if (spec.required) required.append(spec.name);
    }
    QJsonObject schema{{QStringLiteral("type"), QStringLiteral("object")},
                       {QStringLiteral("properties"), properties},
                       {QStringLiteral("additionalProperties"), false}};
    if (!required.isEmpty()) schema[QStringLiteral("required")] = required;
    return schema;
}

QJsonObject toJson(const OperationSpec& operation)
{
    return {{QStringLiteral("name"), operation.name},
            {QStringLiteral("summary"), operation.summary},
            {QStringLiteral("input_schema"), inputSchema(operation)}};
}

std::optional<QJsonObject> validate(const OperationSpec& operation, const QJsonObject& params, QString* error)
{
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (!operation.param(it.key())) {
            if (error) *error = QStringLiteral("Unknown parameter \"%1\"").arg(it.key());
            return std::nullopt;
        }
    }
    QJsonObject result = params;
    for (const ParamSpec& spec : operation.params) {
        const QJsonValue value = params.value(spec.name);
        if (value.isUndefined() || value.isNull()) {
            if (spec.required) {
                if (error) *error = QStringLiteral("\"%1\" is required").arg(spec.name);
                return std::nullopt;
            }
            result.remove(spec.name);
            if (!spec.defaultValue.isUndefined()) result[spec.name] = spec.defaultValue;
            continue;
        }
        if (!check(spec, value, error)) return std::nullopt;
    }
    return result;
}

} // namespace ParamSpecs
