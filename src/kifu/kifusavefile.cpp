/// @file kifusavefile.cpp
/// @brief 棋譜の保存形式（拡張子）と、ファイルへの書き出しの実装

#include "kifusavefile.h"

#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <optional>

#include "kifuioservice.h"

namespace KifuSaveFile {

namespace {

/// 拡張子に対応する保存形式。保存できない拡張子は nullopt
std::optional<KifuFormat> formatForExtension(const QString& path)
{
    const auto format = KifuFormats::fromPath(path);
    if (format && !KifuFormats::isSavable(*format)) return std::nullopt;
    return format;
}

/// Shift_JIS で書き出す場合、先頭行の encoding 宣言を差し替えた行リストを返す
QStringList withShiftJisHeader(const QStringList& lines)
{
    QStringList result = lines;
    if (!result.isEmpty() && result.first().contains(QStringLiteral("encoding=UTF-8"))) {
        result[0].replace(QStringLiteral("encoding=UTF-8"), QStringLiteral("encoding=Shift_JIS"));
    }
    return result;
}

} // namespace

KifuFormat saveFormatForPath(const QString& path)
{
    return formatForExtension(path).value_or(KifuFormat::Kif);
}

QString pathForSelectedFilter(const QString& path, const QString& selectedFilter)
{
    if (path.isEmpty() || !QFileInfo(path).suffix().isEmpty()) return path;
    static const QRegularExpression extensionRe(QStringLiteral("\\(\\*\\.([a-z0-9]+)\\)"));
    const auto match = extensionRe.match(selectedFilter);
    const QString extension = match.hasMatch() ? match.captured(1) : QStringLiteral("kifu");
    return path + QLatin1Char('.') + extension;
}

bool hasKnownSaveExtension(const QString& path)
{
    return formatForExtension(path).has_value();
}

bool usesShiftJisForPath(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    return ext == QStringLiteral("kif") || ext == QStringLiteral("ki2");
}

bool write(const QString& path, const QStringList& lines, QString* outError, const Utf8Fallback& allowUtf8)
{
    if (outError) outError->clear();
    bool shiftJis = usesShiftJisForPath(path);
    if (shiftJis) {
        const QString missing = KifuIoService::charactersNotInShiftJis(lines.join(QLatin1Char('\n')));
        if (!missing.isEmpty()) {
            if (!allowUtf8) {
                if (outError) {
                    *outError = QObject::tr("次の文字はShift_JISで保存できません：%1。"
                                            "UTF-8の形式（.kifu・.ki2u）で保存してください。").arg(missing);
                }
                return false;
            }
            if (!allowUtf8(missing)) return false;
            shiftJis = false;  // 先頭行の encoding=UTF-8 宣言はそのまま残す
        }
    }
    QString err;
    const bool ok = KifuIoService::writeKifuFile(path, shiftJis ? withShiftJisHeader(lines) : lines, &err, shiftJis);
    if (!ok && outError) *outError = err;
    return ok;
}

} // namespace KifuSaveFile
