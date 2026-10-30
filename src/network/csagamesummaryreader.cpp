/// @file csagamesummaryreader.cpp
/// @brief CSA の対局条件（Game_Summary）の読み取りの実装

#include "csagamesummaryreader.h"
#include "logcategories.h"

namespace {
bool parseNonNegativeIntField(const QString& key, const QString& value, int& out)
{
    bool ok = false;
    const int parsed = value.toInt(&ok);
    if (!ok || parsed < 0) {
        qCWarning(lcNetwork) << "Invalid numeric value for" << key << ":" << value;
        return false;
    }
    out = parsed;
    return true;
}

/// Your_Turn・To_Move の値（+ / -）
std::optional<Turn> parseSideSymbol(const QString& value)
{
    if (value == QStringLiteral("+")) return Turn::Black;
    if (value == QStringLiteral("-")) return Turn::White;
    return std::nullopt;
}
} // namespace

CsaGameSummaryReader::Status CsaGameSummaryReader::read(const QString& line)
{
    if (m_status != Status::Reading) return m_status;
    m_bytes += line.toUtf8().size() + 1;
    if (++m_lines > kMaxLines || m_bytes > kMaxBytes) return m_status = Status::TooLarge;
    if (line == QStringLiteral("END Game_Summary")) return m_status = Status::Finished;
    if (readSectionBoundary(line)) return m_status;
    switch (m_section) {
    case Section::Time:
    case Section::TimeBlack:
    case Section::TimeWhite:
        readTimeLine(line);
        break;
    case Section::Position:
        readPositionLine(line);
        break;
    case Section::TopLevel:
        readTopLevelLine(line);
        break;
    }
    return m_status;
}

bool CsaGameSummaryReader::readSectionBoundary(const QString& line)
{
    if (line == QStringLiteral("END Time") ||
        line == QStringLiteral("END Time+") ||
        line == QStringLiteral("END Time-") ||
        line == QStringLiteral("END Position")) {
        m_section = Section::TopLevel;
        return true;
    }

    if (line == QStringLiteral("BEGIN Time")) {
        m_section = Section::Time;
        m_summary.timeBlack.specified = true;
        m_summary.timeWhite.specified = true;
        return true;
    }
    if (line == QStringLiteral("BEGIN Time+")) {
        m_section = Section::TimeBlack;
        m_summary.hasIndividualTime = true;
        m_summary.timeBlack.specified = true;
        return true;
    }
    if (line == QStringLiteral("BEGIN Time-")) {
        m_section = Section::TimeWhite;
        m_summary.hasIndividualTime = true;
        m_summary.timeWhite.specified = true;
        return true;
    }
    if (line == QStringLiteral("BEGIN Position")) {
        m_section = Section::Position;
        return true;
    }

    return false;
}

void CsaGameSummaryReader::readTimeLine(const QString& line)
{
    const qsizetype colonPos = line.indexOf(QLatin1Char(':'));
    if (colonPos <= 0) return;

    const QString key = line.left(static_cast<int>(colonPos));
    const QString value = line.mid(static_cast<int>(colonPos) + 1);

    // 共通の Time は双方に、Time+ は先手に、Time- は後手に書く
    const bool toBlack = m_section != Section::TimeWhite;
    const bool toWhite = m_section != Section::TimeBlack;
    const auto apply = [&](auto&& assign) {
        if (toBlack) assign(m_summary.timeBlack);
        if (toWhite) assign(m_summary.timeWhite);
    };

    using Time = CsaGameSummary::Time;
    if (key == QStringLiteral("Time_Unit")) {
        apply([&](Time& t) { t.timeUnit = value; });
    } else if (key == QStringLiteral("Time_Roundup")) {
        apply([&](Time& t) { t.timeRoundup = (value == QStringLiteral("YES")); });
    } else {
        int number = 0;
        if (key == QStringLiteral("Total_Time")) {
            if (parseNonNegativeIntField(key, value, number)) apply([&](Time& t) { t.totalTime = number; });
        } else if (key == QStringLiteral("Byoyomi")) {
            if (parseNonNegativeIntField(key, value, number)) apply([&](Time& t) { t.byoyomi = number; });
        } else if (key == QStringLiteral("Least_Time_Per_Move")) {
            if (parseNonNegativeIntField(key, value, number)) apply([&](Time& t) { t.leastTimePerMove = number; });
        } else if (key == QStringLiteral("Increment")) {
            if (parseNonNegativeIntField(key, value, number)) apply([&](Time& t) { t.increment = number; });
        } else if (key == QStringLiteral("Delay")) {
            if (parseNonNegativeIntField(key, value, number)) apply([&](Time& t) { t.delay = number; });
        }
    }
}

void CsaGameSummaryReader::readPositionLine(const QString& line)
{
    // P1-P9, P+, P-, +, - で始まる行は局面情報
    if (line.startsWith(QLatin1Char('P')) ||
        line == QStringLiteral("+") ||
        line == QStringLiteral("-")) {
        m_summary.positionLines.append(line);
        return;
    }

    // 再開局面の指し手行（+7776FU,T12 形式）。消費時間は持時間の計算に使う
    if ((line.startsWith(QLatin1Char('+')) || line.startsWith(QLatin1Char('-'))) &&
        line.length() > 1) {
        const qsizetype commaPos = line.indexOf(QLatin1Char(','));
        m_summary.moves.append(commaPos > 0 ? line.left(static_cast<int>(commaPos)) : line);
        m_summary.moveConsumedTimes.append(CsaGameSummary::consumedUnits(line));
    }
}

void CsaGameSummaryReader::readTopLevelLine(const QString& line)
{
    const qsizetype colonPos = line.indexOf(QLatin1Char(':'));
    if (colonPos <= 0) return;

    const QString key = line.left(static_cast<int>(colonPos));
    const QString value = line.mid(static_cast<int>(colonPos) + 1);

    if (key == QStringLiteral("Protocol_Version")) {
        m_summary.protocolVersion = value;
    } else if (key == QStringLiteral("Protocol_Mode")) {
        m_summary.protocolMode = value;
    } else if (key == QStringLiteral("Format")) {
        m_summary.format = value;
    } else if (key == QStringLiteral("Declaration")) {
        m_summary.declaration = value;
    } else if (key == QStringLiteral("Game_ID")) {
        m_summary.gameId = value;
    } else if (key == QStringLiteral("Name+")) {
        m_summary.blackName = value;
    } else if (key == QStringLiteral("Name-")) {
        m_summary.whiteName = value;
    } else if (key == QStringLiteral("Your_Turn")) {
        m_summary.myTurn = parseSideSymbol(value);
        if (!m_summary.myTurn) qCWarning(lcNetwork) << "Invalid Your_Turn:" << value;
    } else if (key == QStringLiteral("To_Move")) {
        if (const auto side = parseSideSymbol(value)) m_summary.toMove = *side;
        else qCWarning(lcNetwork) << "Invalid To_Move:" << value;
    } else if (key == QStringLiteral("Rematch_On_Draw")) {
        m_summary.rematchOnDraw = (value == QStringLiteral("YES"));
    } else if (key == QStringLiteral("Max_Moves")) {
        (void)parseNonNegativeIntField(key, value, m_summary.maxMoves);
    }
}
