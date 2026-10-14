/// @file usiinfoparser.cpp
/// @brief USI エンジンからの info 行・option 行・id 行の解析

#include "usiinfoparser.h"

#include <QJsonArray>
#include <QStringView>

// ============================================================
// UsiScore / UsiInfo
// ============================================================

QString UsiScore::mateText() const
{
    if (kind != Kind::Mate) return QString();
    if (!mateCountKnown) return mateWin ? QStringLiteral("+") : QStringLiteral("-");
    return QString::number(value);
}

int UsiScore::mateSign() const
{
    if (kind != Kind::Mate) return 0;
    if (value > 0) return 1;
    if (value < 0) return -1;
    if (mateCountKnown) return 0;
    return mateWin ? 1 : -1;
}

QJsonObject UsiInfo::toJson() const
{
    QJsonObject obj;
    obj[QStringLiteral("multipv")] = multipv;
    if (depth >= 0) obj[QStringLiteral("depth")] = depth;
    if (seldepth >= 0) obj[QStringLiteral("seldepth")] = seldepth;
    if (score.isCp()) obj[QStringLiteral("score_cp")] = score.value;
    if (score.isMate()) obj[QStringLiteral("score_mate")] = score.value;
    if (score.bound == UsiScore::Bound::Lower) obj[QStringLiteral("bound")] = QStringLiteral("lower");
    if (score.bound == UsiScore::Bound::Upper) obj[QStringLiteral("bound")] = QStringLiteral("upper");
    if (nodes >= 0) obj[QStringLiteral("nodes")] = static_cast<double>(nodes);
    if (nps >= 0) obj[QStringLiteral("nps")] = static_cast<double>(nps);
    if (timeMs >= 0) obj[QStringLiteral("time_ms")] = static_cast<double>(timeMs);
    if (hashfull >= 0) obj[QStringLiteral("hashfull")] = hashfull;
    if (!currmove.isEmpty()) obj[QStringLiteral("currmove")] = currmove;
    if (!pv.isEmpty()) obj[QStringLiteral("pv")] = QJsonArray::fromStringList(pv);
    if (!string.isEmpty()) obj[QStringLiteral("string")] = string;
    return obj;
}

// ============================================================
// 解析
// ============================================================

namespace {

/// tokens[i+1] を整数として out に入れ、i を進める。数でなければ値を変えずに語だけ飛ばす
void takeInt(const QList<QStringView>& tokens, qsizetype& i, int& out)
{
    if (i + 1 >= tokens.size()) return;
    bool ok = false;
    const int v = tokens.at(i + 1).toInt(&ok);
    if (ok) out = v;
    ++i;
}

void takeLongLong(const QList<QStringView>& tokens, qsizetype& i, qint64& out)
{
    if (i + 1 >= tokens.size()) return;
    bool ok = false;
    const qint64 v = tokens.at(i + 1).toLongLong(&ok);
    if (ok) out = v;
    ++i;
}

/// "score cp <v> [lowerbound|upperbound]" / "score mate <v|+|-> [bound]" を読む。i は score の位置
void takeScore(const QList<QStringView>& tokens, qsizetype& i, UsiScore& out)
{
    if (i + 2 >= tokens.size()) { i = tokens.size(); return; }
    const QStringView kind = tokens.at(i + 1);
    const QStringView value = tokens.at(i + 2);
    bool ok = false;
    const int v = value.toInt(&ok);

    UsiScore score;
    if (kind == u"cp" && ok) {
        score.kind = UsiScore::Kind::Cp;
        score.value = v;
    } else if (kind == u"mate") {
        if (ok) {
            score.kind = UsiScore::Kind::Mate;
            score.value = v;
            score.mateCountKnown = true;
            score.mateWin = v > 0;
        } else if (value == u"+" || value == u"-") {
            // 手数を省くエンジンもある
            score.kind = UsiScore::Kind::Mate;
            score.mateWin = value == u"+";
        }
    }
    if (score.kind == UsiScore::Kind::None) {
        // 読めない score は飛ばす（"score cp" だけなら kind の語まで）
        i += 1;
        return;
    }
    i += 2;
    if (i + 1 < tokens.size()) {
        if (tokens.at(i + 1) == u"lowerbound") { score.bound = UsiScore::Bound::Lower; ++i; }
        else if (tokens.at(i + 1) == u"upperbound") { score.bound = UsiScore::Bound::Upper; ++i; }
    }
    out = score;
}

QStringList toStringList(const QList<QStringView>& tokens, qsizetype first)
{
    QStringList list;
    list.reserve(tokens.size() - first);
    for (qsizetype j = first; j < tokens.size(); ++j) list.append(tokens.at(j).toString());
    return list;
}

} // namespace

std::optional<UsiInfo> UsiInfoParser::parse(const QString& line)
{
    // 改行・タブ・連続する空白をまとめ、元の行を指すだけの区切りにする（読み筋の手ごとに文字列を作らない）
    const QString normalized = line.simplified();
    const QList<QStringView> tokens = QStringView(normalized).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (tokens.isEmpty() || tokens.first() != u"info") return std::nullopt;

    UsiInfo info;
    for (qsizetype i = 1; i < tokens.size(); ++i) {
        const QStringView key = tokens.at(i);
        if (key == u"depth") {
            takeInt(tokens, i, info.depth);
        } else if (key == u"seldepth") {
            takeInt(tokens, i, info.seldepth);
        } else if (key == u"multipv") {
            takeInt(tokens, i, info.multipv);
        } else if (key == u"nodes") {
            takeLongLong(tokens, i, info.nodes);
        } else if (key == u"nps") {
            takeLongLong(tokens, i, info.nps);
        } else if (key == u"time") {
            takeLongLong(tokens, i, info.timeMs);
        } else if (key == u"hashfull") {
            takeInt(tokens, i, info.hashfull);
        } else if (key == u"currmove") {
            if (i + 1 < tokens.size()) info.currmove = tokens.at(++i).toString();
        } else if (key == u"score") {
            takeScore(tokens, i, info.score);
        } else if (key == u"pv") {
            // pv より後ろはすべて読み筋
            info.pv = toStringList(tokens, i + 1);
            break;
        } else if (key == u"string") {
            // string より後ろは行末までが本文（中に pv などの語があっても読まない）
            info.string = toStringList(tokens, i + 1).join(QLatin1Char(' '));
            break;
        }
        // currmovenumber など知らない語は飛ばす
    }
    return info;
}

std::optional<UsiOptionDeclaration> UsiInfoParser::parseOptionDeclaration(const QString& line)
{
    static const QString prefix = QStringLiteral("option name ");
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(prefix)) return std::nullopt;

    // 形式: "option name <name> type <type> [default ...]"。name には空白が入ることもある
    const qsizetype typePos = trimmed.indexOf(QStringLiteral(" type "), prefix.size());
    if (typePos <= prefix.size()) return std::nullopt;

    UsiOptionDeclaration option;
    option.name = trimmed.mid(prefix.size(), typePos - prefix.size()).trimmed();
    option.type = trimmed.mid(typePos + 6).section(QLatin1Char(' '), 0, 0);
    if (option.name.isEmpty() || option.type.isEmpty()) return std::nullopt;
    return option;
}

std::optional<QString> UsiInfoParser::parseIdName(const QString& line)
{
    static const QString prefix = QStringLiteral("id name ");
    const QString trimmed = line.trimmed();
    if (!trimmed.startsWith(prefix)) return std::nullopt;
    const QString name = trimmed.mid(prefix.size()).trimmed();
    if (name.isEmpty()) return std::nullopt;
    return name;
}
