/// @file tsumecollectionwriter.cpp
/// @brief 詰将棋局面の一覧（局面集ファイル）の書き出しの実装

#include "tsumecollectionwriter.h"

#include <QSaveFile>
#include <QTextStream>

namespace TsumeCollectionWriter {

QString line(const QString& sfen, const QStringList& pv, bool includePv)
{
    if (!includePv || pv.isEmpty()) return sfen;
    return sfen + QStringLiteral(" moves ") + pv.join(QLatin1Char(' '));
}

bool save(const QString& filePath, const QStringList& header, const QStringList& lines,
          QString* errorMessage)
{
    QSaveFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }

    QTextStream out(&file);
    for (const QString& text : header) out << text << '\n';
    for (const QString& text : lines) {
        if (!text.isEmpty()) out << text << '\n';
    }
    out.flush();
    if (out.status() != QTextStream::Ok || !file.commit()) {
        if (errorMessage) *errorMessage = file.errorString();
        return false;
    }
    return true;
}

} // namespace TsumeCollectionWriter
