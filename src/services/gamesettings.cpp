/// @file gamesettings.cpp
/// @brief 対局・棋譜の設定のうち、宣言の表で表せない項目（時間の換算・以前の版の値の引き継ぎ）

#include "gamesettings.h"

#include <iterator>

namespace GameSettings {

ListSetting<int> gameInfoColumnWidths()
{
    const ListSetting<int> widths{QStringLiteral("GameInfo/columnWidths"), "width"};
    // 以前の版は項目名の列幅だけを1つの値で保存していた
    const Setting<int> legacy{"GameInfo/keyColumnWidth", 0};
    if (legacy.isSaved()) {
        widths.set({qMax(0, legacy())});
        legacy.remove();
    }
    return widths;
}

} // namespace GameSettings

namespace GameSettings::StartGame {

namespace {

constexpr qint64 kSecondMs = 1000;

TimeControlSide timeOf(int hours, int minutes, int byoyomiSec, int incrementSec)
{
    TimeControlSide time;
    time.mainMs = (static_cast<qint64>(hours) * 3600 + minutes * 60) * kSecondMs;
    time.byoyomiMs = byoyomiSec * kSecondMs;
    time.incrementMs = incrementSec * kSecondMs;
    return time;
}

/// 以前の版が保存した開始局面の番号（0=現在の局面、1..14 はこの並び）の手合割。範囲外なら平手
const Handicaps::Handicap* legacyStartingPosition(int number)
{
    static constexpr const char16_t* kLegacyOrder[] = {
        u"平手", u"香落ち", u"右香落ち", u"角落ち", u"飛車落ち", u"飛香落ち", u"二枚落ち",
        u"三枚落ち", u"四枚落ち", u"五枚落ち", u"左五枚落ち", u"六枚落ち", u"八枚落ち", u"十枚落ち",
    };
    if (number == 0) return nullptr;
    const int index = number >= 1 && number <= static_cast<int>(std::size(kLegacyOrder)) ? number - 1 : 0;
    return Handicaps::byLabel(QStringView(kLegacyOrder[index]));
}

} // namespace

TimeControlSide Player::time() const
{
    return timeOf(basicTimeHour(), basicTimeMinutes(), byoyomiSec(), addEachMoveSec());
}

void Player::setTime(const TimeControlSide& time) const
{
    basicTimeHour.set(static_cast<int>(time.mainMs / (3600 * kSecondMs)));
    basicTimeMinutes.set(static_cast<int>(time.mainMs % (3600 * kSecondMs) / (60 * kSecondMs)));
    byoyomiSec.set(static_cast<int>(time.byoyomiMs / kSecondMs));
    addEachMoveSec.set(static_cast<int>(time.incrementMs / kSecondMs));
}

TimeControlSide Player::defaultTime() const
{
    return timeOf(basicTimeHour.defaultValue(), basicTimeMinutes.defaultValue(), byoyomiSec.defaultValue(),
                  addEachMoveSec.defaultValue());
}

const Handicaps::Handicap* startingPosition()
{
    if (startingPositionNumber.isSaved()) return legacyStartingPosition(startingPositionNumber());
    const QString name = startingPositionName();
    if (name.isEmpty()) return nullptr;
    const Handicaps::Handicap* handicap = Handicaps::byLabel(name);
    return handicap ? handicap : &Handicaps::kHirate;
}

void setStartingPosition(const Handicaps::Handicap* handicap)
{
    startingPositionName.set(handicap ? Handicaps::labelOf(*handicap) : QString());
    startingPositionNumber.remove();
}

} // namespace GameSettings::StartGame
