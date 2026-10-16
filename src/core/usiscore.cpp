/// @file usiscore.cpp
/// @brief USI の評価値の実装

#include "usiscore.h"

QString UsiScore::mateText() const
{
    if (kind != Kind::Mate) return QString();
    if (!mateCountKnown) return mateWin ? QStringLiteral("+") : QStringLiteral("-");
    // "mate 0" は見ている側が詰んでいる。相手側から見た値（勝ち）は "+0"
    if (value == 0 && mateWin) return QStringLiteral("+0");
    return QString::number(value);
}

int UsiScore::mateSign() const
{
    if (kind != Kind::Mate) return 0;
    return mateWin ? 1 : -1;
}

UsiScore UsiScore::negated() const
{
    UsiScore s = *this;
    s.value = -value;
    if (bound == Bound::Lower) s.bound = Bound::Upper;
    else if (bound == Bound::Upper) s.bound = Bound::Lower;
    if (kind == Kind::Mate) s.mateWin = !mateWin;
    return s;
}

int UsiScore::chartValue() const
{
    switch (kind) {
    case Kind::Cp: return value;
    case Kind::Mate: return mateSign() * kMateValue;
    case Kind::None: break;
    }
    return 0;
}
