/// @file clioutput.cpp
/// @brief shogiboardq-cli の JSON 出力ヘルパの実装

#include "clioutput.h"

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
    static const char* const usage =
        "Usage: shogiboardq-cli <command> [options]\n"
        "\n"
        "Commands (output is JSON on stdout; long-running commands emit JSON Lines):\n"
        "  version                       Print the application version\n"
        "  list-engines                  List USI engines registered in ShogiBoardQ\n"
        "  validate-sfen --sfen SFEN     Validate a SFEN and report turn/hands/check/legal moves\n"
        "  convert-kifu --input PATH|--text TEXT --output-format kif|ki2|csa|jkf|usi|usen|sfen\n"
        "               [--input-format auto|kif|ki2|csa|jkf|usi|usen] [--output PATH] [--overwrite]\n"
        "  render-board --sfen SFEN --output PATH.png [--square-size N] [--flip] [--last-move 7g7f]\n"
        "               [--black-name NAME] [--white-name NAME] [--overwrite]\n"
        "  analyze --engine NAME --sfen SFEN|startpos [--moves \"7g7f 3c3d\"] [--seconds N] [--multipv N]\n"
        "  mate --engine NAME --sfen SFEN [--moves ...] [--seconds N]\n"
        "  generate-tsume --engine NAME [--target-moves N] [--max-positions N] [--timeout-ms N]\n"
        "               [--max-attack N] [--max-defend N] [--attack-range N]\n"
        "               [--no-remaining-to-hand] [--no-final-alternatives]\n"
        "  verify-tsume --engine NAME --sfen SFEN --target-moves N [--timeout-ms N] [--no-final-alternatives]\n"
        "\n"
        "Long-running commands accept --stdin-control: a line 'stop' on stdin (or EOF) stops them.\n"
        "Engines are referenced by the name registered in ShogiBoardQ; arbitrary paths are not accepted.\n";
    std::fputs(usage, stdout);
    std::fflush(stdout);
}
