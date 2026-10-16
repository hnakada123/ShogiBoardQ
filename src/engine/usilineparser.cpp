/// @file usilineparser.cpp
/// @brief USI エンジンからの受信行の解析

#include "usilineparser.h"

#include "enginesettingsconstants.h"

#include <QJsonArray>
#include <QStringView>

// ============================================================
// UsiInfo
// ============================================================

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
            score.mateWin = v > 0 || (v == 0 && value.startsWith(u'+'));
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

std::optional<UsiInfo> UsiLineParser::parseInfo(const QString& line)
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

namespace {

/// 空白で区切った語（連続する空白は1つとみなす）
QStringList words(const QString& line)
{
    return line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

/// その種類の option に書ける属性か
bool isOptionAttribute(const QString& type, const QString& key)
{
    using namespace EngineSettingsConstants;
    if (type == QLatin1String(OptionTypeSpin))
        return key == u"default" || key == u"min" || key == u"max";
    if (type == QLatin1String(OptionTypeCombo)) return key == u"default" || key == u"var";
    if (type == QLatin1String(OptionTypeButton)) return false;
    if (type == QLatin1String(OptionTypeCheck) || type == QLatin1String(OptionTypeString)
        || type == QLatin1String(OptionTypeFilename))
        return key == u"default";
    return key == u"default" || key == u"min" || key == u"max" || key == u"var";
}

std::optional<QString> parseIdField(const QString& line, QLatin1String field)
{
    const QStringList tokens = words(line);
    if (tokens.size() < 3 || tokens.at(0) != u"id" || tokens.at(1) != field) return std::nullopt;
    return tokens.mid(2).join(QLatin1Char(' '));
}

} // namespace

std::optional<UsiOptionDeclaration> UsiLineParser::parseOption(const QString& line, QString* error)
{
    const auto fail = [error](const QString& message) -> std::optional<UsiOptionDeclaration> {
        if (error) *error = message;
        return std::nullopt;
    };
    const QStringList tokens = words(line);
    if (tokens.size() < 4) return fail(QStringLiteral("Too few tokens"));
    if (tokens.at(0) != u"option" || tokens.at(1) != u"name")
        return fail(QStringLiteral("Command must start with 'option name'"));
    const qsizetype typeIndex = tokens.indexOf(QStringLiteral("type"), 2);
    if (typeIndex <= 2 || typeIndex + 1 >= tokens.size()) return fail(QStringLiteral("Missing 'type' section"));

    UsiOptionDeclaration option;
    option.name = tokens.mid(2, typeIndex - 2).join(QLatin1Char(' '));
    option.type = tokens.at(typeIndex + 1);

    // string / filename は "default" より後ろをそのまま値にする（空白を含むパスなど）
    using namespace EngineSettingsConstants;
    if (option.type == QLatin1String(OptionTypeString) || option.type == QLatin1String(OptionTypeFilename)) {
        const qsizetype defaultIndex = tokens.indexOf(QStringLiteral("default"), typeIndex + 2);
        if (defaultIndex >= 0) option.defaultValue = tokens.mid(defaultIndex + 1).join(QLatin1Char(' '));
        return option;
    }

    // 属性ごとに、次の属性の語までを値にする。種類に合わない語より後ろは読まない
    for (qsizetype i = typeIndex + 2; i < tokens.size();) {
        const QString key = tokens.at(i);
        if (!isOptionAttribute(option.type, key)) break;
        qsizetype j = i + 1;
        while (j < tokens.size() && !isOptionAttribute(option.type, tokens.at(j))) ++j;
        const QString value = tokens.mid(i + 1, j - (i + 1)).join(QLatin1Char(' '));
        if (value.isEmpty()) return fail(QStringLiteral("%1 value is missing").arg(key));
        if (key == u"default") option.defaultValue = value;
        else if (key == u"min") option.minValue = value;
        else if (key == u"max") option.maxValue = value;
        else option.comboValues.append(value);
        i = j;
    }
    return option;
}

std::optional<QString> UsiLineParser::parseIdName(const QString& line)
{
    return parseIdField(line, QLatin1String("name"));
}

std::optional<QString> UsiLineParser::parseIdAuthor(const QString& line)
{
    return parseIdField(line, QLatin1String("author"));
}

bool UsiLineParser::isBestMoveLine(const QString& line)
{
    const QStringList tokens = words(line);
    return !tokens.isEmpty() && tokens.first() == u"bestmove";
}

std::optional<UsiBestMove> UsiLineParser::parseBestMove(const QString& line)
{
    const QStringList tokens = words(line);
    if (tokens.size() < 2 || tokens.at(0) != u"bestmove") return std::nullopt;
    UsiBestMove result;
    result.move = tokens.at(1);
    result.special = parseSpecialMove(result.move);
    const qsizetype ponderIndex = tokens.indexOf(QStringLiteral("ponder"), 2);
    if (ponderIndex >= 0 && ponderIndex + 1 < tokens.size()) result.ponder = tokens.at(ponderIndex + 1);
    return result;
}

std::optional<UsiCheckmate> UsiLineParser::parseCheckmate(const QString& line)
{
    const QStringList tokens = words(line);
    if (tokens.isEmpty() || tokens.at(0) != u"checkmate") return std::nullopt;
    UsiCheckmate result;
    const QString first = tokens.size() > 1 ? tokens.at(1) : QString();
    if (first.compare(QStringLiteral("nomate"), Qt::CaseInsensitive) == 0) {
        result.result = UsiCheckmate::Result::NoMate;
    } else if (first.compare(QStringLiteral("notimplemented"), Qt::CaseInsensitive) == 0) {
        result.result = UsiCheckmate::Result::NotImplemented;
    } else if (first.isEmpty() || first.compare(QStringLiteral("unknown"), Qt::CaseInsensitive) == 0
               || first.compare(QStringLiteral("timeout"), Qt::CaseInsensitive) == 0) {
        // "checkmate timeout"（時間切れ）は指し手列ではないので結果不明として扱う
        result.result = UsiCheckmate::Result::Unknown;
    } else {
        result.result = UsiCheckmate::Result::Mate;
        result.pv = tokens.mid(1);
    }
    return result;
}
