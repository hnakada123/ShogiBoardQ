#include "kifuloadparser.h"
#include "csatosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "jkftosfenconverter.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"
#include "sfenpositiontracer.h"
#include "sfenutils.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace {
using Format = KifuFileReader::KifuFormat;
bool canceled(const CancelFlag& cancel) { return cancel && cancel->load(); }

Format formatForPath(const QString& path)
{
    const auto suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("ki2") || suffix == QLatin1String("ki2u")) return Format::KI2;
    if (suffix == QLatin1String("csa")) return Format::CSA;
    if (suffix == QLatin1String("jkf")) return Format::JKF;
    if (suffix == QLatin1String("usen")) return Format::USEN;
    if (suffix == QLatin1String("usi") || suffix == QLatin1String("sfen")) return Format::USI;
    return Format::KIF;
}
}

KifuLoadResult KifuLoadParser::parseFile(const QString& path, Format format, const CancelFlag& cancel)
{
    KifuLoadResult result;
    result.filePath = path;
    if (canceled(cancel)) return result;
    if (format == Format::Unknown) format = formatForPath(path);
    auto& record = result.record;
    auto* warning = &result.warning;
    auto* label = &result.teaiLabel;
    switch (format) {
    case Format::KI2:
        result.initialSfen = KifToSfenConverter::detectInitialSfenFromFile(path, label);
        result.success = Ki2ToSfenConverter::parseWithVariations(path, record, warning);
        if (result.success) result.gameInfo = Ki2ToSfenConverter::extractGameInfo(path);
        break;
    case Format::CSA:
        result.success = CsaToSfenConverter::parse(path, record, warning);
        result.initialSfen = record.mainline.baseSfen;
        result.teaiLabel = SfenUtils::isHirateStart(result.initialSfen)
            ? QStringLiteral("平手") : QStringLiteral("局面指定");
        if (result.success) result.gameInfo = CsaToSfenConverter::extractGameInfo(path);
        break;
    case Format::JKF:
        result.initialSfen = JkfToSfenConverter::detectInitialSfenFromFile(path, label);
        result.success = JkfToSfenConverter::parseWithVariations(path, record, warning);
        if (result.success) result.gameInfo = JkfToSfenConverter::extractGameInfo(path);
        break;
    case Format::USEN:
        result.initialSfen = UsenToSfenConverter::detectInitialSfenFromFile(path, label);
        result.success = UsenToSfenConverter::parseWithVariations(path, record, warning);
        if (result.success) result.gameInfo = UsenToSfenConverter::extractGameInfo(path);
        break;
    case Format::USI: {
        QString base;
        QStringList moves;
        QString terminal;
        if (UsiToSfenConverter::parseUsiFile(path, base, moves, &terminal, warning)
            && moves.isEmpty() && terminal.isEmpty()) {
            result.success = true;
            result.positionOnly = base;
            return result;
        }
        result.initialSfen = UsiToSfenConverter::detectInitialSfenFromFile(path, label);
        result.success = UsiToSfenConverter::parseWithVariations(path, record, warning);
        break;
    }
    default:
        result.initialSfen = KifToSfenConverter::detectInitialSfenFromFile(path, label);
        result.success = KifToSfenConverter::parseWithVariations(path, record, warning);
        if (result.success) result.gameInfo = KifToSfenConverter::extractGameInfo(path);
        break;
    }
    if (!result.success) {
        const QString detail = result.warning.isEmpty() ? QString() : QLatin1Char('\n') + result.warning;
        result.error = QCoreApplication::translate("KifuLoadCoordinator", "棋譜ファイルの読み込みに失敗しました: %1%2").arg(QFileInfo(path).fileName(), detail);
        return result;
    }
    if (canceled(cancel)) return result;
    if (result.initialSfen.isEmpty()) result.initialSfen = SfenUtils::hirateSfen();
    if (record.mainline.sfenList.isEmpty() && !record.mainline.usiMoves.isEmpty()) {
        record.mainline.sfenList = SfenPositionTracer::buildSfenRecord(
            record.mainline.baseSfen, record.mainline.usiMoves, false);
    }
    for (auto& variation : record.variations) {
        if (canceled(cancel)) break;
        auto& line = variation.line;
        if (!line.sfenList.isEmpty() || line.usiMoves.isEmpty()) continue;
        const int ply = variation.startPly - 1;
        if (line.baseSfen.isEmpty() && ply >= 0 && ply < record.mainline.sfenList.size())
            line.baseSfen = record.mainline.sfenList.at(ply);
        if (!line.baseSfen.isEmpty())
            line.sfenList = SfenPositionTracer::buildSfenRecord(line.baseSfen, line.usiMoves, line.endsWithTerminal);
    }
    if (canceled(cancel)) return result;
    const auto& line = record.mainline;
    const QString terminalText = line.disp.isEmpty() ? QString() : line.disp.last().prettyMove;
    const QStringList terminals = {QStringLiteral("投了"), QStringLiteral("中断"), QStringLiteral("持将棋"),
        QStringLiteral("千日手"), QStringLiteral("切れ負け"), QStringLiteral("反則勝ち"), QStringLiteral("反則負け"),
        QStringLiteral("入玉勝ち"), QStringLiteral("不戦勝"), QStringLiteral("不戦敗"), QStringLiteral("詰み"), QStringLiteral("不詰")};
    bool hasTerminal = line.endsWithTerminal;
    for (const auto& terminal : terminals) hasTerminal |= terminalText.contains(terminal);
    result.sfens = SfenPositionTracer::buildSfenRecord(result.initialSfen, line.usiMoves, hasTerminal);
    if (canceled(cancel)) return result;
    result.moves = SfenPositionTracer::buildGameMoves(result.initialSfen, line.usiMoves);
    QString command = QStringLiteral("position sfen %1").arg(result.initialSfen);
    result.positionCommands.append(command);
    command += QStringLiteral(" moves");
    for (const auto& move : line.usiMoves) {
        if (canceled(cancel)) break;
        command += QLatin1Char(' ') + move;
        result.positionCommands.append(command);
    }
    return result;
}

KifuLoadResult KifuLoadParser::parseText(const QString& content, const CancelFlag& cancel)
{
    KifuLoadResult result;
    if (canceled(cancel)) return result;
    if (content.trimmed().isEmpty()) {
        result.error = QCoreApplication::translate("KifuLoadCoordinator", "貼り付けるテキストが空です。");
        return result;
    }
    const auto format = KifuFileReader::detectFormat(content);
    if (format == Format::SFEN) {
        result.success = true;
        result.positionOnly = content.trimmed();
        return result;
    }
    if (format == Format::BOD) {
        result.success = KifToSfenConverter::buildInitialSfenFromBod(
            content.split(QLatin1Char('\n')), result.positionOnly, &result.teaiLabel, &result.warning)
            && !result.positionOnly.isEmpty();
        if (!result.success) result.error = QCoreApplication::translate("KifuApplyService", "BOD形式の解析に失敗しました。%1").arg(result.warning);
        return result;
    }
    const auto file = KifuFileReader::createTempFile(format, content);
    if (!file) {
        result.error = QCoreApplication::translate("KifuLoadCoordinator", "一時ファイルの作成に失敗しました。");
        return result;
    }
    // 一時ファイルは解析が終わるまで、このワーカー内で所有する。
    return parseFile(file->fileName(), format, cancel);
}
