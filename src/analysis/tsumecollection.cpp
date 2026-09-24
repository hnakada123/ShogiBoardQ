#include "tsumecollection.h"
#include "position.h"
#include <QRegularExpression>

TsumeCollection::Result TsumeCollection::parse(const QString& text)
{
    Result result;
    const auto lines = text.split(QLatin1Char('\n'));
    static const QRegularExpression movePattern(QStringLiteral("^(?:[1-9][a-i][1-9][a-i]\\+?|[PLNSGBR]\\*[1-9][a-i])$"));
    for (qsizetype i = 0; i < lines.size(); ++i) {
        QString line = lines[i].trimmed();
        if (line.startsWith(QChar(0xfeff))) line.remove(0, 1);
        if (line.isEmpty() || line.startsWith(QLatin1Char('#'))) continue;
        auto fields = line.simplified().split(QLatin1Char(' '));
        if (fields.first() == QStringLiteral("position")) fields.removeFirst();
        if (!fields.isEmpty() && fields.first() == QStringLiteral("sfen")) fields.removeFirst();
        const int lineNumber = static_cast<int>(i + 1);
        if (fields.size() < 4) { result.invalidLines.append(lineNumber); continue; }
        TsumeProblem problem{fields.mid(0, 4).join(QLatin1Char(' ')), {}, lineNumber};
        shogi::Position position;
        bool valid = position.set_sfen(problem.sfen.toStdString(), true);
        if (valid) {
            const auto defender = shogi::opposite(position.side_to_move());
            valid = position.find_king(defender) >= 0 && !position.is_in_check(defender);
        }
        if (fields.size() > 4) {
            valid = valid && fields[4] == QStringLiteral("moves") && fields.size() > 5;
            problem.referenceMoves = fields.mid(5);
            for (const auto& move : std::as_const(problem.referenceMoves))
                valid = valid && movePattern.match(move).hasMatch();
        }
        if (valid) result.problems.append(problem);
        else result.invalidLines.append(lineNumber);
    }
    return result;
}
