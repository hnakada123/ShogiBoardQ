/// @file kifusavefile.cpp
/// @brief 棋譜の保存形式と文字コード、ファイルへの書き出し、既定のファイル名の実装

#include "kifusavefile.h"

#include "shiftjiscodec.h"

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>

namespace KifuSaveFile {

namespace {

// ファイル名の1つの名前の部分の長さの上限（文字数。2つの名前と日時で、ファイル名の上限 255 バイトに収まる）
constexpr int kMaxNamePartLength = 40;

QString tr(const char* text)
{
    return QCoreApplication::translate("KifuSaveFile", text);
}

// ファイル名に使えない/避けたい文字を置換し、空ならプレースホルダを返す。制御文字（NUL を含む）と
// 文字の向きを変える制御文字も置き換え、長さを切る（通信対局のサーバーが送る対局者名にも使う。セキュリティ監査 S21）
QString sanitizeForFileName(const QString& s, const QString& placeholder)
{
    QString t = s;
    if (t.isEmpty()) return placeholder;

    static const QRegularExpression badChars(QStringLiteral("[\\\\/:*?\"<>|\\x{0}-\\x{1F}\\x{7F}\\x{200E}\\x{200F}"
                                                            "\\x{202A}-\\x{202E}\\x{2066}-\\x{2069}]"));
    t.replace(badChars, QStringLiteral("_"));
    t = t.simplified();
    t.replace(QLatin1Char(' '), QLatin1Char('_'));
    if (t.size() > kMaxNamePartLength) {
        qsizetype end = kMaxNamePartLength;
        if (t.at(end - 1).isHighSurrogate()) --end;   // サロゲートペアを分けない
        t.truncate(end);
    }
    return t.isEmpty() ? placeholder : t;
}

/// ファイルに書く文（行を改行でつなぎ、末尾にも改行を付ける）
QString contentOf(const QStringList& lines)
{
    return lines.isEmpty() ? QString() : lines.join(QLatin1Char('\n')) + QLatin1Char('\n');
}

QString shiftJisError(const QString& missing)
{
    return tr("次の文字はShift_JISで保存できません：%1。UTF-8の形式（.kifu・.ki2u）で保存してください。").arg(missing);
}

/// 変換を済ませた内容 bytes を書く（フォルダが無ければ作る。途中で失敗しても前の内容を壊さない）
bool writeBytes(const QString& path, const QByteArray& bytes, QString* outError)
{
    const QFileInfo fi(path);
    if (!fi.absolutePath().isEmpty() && !QDir().mkpath(fi.absolutePath())) {
        if (outError) *outError = tr("Failed to create directory: %1").arg(fi.absolutePath());
        return false;
    }

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (outError) *outError = tr("Could not open the file for writing: %1").arg(file.errorString());
        return false;
    }
    if (file.write(bytes) != bytes.size()) {
        if (outError) *outError = tr("Failed to write data to file.");
        return false;
    }
    if (!file.commit()) {
        if (outError) *outError = tr("Failed to close file: %1").arg(file.errorString());
        return false;
    }
    return true;
}

bool checkPath(const QString& path, QString* outError)
{
    if (outError) outError->clear();
    if (!path.isEmpty()) return true;
    if (outError) *outError = tr("File path is empty.");
    return false;
}

} // namespace

KifuFormat saveFormatForPath(const QString& path)
{
    const auto target = KifuFormats::saveTargetForPath(path);
    return target ? target->format : KifuFormat::Kif;
}

KifuEncoding encodingForPath(const QString& path)
{
    const auto target = KifuFormats::saveTargetForPath(path);
    return target ? target->encoding : KifuEncoding::Utf8;
}

bool hasKnownSaveExtension(const QString& path)
{
    return KifuFormats::saveTargetForPath(path).has_value();
}

QString knownSaveExtensionsText()
{
    QStringList suffixes;
    for (const KifuFormats::SaveTarget& target : KifuFormats::saveTargets())
        suffixes.append(QLatin1Char('.') + target.suffix);
    return suffixes.join(QStringLiteral(", "));
}

bool write(const QString& path, const LineGenerator& generate, QString* outError, const Utf8Fallback& allowUtf8)
{
    if (!checkPath(path, outError)) return false;
    const KifuFormat format = saveFormatForPath(path);
    const KifuEncoding encoding = encodingForPath(path);
    const QString content = contentOf(generate(format, encoding));
    if (encoding == KifuEncoding::Utf8) return writeBytes(path, content.toUtf8(), outError);

    // 変換を済ませてから保存を始め、表せない文字による内容の欠けを防ぐ
    if (const auto sjis = ShiftJisCodec::encode(content)) return writeBytes(path, *sjis, outError);
    const QString missing = charactersNotInShiftJis(content);
    if (!allowUtf8) {
        if (outError) *outError = shiftJisError(missing);
        return false;
    }
    if (!allowUtf8(missing)) return false;
    // 先頭行の宣言も UTF-8 にして作り直す
    return writeBytes(path, contentOf(generate(format, KifuEncoding::Utf8)).toUtf8(), outError);
}

bool writeLines(const QString& path, const QStringList& lines, KifuEncoding encoding, QString* outError)
{
    if (!checkPath(path, outError)) return false;
    const QString content = contentOf(lines);
    if (encoding == KifuEncoding::Utf8) return writeBytes(path, content.toUtf8(), outError);
    const auto sjis = ShiftJisCodec::encode(content);
    if (!sjis) {
        if (outError) *outError = shiftJisError(charactersNotInShiftJis(content));
        return false;
    }
    return writeBytes(path, *sjis, outError);
}

QString makeDefaultSaveFileName(const QString& black, const QString& white, const QDateTime& now,
                                const QString& ext)
{
    const QDateTime ts = now.isValid() ? now : QDateTime::currentDateTime();
    const QString stamp = ts.toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString extension = ext.isEmpty() ? QStringLiteral("kifu") : ext;

    // 対局者の名前が無いときは、日時だけの名前にする
    if (black.trimmed().isEmpty() && white.trimmed().isEmpty())
        return QStringLiteral("%1_Session.%2").arg(stamp, extension);
    return QStringLiteral("%1_%2_vs_%3.%4")
        .arg(stamp, sanitizeForFileName(black, QStringLiteral("P1")),
             sanitizeForFileName(white, QStringLiteral("P2")), extension);
}

QString charactersNotInShiftJis(const QString& text, int maxCount)
{
    if (ShiftJisCodec::encode(text)) return QString();

    // 失敗したときだけ、文字（サロゲートペアは1文字として）ごとに確かめる。
    QString result;
    QStringList seen;
    for (qsizetype i = 0; i < text.size() && seen.size() < maxCount; ++i) {
        const qsizetype length = (text.at(i).isHighSurrogate() && i + 1 < text.size()
                                  && text.at(i + 1).isLowSurrogate()) ? 2 : 1;
        const QString ch = text.mid(i, length);
        i += length - 1;
        if (seen.contains(ch) || ShiftJisCodec::encode(ch)) continue;
        seen.append(ch);
        result += ch;
    }
    return result;
}

} // namespace KifuSaveFile
