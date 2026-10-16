/// @file clioutput.cpp
/// @brief shogiboardq-cli の JSON 出力ヘルパの実装

#include "clioutput.h"
#include "clicommandspecs.h"

#include <QJsonDocument>

#include <cstdio>

void CliOutput::printJson(const QJsonObject& object)
{
    const QByteArray bytes = QJsonDocument(object).toJson(QJsonDocument::Compact) + '\n';
    std::fwrite(bytes.constData(), 1, static_cast<size_t>(bytes.size()), stdout);
    std::fflush(stdout);
}

void CliOutput::printEvent(const QString& name, QJsonObject fields)
{
    fields[QStringLiteral("event")] = name;
    printJson(fields);
}

int CliOutput::fail(const QString& code, const QString& message)
{
    QJsonObject error;
    error[QStringLiteral("code")] = code;
    error[QStringLiteral("message")] = message;
    QJsonObject obj;
    obj[QStringLiteral("ok")] = false;
    obj[QStringLiteral("error")] = error;
    printJson(obj);
    return 1;
}

int CliOutput::failEvent(const QString& code, const QString& message)
{
    QJsonObject obj;
    obj[QStringLiteral("code")] = code;
    obj[QStringLiteral("message")] = message;
    printEvent(QStringLiteral("error"), obj);
    return 1;
}

void CliOutput::printUsage()
{
    std::fputs(CliCommandSpecs::usage().toUtf8().constData(), stdout);
    std::fflush(stdout);
}
