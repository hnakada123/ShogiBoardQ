#include "kifuloadparser.h"
#include "csatosfenconverter.h"
#include "ki2tosfenconverter.h"
#include "jkftosfenconverter.h"
#include "usentosfenconverter.h"
#include "usitosfenconverter.h"
#include "kifreader.h"
#include "kiftextparser.h"
#include "kiftosfenconverter.h"
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
        result.error = QCoreApplication::translate("KifuLoader", "棋譜ファイルの読み込みに失敗しました: %1%2")
                           .arg(QFileInfo(path).fileName(), QLatin1Char('\n') + warning);
        return result;
    }
    KifuLoadResult result = parseContent(text, format, cancel, QFileInfo(path).fileName());
    result.filePath = path;
    if (!warning.isEmpty()) result.warning.prepend(warning);
    return result;
}

namespace {

using Parser = KifuParseOutcome (*)(QStringView, const ParseOptions&);

KifuParseOutcome parseBod(QStringView text, const ParseOptions& /*options*/)
{
    return KifTextParser::parseBod(text);
}

/// 形式ごとの読み方（どの形式も1回の解析で棋譜・開始局面・手合割の名前・対局情報・警告を返す）
Parser parserFor(KifuFormat format)
{
    switch (format) {
    case KifuFormat::Kif: return &KifToSfenConverter::parse;
    case KifuFormat::Ki2: return &Ki2ToSfenConverter::parse;
    case KifuFormat::Csa: return &CsaToSfenConverter::parse;
    case KifuFormat::Jkf: return &JkfToSfenConverter::parse;
    case KifuFormat::Usen: return &UsenToSfenConverter::parse;
    case KifuFormat::Usi:
    case KifuFormat::Sfen: return &UsiToSfenConverter::parse;
    case KifuFormat::Bod: return &parseBod;
    }
    return &KifToSfenConverter::parse;
}

} // namespace

KifuLoadResult KifuLoadParser::parseContent(const QString& content, KifuFormat format, const CancelFlag& cancel,
                                            const QString& sourceName)
{
    KifuLoadResult result;
    result.format = format;
    if (canceled(cancel)) return result;

    KifuParseOutcome outcome = parserFor(format)(content, ParseOptions{});
    result.success = outcome.success;
    result.startKind = outcome.startKind;
    result.positionOnly = outcome.positionOnly;
    result.warning = outcome.warning;
    result.gameInfo = outcome.gameInfo;
    result.record = std::move(outcome.record);
    if (!result.success) {
        const QString detail = result.warning.isEmpty() ? QString() : QLatin1Char('\n') + result.warning;
        if (format == KifuFormat::Bod) {
            result.error = QCoreApplication::translate("KifuLoader", "BOD形式の解析に失敗しました。%1").arg(result.warning);
        } else {
            result.error = sourceName.isEmpty()
                ? QCoreApplication::translate("KifuLoader", "棋譜の読み込みに失敗しました。%1").arg(detail)
                : QCoreApplication::translate("KifuLoader", "棋譜ファイルの読み込みに失敗しました: %1%2").arg(sourceName, detail);
        }
        return result;
    }
    if (canceled(cancel)) return result;
    if (result.record.mainline.startSfen.isEmpty()) result.record.mainline.startSfen = SfenUtils::hirateSfen();
    return result;
}

KifuLoadResult KifuLoadParser::parseText(const QString& content, const CancelFlag& cancel)
{
    KifuLoadResult result;
    if (canceled(cancel)) return result;
    if (content.trimmed().isEmpty()) {
        result.error = QCoreApplication::translate("KifuLoader", "貼り付けるテキストが空です。");
        return result;
    }
    return parseContent(content, KifuFormats::detect(content), cancel);
}
