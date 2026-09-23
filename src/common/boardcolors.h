#ifndef BOARDCOLORS_H
#define BOARDCOLORS_H

#include <QColor>
#include <array>

/// 盤面の配色。初期値は従来の色を保持する。
struct BoardColors {
    QColor background{200, 190, 130};
    QColor board{228, 203, 115};
    QColor stand{228, 167, 46};
    QColor grid{80, 60, 30};
    QColor cardBackground{220, 229, 204};
    QColor cardBorder{214, 203, 181};
    QColor activeCardBorder{63, 98, 84};
    QColor turnBackground{63, 98, 84};
    QColor turnBorder{63, 98, 84};
    QColor turnText{255, 255, 255};
    QColor nameBackground{Qt::transparent};
    QColor nameBorder{Qt::transparent};
    QColor nameText{61, 50, 40};
    QColor clockBackground{Qt::transparent};
    QColor clockBorder{Qt::transparent};
    QColor clockText{61, 50, 40};
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
