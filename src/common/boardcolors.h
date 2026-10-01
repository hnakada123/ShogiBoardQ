#ifndef BOARDCOLORS_H
#define BOARDCOLORS_H

#include <QColor>
#include <array>

/// 盤面の配色。初期値は「榧と畳」。
struct BoardColors {
    QColor background{137, 146, 129};
    QColor board{224, 187, 121};
    QColor stand{168, 126, 82};
    QColor grid{84, 65, 39};
    QColor cardBackground{236, 237, 223};
    QColor cardBorder{166, 177, 153};
    QColor activeCardBorder{75, 101, 79};
    QColor turnBackground{75, 101, 79};
    QColor turnBorder{75, 101, 79};
    QColor turnText{255, 255, 255};
    QColor nameBackground{Qt::transparent};
    QColor nameBorder{Qt::transparent};
    QColor nameText{53, 64, 51};
    QColor clockBackground{Qt::transparent};
    QColor clockBorder{Qt::transparent};
    QColor clockText{53, 64, 51};
    QColor clockWarningText{135, 93, 33};
    QColor clockCriticalText{178, 59, 50};

    using Member = QColor BoardColors::*;
    static constexpr std::array<Member, 18> members()
    {
        return {{&BoardColors::background, &BoardColors::board, &BoardColors::stand, &BoardColors::grid,
                 &BoardColors::cardBackground, &BoardColors::cardBorder, &BoardColors::activeCardBorder,
                 &BoardColors::turnBackground, &BoardColors::turnBorder, &BoardColors::turnText,
                 &BoardColors::nameBackground, &BoardColors::nameBorder, &BoardColors::nameText,
                 &BoardColors::clockBackground, &BoardColors::clockBorder, &BoardColors::clockText,
                 &BoardColors::clockWarningText, &BoardColors::clockCriticalText}};
    }

    static bool supportsTransparency(Member member)
    {
        return member == &BoardColors::turnBackground || member == &BoardColors::turnBorder
            || member == &BoardColors::nameBackground || member == &BoardColors::nameBorder
            || member == &BoardColors::clockBackground || member == &BoardColors::clockBorder;
    }

    BoardColors normalized() const
    {
        const BoardColors defaults;
        BoardColors result = *this;
        for (const auto member : members()) {
            QColor& color = result.*member;
            if (!color.isValid()) color = defaults.*member;
            color = QColor(color.red(), color.green(), color.blue(),
                           supportsTransparency(member) ? color.alpha() : 255);
        }
        return result;
    }

    QColor backgroundText(const QColor& dark = QColor(40, 30, 20)) const
    {
        const int brightness = (background.red() * 299 + background.green() * 587
                                + background.blue() * 114) / 1000;
        return brightness < 128 ? QColor(Qt::white) : dark;
    }

    bool sameBoardPalette(const BoardColors& other) const
    {
        return background == other.background && board == other.board
            && stand == other.stand && grid == other.grid;
    }
    bool operator==(const BoardColors& other) const
    {
        for (const auto member : members()) {
            if (this->*member != other.*member) return false;
        }
        return true;
    }
    bool operator!=(const BoardColors& other) const { return !(*this == other); }
};

#endif // BOARDCOLORS_H
