/// @file usiinfolineparser.cpp
/// @brief USI の info 行を構造化する軽量パーサの実装

#include "usiinfolineparser.h"

#include <QJsonArray>
#include <QRegularExpression>

namespace {

bool takeInt(const QStringList& tokens, qsizetype& i, int& out)
{
    if (i + 1 >= tokens.size()) return false;
    bool ok = false;
    const int v = tokens.at(i + 1).toInt(&ok);
    if (!ok) return false;
    out = v;
    ++i;
    return true;
}

bool takeLongLong(const QStringList& tokens, qsizetype& i, qint64& out)
{
    if (i + 1 >= tokens.size()) return false;
    bool ok = false;
    const qint64 v = tokens.at(i + 1).toLongLong(&ok);
    if (!ok) return false;
    out = v;
    ++i;
    return true;
}

} // namespace

QJsonObject UsiInfoLine::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("multipv")] = multipv;
    if (depth >= 0) obj[QStringLiteral("depth")] = depth;
    if (seldepth >= 0) obj[QStringLiteral("seldepth")] = seldepth;
    if (scoreCp) obj[QStringLiteral("score_cp")] = *scoreCp;
    if (scoreMate) obj[QStringLiteral("score_mate")] = *scoreMate;
    if (!bound.isEmpty()) obj[QStringLiteral("bound")] = bound;
    if (nodes >= 0) obj[QStringLiteral("nodes")] = static_cast<double>(nodes);
    if (nps >= 0) obj[QStringLiteral("nps")] = static_cast<double>(nps);
    if (timeMs >= 0) obj[QStringLiteral("time_ms")] = static_cast<double>(timeMs);
    if (hashfull >= 0) obj[QStringLiteral("hashfull")] = hashfull;
    if (!currmove.isEmpty()) obj[QStringLiteral("currmove")] = currmove;
    if (!pv.isEmpty()) obj[QStringLiteral("pv")] = QJsonArray::fromStringList(pv);
    if (!string.isEmpty()) obj[QStringLiteral("string")] = string;
    return obj;
}

UsiInfoLine UsiInfoLineParser::parse(const QString& line)
{
    UsiInfoLine info;
    static const QRegularExpression ws(QStringLiteral("\\s+"));
    const QStringList tokens = line.trimmed().split(ws, Qt::SkipEmptyParts);
    if (tokens.isEmpty() || tokens.first() != QLatin1String("info")) return info;

    for (qsizetype i = 1; i < tokens.size(); ++i) {
        const QString& key = tokens.at(i);
        if (key == QLatin1String("depth")) {
            takeInt(tokens, i, info.depth);
        } else if (key == QLatin1String("seldepth")) {
            takeInt(tokens, i, info.seldepth);
        } else if (key == QLatin1String("multipv")) {
            takeInt(tokens, i, info.multipv);
        } else if (key == QLatin1String("nodes")) {
            takeLongLong(tokens, i, info.nodes);
        } else if (key == QLatin1String("nps")) {
            takeLongLong(tokens, i, info.nps);
        } else if (key == QLatin1String("time")) {
            takeLongLong(tokens, i, info.timeMs);
        } else if (key == QLatin1String("hashfull")) {
            takeInt(tokens, i, info.hashfull);
        } else if (key == QLatin1String("currmove")) {
            if (i + 1 < tokens.size()) info.currmove = tokens.at(++i);
        } else if (key == QLatin1String("score")) {
            if (i + 2 < tokens.size()) {
                const QString kind = tokens.at(i + 1);
                const QString value = tokens.at(i + 2);
                bool ok = false;
                const int v = value.toInt(&ok);
                if (kind == QLatin1String("cp") && ok) {
                    info.scoreCp = v;
                } else if (kind == QLatin1String("mate")) {
                    // "mate +" / "mate -" のように手数を省くエンジンもある
                    if (ok) info.scoreMate = v;
                    else if (value == QLatin1String("+")) info.scoreMate = 0;
                    else if (value == QLatin1String("-")) info.scoreMate = 0;
                }
                i += 2;
                if (i + 1 < tokens.size()) {
                    if (tokens.at(i + 1) == QLatin1String("lowerbound")) { info.bound = QStringLiteral("lower"); ++i; }
                    else if (tokens.at(i + 1) == QLatin1String("upperbound")) { info.bound = QStringLiteral("upper"); ++i; }
                }
            }
        } else if (key == QLatin1String("pv")) {
            for (qsizetype j = i + 1; j < tokens.size(); ++j) info.pv.append(tokens.at(j));
            break;
        } else if (key == QLatin1String("string")) {
            QStringList rest;
            for (qsizetype j = i + 1; j < tokens.size(); ++j) rest.append(tokens.at(j));
            info.string = rest.join(QLatin1Char(' '));
            break;
        }
    }
    return info;
}
