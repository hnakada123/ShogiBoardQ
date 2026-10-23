/// @file positioncollectionfile.cpp
/// @brief 局面集ファイル（1行1局面）の読み取りの実装

#include "positioncollectionfile.h"

#include "sfenutils.h"
#include "sfenvalidationservice.h"
#include "usimove.h"


namespace PositionCollectionFile {

namespace {

bool isKeyword(const QString& token, QLatin1StringView keyword)
{
    return token.compare(keyword, Qt::CaseInsensitive) == 0;
}

} // namespace

QString Line::position() const
{
    if (!movesPlayed || moves.isEmpty()) return SfenValidationService::validate(startSfen).normalizedSfen;
    return SfenValidationService::applyUsiMoves(startSfen, moves);
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
    if (!SfenValidationService::validate(result.startSfen).valid) return LineKind::Invalid;

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

} // namespace PositionCollectionFile
