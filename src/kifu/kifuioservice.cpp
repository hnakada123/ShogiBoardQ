/// @file kifuioservice.cpp
/// @brief 棋譜ファイルI/Oサービスの実装

#include "kifuioservice.h"
#include "shiftjiscodec.h"

#include <QSaveFile>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

namespace {

// ファイル名に使えない/避けたい文字を置換し、空ならプレースホルダを返す
QString sanitizeForFileName(const QString& s, const QString& placeholder)
{
    QString t = s;
    if (t.isEmpty()) return placeholder;

    static const QRegularExpression badChars(R"([\\/:*?"<>|\r\n\t])");
    t.replace(badChars, QStringLiteral("_"));
    t = t.simplified();
    t.replace(QLatin1Char(' '), QLatin1Char('_'));
    return t;
}

} // namespace

QString KifuIoService::makeDefaultSaveFileName(const MatchPlayers& players, const QDateTime& now, const QString& ext)
{
    const QDateTime ts = now.isValid() ? now : QDateTime::currentDateTime();
    const QString stamp = ts.toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString extension = ext.isEmpty() ? QStringLiteral("kifu") : ext;

    const QString black = players.name(Turn::Black);
    const QString white = players.name(Turn::White);
    // 対局をしていない（対局者の名前が無い）ときは、日時だけの名前にする
    if (black.isEmpty() && white.isEmpty()) return QStringLiteral("%1_Session.%2").arg(stamp, extension);
    return QStringLiteral("%1_%2_vs_%3.%4")
        .arg(stamp, sanitizeForFileName(black, QStringLiteral("P1")),
             sanitizeForFileName(white, QStringLiteral("P2")), extension);
}

QString KifuIoService::charactersNotInShiftJis(const QString& text, int maxCount)
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

bool KifuIoService::writeKifuFile(const QString& filePath,
                                  const QStringList& kifuLines,
                                  QString* errorText,
                                  bool useShiftJis)
{
    if (errorText) errorText->clear();
    if (filePath.isEmpty()) {
        if (errorText) *errorText = QObject::tr("File path is empty.");
        return false;
    }

    // 変換を完了してから保存を開始し、表現不能文字によるデータ損失を防ぐ。
    QByteArray encoded;
    const QString text = kifuLines.isEmpty() ? QString()
        : kifuLines.join(QLatin1Char('\n')) + QLatin1Char('\n');
    if (useShiftJis) {
        auto sjis = ShiftJisCodec::encode(text);
        if (!sjis) {
            if (errorText) *errorText = QObject::tr("Some characters cannot be saved in Shift_JIS. Please save as UTF-8.");
            return false;
        }
        encoded = std::move(*sjis);
    } else {
        encoded = text.toUtf8();
    }

    const QFileInfo fi(filePath);
    if (!fi.absolutePath().isEmpty()) {
        QDir dir;
        if (!dir.mkpath(fi.absolutePath())) {
            if (errorText) {
                *errorText = QObject::tr("Failed to create directory: %1").arg(fi.absolutePath());
            }
            return false;
        }
    }

    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorText) {
            *errorText = QObject::tr("Could not open the file for writing: %1").arg(file.errorString());
        }
        return false;
    }

    if (file.write(encoded) != encoded.size()) {
        if (errorText) *errorText = QObject::tr("Failed to write data to file.");
        return false;
    }

    if (!file.commit()) {
        if (errorText) *errorText = QObject::tr("Failed to close file: %1").arg(file.errorString());
        return false;
    }
    return true;
}
