#include "kifuloadparser.h"
#include "csatosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "jkftosfenconverter.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"
#include "kifreader.h"
#include "sfenreplay.h"
#include "shogiboard.h"
#include "sfenutils.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace {
bool canceled(const CancelFlag& cancel) { return cancel && cancel->load(); }
}

bool KifuLoadParser::readFileText(const QString& path, KifuFormat format, QString& outText, QString* warn)
{
    if (format == KifuFormat::Csa) return CsaToSfenConverter::readFileText(path, outText, warn);
    QString encoding;
    return KifReader::readAllTextAuto(path, outText, &encoding, warn);
}

KifuLoadResult KifuLoadParser::parseFile(const QString& path, std::optional<KifuFormat> requested, const CancelFlag& cancel)
{
    if (canceled(cancel)) {
        KifuLoadResult result;
        result.filePath = path;
        return result;
    }
    const KifuFormat format = requested ? *requested : KifuFormats::detectFile(path);
    QString text;
    QString warning;
    if (!readFileText(path, format, text, &warning)) {
        KifuLoadResult result;
        result.filePath = path;
        result.warning = warning;
        result.error = QCoreApplication::translate("KifuLoadCoordinator", "棋譜ファイルの読み込みに失敗しました: %1%2")
                           .arg(QFileInfo(path).fileName(), QLatin1Char('\n') + warning);
        return result;
    }
    KifuLoadResult result = parseContent(text, format, cancel, QFileInfo(path).fileName());
    result.filePath = path;
    if (!warning.isEmpty()) result.warning.prepend(warning);
    return result;
}

KifuLoadResult KifuLoadParser::parseContent(const QString& content, KifuFormat format, const CancelFlag& cancel,
                                            const QString& sourceName)
{
    KifuLoadResult result;
    result.format = format;
    if (canceled(cancel)) return result;
    auto& record = result.record;
    auto* warning = &result.warning;
    auto* label = &result.teaiLabel;
    switch (format) {
    case KifuFormat::Ki2: {
        // 行に分けるのも局面図を読むのも1回だけ
        const QStringList lines = KifReader::splitLines(content);
        result.success = Ki2ToSfenConverter::parseLines(lines, record, warning, label);
        result.initialSfen = record.mainline.baseSfen;
        if (result.success) result.gameInfo = Ki2ToSfenConverter::extractGameInfoFromLines(lines);
        break;
    }
    case KifuFormat::Csa:
        result.success = CsaToSfenConverter::parseText(content, record, warning);
        result.initialSfen = record.mainline.baseSfen;
        result.teaiLabel = SfenUtils::isHirateStart(result.initialSfen)
            ? QStringLiteral("平手") : QStringLiteral("局面指定");
        if (result.success) result.gameInfo = CsaToSfenConverter::extractGameInfoFromText(content);
        break;
    case KifuFormat::Jkf:
        result.initialSfen = JkfToSfenConverter::detectInitialSfenFromText(content, label);
        result.success = JkfToSfenConverter::parseText(content, record, warning);
        if (result.success) result.gameInfo = JkfToSfenConverter::extractGameInfoFromText(content);
        break;
    case KifuFormat::Usen:
        result.initialSfen = UsenToSfenConverter::detectInitialSfenFromText(content, label);
        result.success = UsenToSfenConverter::parseText(content, record, warning);
        break;
    case KifuFormat::Usi:
    case KifuFormat::Sfen: {
        QString base;
        QStringList moves;
        QString terminal;
        if (UsiToSfenConverter::parseUsiText(content, base, moves, &terminal, warning)
            && moves.isEmpty() && terminal.isEmpty()) {
            result.success = true;
            result.positionOnly = base;
            return result;
        }
        result.initialSfen = UsiToSfenConverter::detectInitialSfenFromText(content, label);
        result.success = UsiToSfenConverter::parseText(content, record, warning);
        break;
    }
    case KifuFormat::Bod:
        // 局面図だけ（局面として読む）
        result.success = KifToSfenConverter::buildInitialSfenFromBod(
            content.split(QLatin1Char('\n')), result.positionOnly, label, warning)
            && !result.positionOnly.isEmpty();
        if (!result.success) {
            result.error = QCoreApplication::translate("KifuLoadCoordinator", "BOD形式の解析に失敗しました。%1").arg(result.warning);
        }
        return result;
    case KifuFormat::Kif: {
        const QStringList lines = KifReader::splitLines(content);
        result.success = KifToSfenConverter::parseLines(lines, record, warning, label);
        result.initialSfen = record.mainline.baseSfen;
        if (result.success) result.gameInfo = KifToSfenConverter::extractGameInfoFromLines(lines);
        break;
    }
    }
    if (!result.success) {
        const QString detail = result.warning.isEmpty() ? QString() : QLatin1Char('\n') + result.warning;
        result.error = sourceName.isEmpty()
            ? QCoreApplication::translate("KifuLoadCoordinator", "棋譜の読み込みに失敗しました。%1").arg(detail)
            : QCoreApplication::translate("KifuLoadCoordinator", "棋譜ファイルの読み込みに失敗しました: %1%2").arg(sourceName, detail);
        return result;
    }
    if (canceled(cancel)) return result;
    if (result.initialSfen.isEmpty()) result.initialSfen = SfenUtils::hirateSfen();
    if (record.mainline.baseSfen.isEmpty()) record.mainline.baseSfen = result.initialSfen;
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
    return parseContent(content, KifuFormats::detect(content), cancel);
}
