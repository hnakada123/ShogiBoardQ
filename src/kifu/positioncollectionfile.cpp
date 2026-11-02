/// @file positioncollectionfile.cpp
/// @brief 局面集ファイル（1行1局面）の読み取りの実装

#include "positioncollectionfile.h"

#include "boundedfile.h"
#include "sfenutils.h"
#include "sfenvalidator.h"
#include "usimove.h"

#include <QCoreApplication>
#include <QFileInfo>

namespace PositionCollectionFile {

namespace {

bool isKeyword(const QString& token, QLatin1StringView keyword)
{
    return token.compare(keyword, Qt::CaseInsensitive) == 0;
}

} // namespace

QString Line::position() const
{
    if (!movesPlayed || moves.isEmpty()) return SfenValidator::validate(startSfen).normalizedSfen;
    return SfenValidator::applyUsiMoves(startSfen, moves);
}

LineKind parseLine(const QString& text, int lineNumber, Line* line)
{
    QString trimmed = text.trimmed();
    if (trimmed.startsWith(QChar(0xfeff))) trimmed = trimmed.mid(1).trimmed();
    if (trimmed.isEmpty() || trimmed.startsWith(QLatin1Char('#'))) return LineKind::Skipped;

    QStringList tokens = trimmed.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    Line result;
    result.lineNumber = lineNumber;
    if (!tokens.isEmpty() && isKeyword(tokens.first(), QLatin1StringView("position"))) {
        tokens.removeFirst();
        result.movesPlayed = true;
    }

    qsizetype next = 0;
    if (!tokens.isEmpty() && isKeyword(tokens.first(), QLatin1StringView("startpos"))) {
        result.startSfen = SfenUtils::hirateSfen();
        result.movesPlayed = true;
        next = 1;
    } else {
        if (!tokens.isEmpty() && isKeyword(tokens.first(), QLatin1StringView("sfen"))) tokens.removeFirst();
        if (tokens.size() < 4) return LineKind::Invalid;
        result.startSfen = tokens.first(4).join(QLatin1Char(' '));
        next = 4;
    }

    if (next < tokens.size()) {
        if (!isKeyword(tokens.at(next), QLatin1StringView("moves"))) return LineKind::Invalid;
        result.moves = tokens.mid(next + 1);
        for (const QString& move : std::as_const(result.moves))
            if (!UsiMove::isExact(move)) return LineKind::Invalid;
    }
    if (!SfenValidator::validate(result.startSfen).valid) return LineKind::Invalid;

    if (line) *line = std::move(result);
    return LineKind::Position;
}

Contents parse(const QString& text)
{
    Contents contents;
    const QStringList lines = text.split(QLatin1Char('\n'));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        const int lineNumber = static_cast<int>(i + 1);
        Line line;
        switch (parseLine(lines.at(i), lineNumber, &line)) {
        case LineKind::Skipped: break;
        case LineKind::Position: contents.lines.append(std::move(line)); break;
        case LineKind::Invalid: contents.invalidLines.append(lineNumber); break;
        }
    }
    return contents;
}

File readFile(const QString& path)
{
    File file;
    const qint64 limit = BoundedFile::kMaxCollectionBytes;
    const QString name = QFileInfo(path).fileName();
    switch (BoundedFile::read(path, limit, file.bytes)) {
    case BoundedFile::Result::Ok:
        file.ok = true;
        file.text = QString::fromUtf8(file.bytes);
        break;
    case BoundedFile::Result::OpenFailed:
        file.error = QCoreApplication::translate("PositionCollectionFile", "ファイルを開けませんでした: %1").arg(name);
        break;
    case BoundedFile::Result::NotRegularFile:
        file.error = QCoreApplication::translate("PositionCollectionFile", "通常のファイルではありません: %1").arg(name);
        break;
    case BoundedFile::Result::TooLarge:
        file.error = QCoreApplication::translate("PositionCollectionFile", "ファイルが大きすぎます（%1 MB まで）: %2")
                         .arg(limit / (1024 * 1024))
                         .arg(name);
        break;
    }
    return file;
}

QString fileFilter()
{
    return QCoreApplication::translate("PositionCollectionFile", "局面集 (*.txt *.sfen *.usi);;すべてのファイル (*)");
}

} // namespace PositionCollectionFile
