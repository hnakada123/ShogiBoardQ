/// @file usioptionlineparser.cpp
/// @brief USI option 行パーサの実装

#include "usioptionlineparser.h"
#include "enginesettingsconstants.h"
#include "logcategories.h"

#include <QRegularExpression>
#include <QSet>

using namespace EngineSettingsConstants;

bool parseUsiOptionLine(const QString& rawLine, ParsedOptionLine& parsed, QString* errorMessage)
{
    static const QRegularExpression whitespaceRe(QStringLiteral("\\s+"));
    static const QSet<QString> allAttributeKeywords{
        QStringLiteral("default"), QStringLiteral("min"),
        QStringLiteral("max"),     QStringLiteral("var")
    };

    const QString line = rawLine.trimmed();
    const QStringList tokens = line.split(whitespaceRe, Qt::SkipEmptyParts);

    if (tokens.size() < 4) {
        if (errorMessage) *errorMessage = QStringLiteral("Too few tokens");
        return false;
    }
    if (tokens.at(0) != QStringLiteral("option")
        || tokens.at(1) != QStringLiteral("name")) {
        if (errorMessage) *errorMessage = QStringLiteral("Command must start with 'option name'");
        return false;
    }

    const qsizetype typeIndex = tokens.indexOf(QStringLiteral("type"), 2);
    if (typeIndex <= 2 || typeIndex + 1 >= tokens.size()) {
        if (errorMessage) *errorMessage = QStringLiteral("Missing 'type' section");
        return false;
    }

    parsed.name = tokens.mid(2, typeIndex - 2).join(QStringLiteral(" "));
    parsed.type = tokens.at(typeIndex + 1);

    if (parsed.name.isEmpty() || parsed.type.isEmpty()) {
        if (errorMessage) *errorMessage = QStringLiteral("Option name/type is empty");
        return false;
    }

    const auto isAllowedKeyword = [&](const QString& key) {
        if (parsed.type == QLatin1String(OptionTypeSpin)) {
            return key == QStringLiteral("default")
                || key == QStringLiteral("min")
                || key == QStringLiteral("max");
        }
        if (parsed.type == QLatin1String(OptionTypeCheck)) {
            return key == QStringLiteral("default");
        }
        if (parsed.type == QLatin1String(OptionTypeCombo)) {
            return key == QStringLiteral("default")
                || key == QStringLiteral("var");
        }
        if (parsed.type == QLatin1String(OptionTypeButton)) {
            return false;
        }
        if (parsed.type == QLatin1String(OptionTypeString)
            || parsed.type == QLatin1String(OptionTypeFilename)) {
            return key == QStringLiteral("default");
        }
        return allAttributeKeywords.contains(key);
    };

    if (parsed.type == QLatin1String(OptionTypeButton)) {
        if (typeIndex + 2 < tokens.size()) {
            qCWarning(lcUi) << "parseUsiOptionLine: button option has extra attributes, ignore tail:"
                            << line;
        }
        return true;
    }

    if (parsed.type == QLatin1String(OptionTypeString)
        || parsed.type == QLatin1String(OptionTypeFilename)) {
        const qsizetype defaultIndex = tokens.indexOf(QStringLiteral("default"), typeIndex + 2);
        if (defaultIndex < 0) {
            if (typeIndex + 2 < tokens.size()) {
                qCWarning(lcUi) << "parseUsiOptionLine: unknown option attribute, ignore tail:"
                                << tokens.at(typeIndex + 2) << line;
            }
            return true;
        }
        if (defaultIndex > typeIndex + 2) {
            qCWarning(lcUi) << "parseUsiOptionLine: unknown option attribute, ignore prefix:"
                            << tokens.at(typeIndex + 2) << line;
        }
        // string / filename は "default" 以降をそのまま値として扱う。
        parsed.defaultValue = tokens.mid(defaultIndex + 1).join(QStringLiteral(" "));
        return true;
    }

    for (qsizetype i = typeIndex + 2; i < tokens.size();) {
        const QString key = tokens.at(i);
        if (!isAllowedKeyword(key)) {
            qCWarning(lcUi) << "parseUsiOptionLine: unknown option attribute, ignore tail:"
                            << key << line;
            break;
        }

        qsizetype j = i + 1;
        while (j < tokens.size() && !isAllowedKeyword(tokens.at(j))) {
            ++j;
        }
        const QString value = tokens.mid(i + 1, j - (i + 1)).join(QStringLiteral(" "));

        if (key == QStringLiteral("default")) {
            if (value.isEmpty()) {
                if (errorMessage) *errorMessage = QStringLiteral("default value is missing");
                return false;
            }
            parsed.defaultValue = value;
        } else if (key == QStringLiteral("min")) {
            if (value.isEmpty()) {
                if (errorMessage) *errorMessage = QStringLiteral("min value is missing");
                return false;
            }
            parsed.minValue = value;
        } else if (key == QStringLiteral("max")) {
            if (value.isEmpty()) {
                if (errorMessage) *errorMessage = QStringLiteral("max value is missing");
                return false;
            }
            parsed.maxValue = value;
        } else if (key == QStringLiteral("var")) {
            if (value.isEmpty()) {
                if (errorMessage) *errorMessage = QStringLiteral("var value is missing");
                return false;
            }
            parsed.comboValues.append(value);
        }

        i = j;
    }

    return true;
}
